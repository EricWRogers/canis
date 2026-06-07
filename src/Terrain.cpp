#include <Canis/Terrain.hpp>

#include <Canis/AssetManager.hpp>

#include <algorithm>
#include <cmath>

namespace Canis
{
    namespace
    {
        bool SampleClampedTerrainHeight(
            const TerrainAsset &_terrain,
            float _localX,
            float _localZ,
            float &_height)
        {
            const int width = _terrain.GetSampleWidth();
            const int depth = _terrain.GetSampleDepth();
            if (width < 2 || depth < 2)
                return false;

            const float minX = -static_cast<float>(width - 1) * _terrain.cellSize * 0.5f;
            const float minZ = -static_cast<float>(depth - 1) * _terrain.cellSize * 0.5f;
            const float maxX = minX + static_cast<float>(width - 1) * _terrain.cellSize;
            const float maxZ = minZ + static_cast<float>(depth - 1) * _terrain.cellSize;
            return _terrain.SampleHeightBilinear(
                std::clamp(_localX, minX, maxX),
                std::clamp(_localZ, minZ, maxZ),
                _height);
        }

        bool TrySampleNeighborTerrainHeight(
            Entity &_entity,
            const TerrainAsset &_terrain,
            const Matrix4 &_sourceModel,
            const Matrix4 &_sourceInverseModel,
            float _localX,
            float _localZ,
            float &_height)
        {
            const Vector3 worldFlatPoint = Vector3(_sourceModel * Vector4(_localX, 0.0f, _localZ, 1.0f));

            for (Entity *candidate : _entity.scene.GetEntities())
            {
                if (candidate == nullptr || candidate == &_entity ||
                    !candidate->HasComponent<Terrain>() || !candidate->HasComponent<Transform>())
                    continue;

                const std::string terrainPath = AssetManager::ResolvePath(candidate->GetComponent<Terrain>().terrain);
                if (terrainPath.empty())
                    continue;

                TerrainAsset *neighborTerrain = AssetManager::GetTerrain(terrainPath);
                if (neighborTerrain == nullptr || neighborTerrain == &_terrain)
                    continue;

                const Transform &neighborTransform = candidate->GetComponent<Transform>();
                const Matrix4 neighborModel = neighborTransform.GetModelMatrix();
                const Matrix4 neighborInverseModel = glm::inverse(neighborModel);
                const Vector3 neighborLocal = Vector3(neighborInverseModel * Vector4(worldFlatPoint, 1.0f));

                float neighborHeight = 0.0f;
                if (!neighborTerrain->SampleHeightBilinear(neighborLocal.x, neighborLocal.z, neighborHeight))
                    continue;

                const Vector3 neighborWorldHeight = Vector3(neighborModel * Vector4(neighborLocal.x, neighborHeight, neighborLocal.z, 1.0f));
                const Vector3 sourceLocalHeight = Vector3(_sourceInverseModel * Vector4(neighborWorldHeight, 1.0f));
                _height = sourceLocalHeight.y;
                return true;
            }

            return false;
        }

        float SampleTerrainHeightForNormal(
            Entity &_entity,
            const TerrainAsset &_terrain,
            const Matrix4 &_sourceModel,
            const Matrix4 &_sourceInverseModel,
            float _localX,
            float _localZ)
        {
            float height = 0.0f;
            if (_terrain.SampleHeightBilinear(_localX, _localZ, height))
                return height;

            if (TrySampleNeighborTerrainHeight(
                    _entity,
                    _terrain,
                    _sourceModel,
                    _sourceInverseModel,
                    _localX,
                    _localZ,
                    height))
                return height;

            if (SampleClampedTerrainHeight(_terrain, _localX, _localZ, height))
                return height;

            return 0.0f;
        }

        Vector3 CalculateTerrainNormal(
            Entity &_entity,
            const TerrainAsset &_terrain,
            const Matrix4 &_sourceModel,
            const Matrix4 &_sourceInverseModel,
            int _x,
            int _z)
        {
            const Vector3 localPosition = _terrain.GetLocalPosition(_x, _z);
            const float cellSize = std::max(_terrain.cellSize, 0.01f);
            const float left = SampleTerrainHeightForNormal(_entity, _terrain, _sourceModel, _sourceInverseModel, localPosition.x - cellSize, localPosition.z);
            const float right = SampleTerrainHeightForNormal(_entity, _terrain, _sourceModel, _sourceInverseModel, localPosition.x + cellSize, localPosition.z);
            const float down = SampleTerrainHeightForNormal(_entity, _terrain, _sourceModel, _sourceInverseModel, localPosition.x, localPosition.z - cellSize);
            const float up = SampleTerrainHeightForNormal(_entity, _terrain, _sourceModel, _sourceInverseModel, localPosition.x, localPosition.z + cellSize);
            return glm::normalize(Vector3(left - right, 2.0f * std::max(_terrain.cellSize, 0.01f), down - up));
        }
    }

