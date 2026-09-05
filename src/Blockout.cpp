#include <Canis/Blockout.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>

#include <Canis/AssetManager.hpp>
#include <Canis/Scene.hpp>

namespace Canis
{
    namespace
    {
        constexpr float kMinimumDimension = 0.01f;
        constexpr float kPi = 3.14159265358979323846f;

        Vector3 SafeSize(const Vector3 &_size)
        {
            return glm::max(glm::abs(_size), Vector3(kMinimumDimension));
        }

        Vector2 ScaleUV(const Vector2 &_uv, const Vector2 &_scale)
        {
            return _uv * glm::max(glm::abs(_scale), Vector2(0.001f));
        }

        void AddVertex(
            BlockoutMeshData &_mesh,
            const Vector3 &_position,
            const Vector3 &_normal,
            const Vector2 &_uv)
        {
            ModelAsset::RenderVertex3D vertex = {};
            vertex.position = _position;
            vertex.normal = _normal;
            vertex.uv = _uv;
            _mesh.vertices.push_back(vertex);
        }

        void AddQuad(
            BlockoutMeshData &_mesh,
            const Vector3 &_a,
            const Vector3 &_b,
            const Vector3 &_c,
            const Vector3 &_d,
            const Vector3 &_normal,
            const Vector2 &_uvScale)
        {
            const u32 first = static_cast<u32>(_mesh.vertices.size());
            const Vector2 extent(
                std::max(glm::length(_b - _a), kMinimumDimension),
                std::max(glm::length(_d - _a), kMinimumDimension));
            AddVertex(_mesh, _a, _normal, ScaleUV(Vector2(0.0f, 0.0f), _uvScale));
            AddVertex(_mesh, _b, _normal, ScaleUV(Vector2(extent.x, 0.0f), _uvScale));
            AddVertex(_mesh, _c, _normal, ScaleUV(extent, _uvScale));
            AddVertex(_mesh, _d, _normal, ScaleUV(Vector2(0.0f, extent.y), _uvScale));
            _mesh.indices.insert(_mesh.indices.end(), {
                first, first + 1u, first + 2u,
                first, first + 2u, first + 3u});
            _mesh.faces.push_back({{first, first+1, first+2, first+3},
                {ScaleUV({0,0}, _uvScale), ScaleUV({extent.x,0}, _uvScale),
                 ScaleUV(extent, _uvScale), ScaleUV({0,extent.y}, _uvScale)}});
        }

        void AddTriangle(
            BlockoutMeshData &_mesh,
            const Vector3 &_a,
            const Vector3 &_b,
            const Vector3 &_c,
            const Vector3 &_normal,
            const Vector2 &_uvScale)
        {
            const u32 first = static_cast<u32>(_mesh.vertices.size());
            AddVertex(_mesh, _a, _normal, ScaleUV(Vector2(0.0f, 0.0f), _uvScale));
            AddVertex(_mesh, _b, _normal, ScaleUV(Vector2(1.0f, 0.0f), _uvScale));
            AddVertex(_mesh, _c, _normal, ScaleUV(Vector2(0.5f, 1.0f), _uvScale));
            _mesh.indices.insert(_mesh.indices.end(), {first, first + 1u, first + 2u});
            _mesh.faces.push_back({{first, first+1, first+2},
                {ScaleUV({0,0}, _uvScale), ScaleUV({1,0}, _uvScale), ScaleUV({0.5f,1}, _uvScale)}});
        }

