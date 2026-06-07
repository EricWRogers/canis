#include <Canis/Terrain.hpp>

#include <Canis/AssetManager.hpp>

#include <algorithm>
#include <cmath>

namespace Canis
{
    namespace
    {
        Vector3 CalculateTerrainNormal(const TerrainAsset &_terrain, int _x, int _z)
        {
            const int width = _terrain.GetSampleWidth();
            const int depth = _terrain.GetSampleDepth();
            const float left = _terrain.GetHeight(std::max(0, _x - 1), _z);
            const float right = _terrain.GetHeight(std::min(width - 1, _x + 1), _z);
            const float down = _terrain.GetHeight(_x, std::max(0, _z - 1));
            const float up = _terrain.GetHeight(_x, std::min(depth - 1, _z + 1));
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

        std::vector<ModelAsset::RenderVertex3D> vertices;
        vertices.reserve(static_cast<size_t>(width * depth));
        for (int z = 0; z < depth; ++z)
        {
            for (int x = 0; x < width; ++x)
            {
                ModelAsset::RenderVertex3D vertex = {};
                vertex.position = terrain->GetLocalPosition(x, z);
                vertex.normal = CalculateTerrainNormal(*terrain, x, z);
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