    bool RebuildTerrainEntity(Entity &_entity)
    {
        if (!_entity.HasComponent<Terrain>())
            return false;

        Terrain &terrainComponent = _entity.GetComponent<Terrain>();
        const std::string terrainPath = AssetManager::ResolvePath(terrainComponent.terrain);
        if (terrainPath.empty())
            return false;

        TerrainAsset *terrain = AssetManager::GetTerrain(terrainPath);
        if (terrain == nullptr)
            return false;

        terrainComponent.terrain.path = terrainPath;
        if (MetaFileAsset *meta = AssetManager::GetMetaFile(terrainPath))
            terrainComponent.terrain.uuid = meta->uuid;

        const int width = terrain->GetSampleWidth();
        const int depth = terrain->GetSampleDepth();
        if (width < 2 || depth < 2)
            return false;

        Matrix4 terrainModel = Matrix4(1.0f);
        Matrix4 inverseModel = Matrix4(1.0f);
        if (_entity.HasComponent<Transform>())
        {
            terrainModel = _entity.GetComponent<Transform>().GetModelMatrix();
            inverseModel = glm::inverse(terrainModel);
        }

        std::vector<ModelAsset::RenderVertex3D> vertices;
        vertices.reserve(static_cast<size_t>(width * depth));
        for (int z = 0; z < depth; ++z)
        {
            for (int x = 0; x < width; ++x)
            {
                ModelAsset::RenderVertex3D vertex = {};
                vertex.position = terrain->GetLocalPosition(x, z);
                vertex.normal = CalculateTerrainNormal(_entity, *terrain, terrainModel, inverseModel, x, z);
                vertex.uv = Vector2(
                    static_cast<float>(x) / static_cast<float>(width - 1),
                    static_cast<float>(z) / static_cast<float>(depth - 1));
                vertices.push_back(vertex);
            }
        }

        std::vector<unsigned int> indices;
        indices.reserve(static_cast<size_t>((width - 1) * (depth - 1) * 6));
        for (int z = 0; z < depth - 1; ++z)
        {
            for (int x = 0; x < width - 1; ++x)
            {
                const unsigned int i0 = static_cast<unsigned int>(z * width + x);
                const unsigned int i1 = static_cast<unsigned int>(z * width + x + 1);
                const unsigned int i2 = static_cast<unsigned int>((z + 1) * width + x);
                const unsigned int i3 = static_cast<unsigned int>((z + 1) * width + x + 1);
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);
                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }

        if (terrainComponent.runtimeModelId < 0)
            terrainComponent.runtimeModelId = AssetManager::CreateModel();

        ModelAsset *modelAsset = AssetManager::GetModel(terrainComponent.runtimeModelId);
        if (modelAsset == nullptr)
            return false;

        ModelAsset::PrimitiveBuild3D primitive = {};
        primitive.vertices = std::move(vertices);
        primitive.indices = std::move(indices);
        primitive.materialSlot = 0;
        if (!modelAsset->SetRuntimePrimitives({primitive}, {"Terrain"}))
            return false;

        terrainComponent.runtimeRevision = modelAsset->GetGeometryRevision();

        Model &model = _entity.AddOrReplaceComponent<Model>();
        model.modelId = terrainComponent.runtimeModelId;
        model.color = Color(1.0f);
        model.staticModel = true;

        if (!terrain->materialPath.empty())
        {
            Material &material = _entity.AddOrReplaceComponent<Material>();
            material.materialId = AssetManager::LoadMaterial(terrain->materialPath);
            material.color = Color(1.0f);
        }

        MeshCollider &meshCollider = _entity.AddOrReplaceComponent<MeshCollider>();
        meshCollider.useAttachedModel = true;
        meshCollider.modelId = -1;
        meshCollider.modelPath.clear();
        meshCollider.active = true;

        return true;
    }
}