        void AddBox(
            BlockoutMeshData &_mesh,
            const Vector3 &_minimum,
            const Vector3 &_maximum,
            const Vector2 &_uvScale,
            int _cutsX = 0,
            int _cutsY = 0,
            int _cutsZ = 0)
        {
            const float x0 = _minimum.x;
            const float y0 = _minimum.y;
            const float z0 = _minimum.z;
            const float x1 = _maximum.x;
            const float y1 = _maximum.y;
            const float z1 = _maximum.z;

            const int segmentsX = std::clamp(_cutsX, 0, 32) + 1;
            const int segmentsY = std::clamp(_cutsY, 0, 32) + 1;
            const int segmentsZ = std::clamp(_cutsZ, 0, 32) + 1;
            auto coordinate = [](float _minimum, float _maximum, int _segment, int _segments) -> float
            {
                return _minimum + (_maximum - _minimum) *
                    (static_cast<float>(_segment) / static_cast<float>(_segments));
            };

            for (int x = 0; x < segmentsX; ++x)
            {
                const float xa = coordinate(x0, x1, x, segmentsX);
                const float xb = coordinate(x0, x1, x + 1, segmentsX);
                for (int z = 0; z < segmentsZ; ++z)
                {
                    const float za = coordinate(z0, z1, z, segmentsZ);
                    const float zb = coordinate(z0, z1, z + 1, segmentsZ);
                    AddQuad(_mesh, {xa,y1,za}, {xa,y1,zb}, {xb,y1,zb}, {xb,y1,za}, {0,1,0}, _uvScale);
                    AddQuad(_mesh, {xa,y0,zb}, {xa,y0,za}, {xb,y0,za}, {xb,y0,zb}, {0,-1,0}, _uvScale);
                }
            }
            for (int x = 0; x < segmentsX; ++x)
            {
                const float xa = coordinate(x0, x1, x, segmentsX);
                const float xb = coordinate(x0, x1, x + 1, segmentsX);
                for (int y = 0; y < segmentsY; ++y)
                {
                    const float ya = coordinate(y0, y1, y, segmentsY);
                    const float yb = coordinate(y0, y1, y + 1, segmentsY);
                    AddQuad(_mesh, {xb,ya,z0}, {xa,ya,z0}, {xa,yb,z0}, {xb,yb,z0}, {0,0,-1}, _uvScale);
                    AddQuad(_mesh, {xa,ya,z1}, {xb,ya,z1}, {xb,yb,z1}, {xa,yb,z1}, {0,0,1}, _uvScale);
                }
            }
            for (int z = 0; z < segmentsZ; ++z)
            {
                const float za = coordinate(z0, z1, z, segmentsZ);
                const float zb = coordinate(z0, z1, z + 1, segmentsZ);
                for (int y = 0; y < segmentsY; ++y)
                {
                    const float ya = coordinate(y0, y1, y, segmentsY);
                    const float yb = coordinate(y0, y1, y + 1, segmentsY);
                    AddQuad(_mesh, {x0,ya,za}, {x0,ya,zb}, {x0,yb,zb}, {x0,yb,za}, {-1,0,0}, _uvScale);
                    AddQuad(_mesh, {x1,ya,zb}, {x1,ya,za}, {x1,yb,za}, {x1,yb,zb}, {1,0,0}, _uvScale);
                }
            }
        }

        void BuildBox(const BlockoutShape &_shape, BlockoutMeshData &_mesh)
        {
            Vector3 size = SafeSize(_shape.size);
            if (_shape.type == BlockoutShapeType::PLANE)
                size.y = std::max(size.y, 0.05f);
            const Vector3 half(size.x * 0.5f, 0.0f, size.z * 0.5f);
            Vector3 minimum(-half.x, 0.0f, -half.z);
            Vector3 maximum(half.x, size.y, half.z);
            if (_shape.type == BlockoutShapeType::BOX)
            {
                minimum -= glm::max(_shape.extrudeNegative, Vector3(0.0f));
                maximum += glm::max(_shape.extrudePositive, Vector3(0.0f));
            }
            AddBox(
                _mesh,
                minimum,
                maximum,
                _shape.uvScale,
                _shape.loopCutsX,
                _shape.loopCutsY,
                _shape.loopCutsZ);
            _mesh.boundsMin = minimum;
            _mesh.boundsMax = maximum;
        }

        void BuildRamp(const BlockoutShape &_shape, BlockoutMeshData &_mesh)
        {
            const Vector3 size = SafeSize(_shape.size);
            const float x = size.x * 0.5f;
            const float z = size.z * 0.5f;
            const float h = size.y;
            const Vector3 slopeNormal = glm::normalize(Vector3(0.0f, size.z, -h));

            AddQuad(_mesh, {-x,0,z}, {-x,0,-z}, {x,0,-z}, {x,0,z}, {0,-1,0}, _shape.uvScale);
            AddQuad(_mesh, {-x,0,-z}, {-x,h,z}, {x,h,z}, {x,0,-z}, slopeNormal, _shape.uvScale);
            AddQuad(_mesh, {-x,0,z}, {x,0,z}, {x,h,z}, {-x,h,z}, {0,0,1}, _shape.uvScale);
            AddTriangle(_mesh, {-x,0,-z}, {-x,0,z}, {-x,h,z}, {-1,0,0}, _shape.uvScale);
            AddTriangle(_mesh, {x,0,z}, {x,0,-z}, {x,h,z}, {1,0,0}, _shape.uvScale);
            _mesh.boundsMin = Vector3(-x, 0.0f, -z);
            _mesh.boundsMax = Vector3(x, h, z);
        }

        void BuildCylinder(const BlockoutShape &_shape, BlockoutMeshData &_mesh)
        {
            const Vector3 size = SafeSize(_shape.size);
            const float radiusX = size.x * 0.5f;
            const float radiusZ = size.z * 0.5f;
            const float height = size.y;
            const int sides = std::clamp(_shape.sides, 3, 64);

            for (int side = 0; side < sides; ++side)
            {
                const float a0 = (static_cast<float>(side) / static_cast<float>(sides)) * 2.0f * kPi;
                const float a1 = (static_cast<float>(side + 1) / static_cast<float>(sides)) * 2.0f * kPi;
                const Vector3 p0(std::cos(a0) * radiusX, 0.0f, std::sin(a0) * radiusZ);
                const Vector3 p1(std::cos(a1) * radiusX, 0.0f, std::sin(a1) * radiusZ);
                const Vector3 p2(p1.x, height, p1.z);
                const Vector3 p3(p0.x, height, p0.z);
                const float mid = (a0 + a1) * 0.5f;
                const Vector3 normal = glm::normalize(Vector3(
                    std::cos(mid) / radiusX, 0.0f, std::sin(mid) / radiusZ));
                AddQuad(_mesh, p1, p0, p3, p2, normal, _shape.uvScale);
                AddTriangle(_mesh, {0,0,0}, p0, p1, {0,-1,0}, _shape.uvScale);
                AddTriangle(_mesh, {0,height,0}, p2, p3, {0,1,0}, _shape.uvScale);
            }
            _mesh.boundsMin = Vector3(-radiusX, 0.0f, -radiusZ);
            _mesh.boundsMax = Vector3(radiusX, height, radiusZ);
        }

        void BuildStairs(const BlockoutShape &_shape, BlockoutMeshData &_mesh)
        {
            const Vector3 size = SafeSize(_shape.size);
            const int steps = std::clamp(_shape.stepCount, 1, 64);
            const float run = size.z / static_cast<float>(steps);
            const float rise = size.y / static_cast<float>(steps);
            const float x = size.x * 0.5f;
            const float front = -size.z * 0.5f;

            for (int step = 0; step < steps; ++step)
            {
                const float z0 = front + run * static_cast<float>(step);
                const float height = rise * static_cast<float>(step + 1);
                AddBox(
                    _mesh,
                    Vector3(-x, 0.0f, z0),
                    Vector3(x, height, size.z * 0.5f),
                    _shape.uvScale);
            }
            _mesh.boundsMin = Vector3(-x, 0.0f, -size.z * 0.5f);
            _mesh.boundsMax = Vector3(x, size.y, size.z * 0.5f);
        }
    }

    bool BuildBlockoutMesh(
        const BlockoutShape &_shape,
        BlockoutMeshData &_mesh,
        std::string *_error)
    {
        _mesh = {};
        if (_shape.meshEdited)
        {
            std::string error;
            if (!ValidateBlockoutTopology(_shape.editMesh, error))
            {
                if (_error) *_error = error;
                return false;
            }
            bool firstPoint = true;
            for (const auto &face : _shape.editMesh.faces)
            {
                if (face.vertices.empty()) continue;
                const auto &points = _shape.editMesh.vertices;
                Vector3 normal(0);
                for (size_t i=0;i<face.vertices.size();++i)
                    normal += glm::cross(points[face.vertices[i]],points[face.vertices[(i+1)%face.vertices.size()]]);
                normal = glm::normalize(normal);
                u32 base = static_cast<u32>(_mesh.vertices.size());
                for (size_t i=0; i<face.vertices.size(); ++i)
                {
                    Vector3 p = points[face.vertices[i]];
                    AddVertex(_mesh, p, normal, face.uv[i]);
                    if (firstPoint) { _mesh.boundsMin = _mesh.boundsMax = p; firstPoint = false; }
                    _mesh.boundsMin = glm::min(_mesh.boundsMin, p);
                    _mesh.boundsMax = glm::max(_mesh.boundsMax, p);
                }
                for (u32 i=1; i+1<face.vertices.size(); ++i)
                    if (glm::length(glm::cross(points[face.vertices[i]]-points[face.vertices[0]],
                        points[face.vertices[i+1]]-points[face.vertices[0]]))>1e-7f)
                        _mesh.indices.insert(_mesh.indices.end(), {base, base+i, base+i+1});
            }
            return !_mesh.indices.empty();
        }
        switch (_shape.type)
        {
        case BlockoutShapeType::BOX:
        case BlockoutShapeType::PLANE:
            BuildBox(_shape, _mesh);
            break;
        case BlockoutShapeType::RAMP:
            BuildRamp(_shape, _mesh);
            break;
        case BlockoutShapeType::CYLINDER:
            BuildCylinder(_shape, _mesh);
            break;
        case BlockoutShapeType::STAIRS:
            BuildStairs(_shape, _mesh);
            break;
        default:
            if (_error != nullptr)
                *_error = "Unknown blockout shape type.";
            return false;
        }

        if (_mesh.vertices.empty() || _mesh.indices.empty())
        {
            if (_error != nullptr)
                *_error = "Blockout shape generated no geometry.";
            return false;
        }
        return true;
    }

    bool ExportBlockoutObj(
        const BlockoutShape &_shape,
        const std::filesystem::path &_path,
        std::string *_error)
    {
        BlockoutMeshData mesh = {};
        if (!BuildBlockoutMesh(_shape, mesh, _error))
            return false;

        std::error_code directoryError = {};
        if (!_path.parent_path().empty())
            std::filesystem::create_directories(_path.parent_path(), directoryError);
        if (directoryError)
        {
            if (_error != nullptr)
                *_error = "Could not create output directory: " + directoryError.message();
            return false;
        }

        std::ofstream output(_path, std::ios::trunc);
        if (!output)
        {
            if (_error != nullptr)
                *_error = "Could not open OBJ output path.";
            return false;
        }

        output << "# Canis procedural blockout\n";
        output << "o Blockout\n";
        for (const ModelAsset::RenderVertex3D &vertex : mesh.vertices)
            output << "v " << vertex.position.x << ' ' << vertex.position.y << ' ' << vertex.position.z << '\n';
        for (const ModelAsset::RenderVertex3D &vertex : mesh.vertices)
            output << "vt " << vertex.uv.x << ' ' << vertex.uv.y << '\n';
        for (const ModelAsset::RenderVertex3D &vertex : mesh.vertices)
            output << "vn " << vertex.normal.x << ' ' << vertex.normal.y << ' ' << vertex.normal.z << '\n';
        for (std::size_t index = 0; index + 2 < mesh.indices.size(); index += 3)
        {
            const u32 a = mesh.indices[index] + 1u;
            const u32 b = mesh.indices[index + 1u] + 1u;
            const u32 c = mesh.indices[index + 2u] + 1u;
            output << "f "
                << a << '/' << a << '/' << a << ' '
                << b << '/' << b << '/' << b << ' '
                << c << '/' << c << '/' << c << '\n';
        }

        if (!output.good())
        {
            if (_error != nullptr)
                *_error = "Failed while writing OBJ geometry.";
            return false;
        }
        return true;
    }

    bool RebuildBlockoutEntity(Entity &_entity, bool _collision)
    {
        if (!_entity.HasComponent<BlockoutShape>())
            return false;

        BlockoutShape &shape = _entity.GetComponent<BlockoutShape>();
        BlockoutMeshData mesh = {};
        std::string error = {};
        if (!BuildBlockoutMesh(shape, mesh, &error))
            return false;

        if (shape.runtimeModelId < 0)
            shape.runtimeModelId = AssetManager::CreateModel();
        ModelAsset *modelAsset = AssetManager::GetModel(shape.runtimeModelId);
        if (modelAsset == nullptr)
            return false;

        ModelAsset::PrimitiveBuild3D primitive = {};
        primitive.vertices = std::move(mesh.vertices);
        primitive.indices = std::move(mesh.indices);
        primitive.materialSlot = 0;
        if (!modelAsset->SetRuntimePrimitives({primitive}, {"Blockout"}))
            return false;

        shape.runtimeRevision = modelAsset->GetGeometryRevision();
        Model &model = _entity.AddOrReplaceComponent<Model>();
        model.modelId = shape.runtimeModelId;
        model.staticModel = true;
        model.castShadow = shape.castShadow;

        if (!_collision) return true;

        if (!_entity.HasComponent<Material>())
        {
            Material &material = *_entity.AddComponent<Material>();
            material.materialId = AssetManager::LoadMaterial(
                "assets/defaults/materials/whitebox_neutral.material");
        }

        if (shape.active && shape.generateCollision)
        {
            Rigidbody &rigidbody = _entity.AddOrReplaceComponent<Rigidbody>();
            rigidbody.motionType = RigidbodyMotionType::STATIC;
            rigidbody.useGravity = false;

            if (!shape.meshEdited && (shape.type == BlockoutShapeType::BOX || shape.type == BlockoutShapeType::PLANE))
            {
                if (_entity.HasComponent<MeshCollider>())
                    _entity.RemoveComponent<MeshCollider>();
                BoxCollider &collider = _entity.AddOrReplaceComponent<BoxCollider>();
                collider.size = SafeSize(mesh.boundsMax - mesh.boundsMin);
                collider.offset = (mesh.boundsMin + mesh.boundsMax) * 0.5f;
            }
            else
            {
                if (_entity.HasComponent<BoxCollider>())
                    _entity.RemoveComponent<BoxCollider>();
                MeshCollider &collider = _entity.AddOrReplaceComponent<MeshCollider>();
                collider.useAttachedModel = true;
                collider.modelId = -1;
                collider.modelPath.clear();
                collider.active = true;
            }
        }
        else
        {
            if (_entity.HasComponent<BoxCollider>())
                _entity.RemoveComponent<BoxCollider>();
            if (_entity.HasComponent<MeshCollider>())
                _entity.RemoveComponent<MeshCollider>();
            if (_entity.HasComponent<Rigidbody>())
                _entity.RemoveComponent<Rigidbody>();
        }

        if (shape.generateCollision)
        {
            for (Entity *candidate : _entity.scene.GetEntities())
            {
                if (candidate == nullptr)
                    continue;
                if (candidate->HasComponent<NavMeshSurface>())
                    _entity.scene.InvalidateNavMesh(*candidate);
                if (candidate->HasComponent<CloudNavSurface>())
                    _entity.scene.InvalidateCloudNav(*candidate);
            }
        }
        return true;
    }
}
