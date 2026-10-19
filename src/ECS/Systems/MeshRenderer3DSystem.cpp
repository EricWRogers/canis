#include <Canis/StaticVisibility.hpp>
#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <Canis/VFX/Trails.hpp>
#include <Canis/Frustum.hpp>
#include <Canis/AnimationVisibility.hpp>
#include <Canis/Profiler.hpp>
#include <Canis/RenderMetrics.hpp>
#include <Canis/RenderVisibility.hpp>
#include <deque>

#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Shader.hpp>
#include <Canis/Time.hpp>
#include <Canis/Window.hpp>
#include <Canis/CloudVolume.hpp>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>

namespace Canis
{
    namespace
    {
        constexpr int kMaxPointLights = 8;
        constexpr int kDirectionalShadowMapSize = 4096;

        static const float kSkyboxVertices[] = {
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        struct DirectionalLightState
        {
            bool enabled = false;
            int mode = LightMode::Realtime;
            Vector3 direction = Vector3(-0.4f, -1.0f, -0.25f);
            Vector3 color = Vector3(1.0f, 0.98f, 0.95f);
            float intensity = 1.0f;
        };

        struct PointLightState
        {
            Vector3 position = Vector3(0.0f);
            Vector3 color = Vector3(1.0f);
            float intensity = 1.0f;
            float range = 12.0f;
            int mode = LightMode::Realtime;
            Vector3 direction{0, 0, -1};
            Vector2 cone{-1, -2};
        };

        struct TransparentModelEntry
        {
            entt::entity entityHandle = entt::null;
            float distanceSquared = 0.0f;
        };

        constexpr int kColliderCircleSegments = 28;

        Vector3 TransformPoint(const Matrix4 &_matrix, const Vector3 &_point)
        {
            const Vector4 transformed = _matrix * Vector4(_point, 1.0f);
            return Vector3(transformed.x, transformed.y, transformed.z);
        }

        void AppendLine(std::vector<Vector3> &_lines, const Vector3 &_start, const Vector3 &_end)
        {
            _lines.push_back(_start);
            _lines.push_back(_end);
        }

        void AppendBoundsBoxLines(std::vector<Vector3> &_lines, const Matrix4 &_matrix, const Vector3 &_min, const Vector3 &_max)
        {
            const std::array<Vector3, 8> corners = {
                Vector3(_min.x, _min.y, _min.z),
                Vector3(_max.x, _min.y, _min.z),
                Vector3(_max.x, _max.y, _min.z),
                Vector3(_min.x, _max.y, _min.z),
                Vector3(_min.x, _min.y, _max.z),
                Vector3(_max.x, _min.y, _max.z),
                Vector3(_max.x, _max.y, _max.z),
                Vector3(_min.x, _max.y, _max.z)
            };

            const std::array<std::array<int, 2>, 12> edges = {{
                {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
                {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
                {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
            }};

            for (const std::array<int, 2> &edge : edges)
            {
                AppendLine(
                    _lines,
                    TransformPoint(_matrix, corners[edge[0]]),
                    TransformPoint(_matrix, corners[edge[1]]));
            }
        }

        void AppendTriangleMeshLines(
            std::vector<Vector3> &_lines,
            const Matrix4 &_matrix,
            const std::vector<Vector3> &_vertices,
            const std::vector<u32> &_indices)
        {
            for (size_t i = 0; i + 2 < _indices.size(); i += 3)
            {
                const u32 index0 = _indices[i + 0];
                const u32 index1 = _indices[i + 1];
                const u32 index2 = _indices[i + 2];
                if (index0 >= _vertices.size() || index1 >= _vertices.size() || index2 >= _vertices.size())
                    continue;

                const Vector3 point0 = TransformPoint(_matrix, _vertices[index0]);
                const Vector3 point1 = TransformPoint(_matrix, _vertices[index1]);
                const Vector3 point2 = TransformPoint(_matrix, _vertices[index2]);
                AppendLine(_lines, point0, point1);
                AppendLine(_lines, point1, point2);
                AppendLine(_lines, point2, point0);
            }
        }

        bool AppendConvexHullLines(
            std::vector<Vector3> &_lines,
            const Matrix4 &_matrix,
            const std::vector<Vector3> &_vertices,
            float _convexRadius)
        {
            if (_vertices.size() < 4)
                return false;

            std::vector<JPH::Vec3> points = {};
            points.reserve(_vertices.size());
            for (const Vector3 &vertex : _vertices)
                points.emplace_back(vertex.x, vertex.y, vertex.z);

            JPH::ConvexHullShapeSettings settings(
                points.data(),
                static_cast<int>(points.size()),
                glm::max(0.0f, _convexRadius));
            JPH::Shape::ShapeResult result = settings.Create();
            if (result.HasError())
                return false;

            const JPH::Shape *shape = result.Get().GetPtr();
            JPH::Shape::GetTrianglesContext context;
            shape->GetTrianglesStart(
                context,
                JPH::AABox::sBiggest(),
                // Convex hull triangles are emitted relative to their center
                // of mass. Put that center back at its source-model position
                // before applying the collider and entity transforms.
                shape->GetCenterOfMass(),
                JPH::Quat::sIdentity(),
                JPH::Vec3::sOne());

            constexpr int batchSize = 256;
            JPH::Float3 triangleVertices[batchSize * 3];
            for (;;)
            {
                const int triangleCount = shape->GetTrianglesNext(context, batchSize, triangleVertices);
                if (triangleCount == 0)
                    break;

                for (int triangle = 0; triangle < triangleCount; ++triangle)
                {
                    const JPH::Vec3 point0(triangleVertices[triangle * 3 + 0]);
                    const JPH::Vec3 point1(triangleVertices[triangle * 3 + 1]);
                    const JPH::Vec3 point2(triangleVertices[triangle * 3 + 2]);
                    const Vector3 vertex0(point0.GetX(), point0.GetY(), point0.GetZ());
                    const Vector3 vertex1(point1.GetX(), point1.GetY(), point1.GetZ());
                    const Vector3 vertex2(point2.GetX(), point2.GetY(), point2.GetZ());
                    AppendLine(_lines, TransformPoint(_matrix, vertex0), TransformPoint(_matrix, vertex1));
                    AppendLine(_lines, TransformPoint(_matrix, vertex1), TransformPoint(_matrix, vertex2));
                    AppendLine(_lines, TransformPoint(_matrix, vertex2), TransformPoint(_matrix, vertex0));
                }
            }

            return true;
        }

        void AppendCircleLines(
            std::vector<Vector3> &_lines,
            const Matrix4 &_matrix,
            const Vector3 &_center,
            const Vector3 &_axisA,
            const Vector3 &_axisB,
            float _radius)
        {
            constexpr float twoPi = 6.28318530718f;
            for (int i = 0; i < kColliderCircleSegments; ++i)
            {
                const float a0 = (static_cast<float>(i) / static_cast<float>(kColliderCircleSegments)) * twoPi;
                const float a1 = (static_cast<float>(i + 1) / static_cast<float>(kColliderCircleSegments)) * twoPi;
                const Vector3 p0 = _center + (_axisA * (std::cos(a0) * _radius)) + (_axisB * (std::sin(a0) * _radius));
                const Vector3 p1 = _center + (_axisA * (std::cos(a1) * _radius)) + (_axisB * (std::sin(a1) * _radius));
                AppendLine(_lines, TransformPoint(_matrix, p0), TransformPoint(_matrix, p1));
            }
        }

        void AppendArcLines(
            std::vector<Vector3> &_lines,
            const Matrix4 &_matrix,
            const Vector3 &_center,
            const Vector3 &_axisA,
            const Vector3 &_axisB,
            float _radius,
            float _startRadians,
            float _endRadians)
        {
            const int segmentCount = kColliderCircleSegments / 2;
            for (int i = 0; i < segmentCount; ++i)
            {
                const float t0 = static_cast<float>(i) / static_cast<float>(segmentCount);
                const float t1 = static_cast<float>(i + 1) / static_cast<float>(segmentCount);
                const float a0 = _startRadians + ((_endRadians - _startRadians) * t0);
                const float a1 = _startRadians + ((_endRadians - _startRadians) * t1);
                const Vector3 p0 = _center + (_axisA * (std::cos(a0) * _radius)) + (_axisB * (std::sin(a0) * _radius));
                const Vector3 p1 = _center + (_axisA * (std::cos(a1) * _radius)) + (_axisB * (std::sin(a1) * _radius));
                AppendLine(_lines, TransformPoint(_matrix, p0), TransformPoint(_matrix, p1));
            }
        }

        i32 ResolveMeshColliderModelId(entt::registry &_registry, entt::entity _entityHandle, const MeshCollider *_meshCollider)
        {
            if (_meshCollider == nullptr)
                return -1;

            if (_meshCollider->useAttachedModel)
            {
                if (const Model *model = _registry.try_get<Model>(_entityHandle))
                    return model->modelId;

                return -1;
            }

            if (_meshCollider->modelId >= 0)
                return _meshCollider->modelId;

            if (!_meshCollider->modelPath.empty())
                return AssetManager::LoadModel(_meshCollider->modelPath);

            return -1;
        }

        i32 ResolveMeshColliderNodeIndex(entt::registry &_registry, entt::entity _entityHandle, const MeshCollider *_meshCollider)
        {
            if (_meshCollider == nullptr || !_meshCollider->useAttachedModel)
                return -1;

            if (const Model *model = _registry.try_get<Model>(_entityHandle))
                return model->nodeIndex;

            return -1;
        }

        bool ResolveMeshColliderApplyNodeTransform(entt::registry &_registry, entt::entity _entityHandle, const MeshCollider *_meshCollider)
        {
            if (_meshCollider == nullptr || !_meshCollider->useAttachedModel)
                return true;

            if (const Model *model = _registry.try_get<Model>(_entityHandle))
                return model->applyNodeTransform;

            return true;
        }

        i32 ResolveConvexMeshColliderModelId(entt::registry &_registry, entt::entity _entityHandle, const ConvexMeshCollider *_collider)
        {
            if (_collider == nullptr)
                return -1;
            if (_collider->useAttachedModel)
            {
                if (const Model *model = _registry.try_get<Model>(_entityHandle))
                    return model->modelId;
                return -1;
            }
            if (_collider->modelId >= 0)
                return _collider->modelId;
            if (!_collider->modelPath.empty())
                return AssetManager::LoadModel(_collider->modelPath);
            return -1;
        }

        i32 ResolveConvexMeshColliderNodeIndex(entt::registry &_registry, entt::entity _entityHandle, const ConvexMeshCollider *_collider)
        {
            if (_collider == nullptr || !_collider->useAttachedModel)
                return -1;
            if (const Model *model = _registry.try_get<Model>(_entityHandle))
                return model->nodeIndex;
            return -1;
        }

        bool ResolveConvexMeshColliderApplyNodeTransform(entt::registry &_registry, entt::entity _entityHandle, const ConvexMeshCollider *_collider)
        {
            if (_collider == nullptr || !_collider->useAttachedModel)
                return true;
            if (const Model *model = _registry.try_get<Model>(_entityHandle))
                return model->applyNodeTransform;
            return true;
        }

        struct StaticModelBatchKey
        {
            uint64_t group=0;
            i32 modelId = -1;
            int lodLevel = 0;
            i32 materialId = -1;
            i32 nodeIndex = -1;
            bool applyNodeTransform = true;
            bool weatherSurface = false;
            float modelColorR = 1.0f;
            float modelColorG = 1.0f;
            float modelColorB = 1.0f;
            float modelColorA = 1.0f;
            float materialColorR = 1.0f;
            float materialColorG = 1.0f;
            float materialColorB = 1.0f;
            float materialColorA = 1.0f;

            bool operator<(const StaticModelBatchKey &_other) const
            {
                return std::tie(
                    group,
                    lodLevel,
                    modelId,
                    materialId,
                    nodeIndex,
                    applyNodeTransform,
                    weatherSurface,
                    modelColorR,
                    modelColorG,
                    modelColorB,
                    modelColorA,
                    materialColorR,
                    materialColorG,
                    materialColorB,
                    materialColorA) <
                    std::tie(
                        _other.group,
                        _other.lodLevel,
                        _other.modelId,
                        _other.materialId,
                        _other.nodeIndex,
                        _other.applyNodeTransform,
                        _other.weatherSurface,
                        _other.modelColorR,
                        _other.modelColorG,
                        _other.modelColorB,
                        _other.modelColorA,
                        _other.materialColorR,
                        _other.materialColorG,
                        _other.materialColorB,
                        _other.materialColorA);
            }
        };

        struct StaticModelBatch
        {
            StaticModelBatchKey key = {};
            std::vector<Matrix4> modelMatrices = {};
            std::vector<Color> colors = {};
            bool graph=false;
            std::vector<TransparentModelEntry> sourceEntries = {};
            float distanceSquared = 0.0f;
        };

        bool StaticRenderTransform(entt::registry& registry, entt::entity handle, const Model& model)
        {
            if (!model.staticModel)
                return false;
            const auto* body = registry.try_get<Rigidbody>(handle);
            return !body || !body->active || body->motionType == RigidbodyMotionType::STATIC;
        }

        bool CullBounds(entt::registry& registry,entt::entity handle,ModelAsset& asset,
                        Model& model,Shader* defaultShader,Vector3& minimum,Vector3& maximum)
        {
            if (!RenderMetrics::CullingEnabled() || registry.try_get<Terrain>(handle)) return false;
            if (auto* material=registry.try_get<Material>(handle)) {
                auto safeShader = [defaultShader](i32 materialId) {
                    if (materialId < 0) return true; // Empty slots use the base material.
                    auto* source = AssetManager::GetMaterial(materialId);
                    if (!source) return false;
                    if (source->shaderId < 0) return true;
                    auto* shader = AssetManager::Get<ShaderAsset>(source->shaderId);
                    return shader && (shader->GetShader() == defaultShader ||
                        shader->GetShader()->SupportsStaticBatching() ||
                        (shader->SupportsGraphInstancing() && !shader->HasVertexDeformation()));
                };
                if (!safeShader(material->materialId)) return false;
                // Texture/color slots do not expand geometry. Check every shader
                // before admitting the complete mesh bounds into static visibility.
                for (i32 slot : material->materialIds)
                    if (!safeShader(slot)) return false;
            }
            if (asset.HasDeformingGeometry() || registry.try_get<ModelAnimation>(handle))
                return AnimatedModelBounds(registry,handle,asset,model,minimum,maximum);
            return asset.GetLocalBounds(minimum,maximum,model.nodeIndex,model.applyNodeTransform);
        }

        bool UsesTransparentColor(const Color &_color)
        {
            return _color.a < 0.999f;
        }

        bool MaterialAssetUsesTransparency(MaterialAsset *_materialAsset)
        {
            return _materialAsset != nullptr &&
                (((
                    _materialAsset->info & MATERIAL_HAS_COLOR) != 0u) &&
                    UsesTransparentColor(_materialAsset->color));
        }

        bool EntityUsesTransparency(entt::registry &_registry, entt::entity _entityHandle)
        {
            Model *model = _registry.try_get<Model>(_entityHandle);
            if (model != nullptr && UsesTransparentColor(model->color))
                return true;

            Material *material = _registry.try_get<Material>(_entityHandle);
            if (material == nullptr)
                return false;

            if (UsesTransparentColor(material->color))
                return true;

            if (material->materialId >= 0 && MaterialAssetUsesTransparency(AssetManager::GetMaterial(material->materialId)))
                return true;

            for (const i32 slotMaterialId : material->materialIds)
            {
                if (slotMaterialId >= 0 && MaterialAssetUsesTransparency(AssetManager::GetMaterial(slotMaterialId)))
                    return true;
            }

            return false;
        }

        bool HasMaterialFields(const MaterialFields &_fields)
        {
            return !_fields.GetIntUniforms().empty() ||
                !_fields.GetFloatUniforms().empty() ||
                !_fields.GetVec2Uniforms().empty() ||
                !_fields.GetVec3Uniforms().empty() ||
                !_fields.GetVec4Uniforms().empty() ||
                !_fields.GetColorUniforms().empty() ||
                !_fields.GetTextureUniforms().empty();
        }

        DirectionalLightState GatherDirectionalLight(entt::registry &_registry)
        {
            DirectionalLightState state = {};

            auto directionalLightView = _registry.view<DirectionalLight>();
            for (const entt::entity entityHandle : directionalLightView)
            {
                DirectionalLight &light = directionalLightView.get<DirectionalLight>(entityHandle);
                Entity *entity = light.entity;
                if (entity == nullptr || !entity->Active() || !light.enabled || light.mode == LightMode::Baked)
                    continue;
                if (auto* t = _registry.try_get<Transform>(entityHandle); t && !t->IsActiveInHierarchy())
                    continue;
                state.mode = light.mode;
                state.enabled = light.enabled;
                state.direction = light.direction;
                const float directionLength = glm::length(state.direction);
                if (directionLength > 0.0001f)
                    state.direction /= directionLength;
                else
                    state.direction = Vector3(0.0f, -1.0f, 0.0f);
                state.color = Vector3(light.color.r, light.color.g, light.color.b);
                state.intensity = light.intensity;
                break;
            }

            return state;
        }

        std::vector<PointLightState> GatherPointLights(entt::registry &_registry)
        {
            std::vector<PointLightState> lights = {};
            lights.reserve(kMaxPointLights);

            auto pointLightView = _registry.view<PointLight, Transform>();
            for (const entt::entity entityHandle : pointLightView)
            {
                if (lights.size() >= kMaxPointLights)
                    break;

                PointLight &light = pointLightView.get<PointLight>(entityHandle);
                Transform &lightTransform = pointLightView.get<Transform>(entityHandle);
                Entity *entity = light.entity;
                if (entity == nullptr)
                    entity = lightTransform.entity;

                if (entity == nullptr || !lightTransform.IsActiveInHierarchy() || !light.enabled || light.mode == LightMode::Baked)
                    continue;

                PointLightState state = {};
                state.mode = light.mode;
                state.position = lightTransform.GetGlobalPosition();
                state.color = Vector3(light.color.r, light.color.g, light.color.b);
                state.intensity = light.intensity;
                state.range = light.range;
                lights.push_back(state);
            }

            for (auto handle : _registry.view<SpotLight, Transform>())
            {
                if (lights.size() >= kMaxPointLights) break;
                auto& light = _registry.get<SpotLight>(handle);
                auto& t = _registry.get<Transform>(handle);
                if (!t.IsActiveInHierarchy() || !light.enabled || light.mode == LightMode::Baked) continue;
                PointLightState state;
                state.mode = light.mode;
                state.position = t.GetGlobalPosition();
                state.direction = glm::normalize(t.GetGlobalRotation() * Vector3(0, 0, -1));
                state.color = Vector3(light.color);
                state.intensity = light.intensity;
                state.range = light.range;
                const float outer = std::clamp(light.spotAngle, .1f, 179.f);
                const float inner = std::clamp(light.innerAngle, 0.f, outer);
                state.cone = {std::cos(glm::radians(inner * .5f)), std::cos(glm::radians(outer * .5f))};
                lights.push_back(state);
            }
            return lights;
        }
    } // namespace

    struct MeshRendererCache
    {
        struct TransformRecord {
            uint64_t frame = 0, revision = 0, parentRevision = 0;
            entt::entity parent = entt::null;
            Vector3 position{0}, scale{1};
            Quaternion rotation{1,0,0,0};
            bool prefixEnabled = false;
            Matrix4 prefix{1}, matrix{1};
            uint64_t boundsRevision=0;
            Vector3 localMinimum{0},localMaximum{0},worldMinimum{0},worldMaximum{0};
            bool finiteBounds=false;
        };
        std::unordered_map<entt::entity,TransformRecord> frameMatrices;
        uint64_t preparationFrame = 0, transformRevision = 0;
        struct Candidate {
            entt::entity handle;
            ModelAsset* asset;
            bool transparent;
            bool cullable;
            Vector3 minimum, maximum;
        };
        struct LODBoundsRecord { uint64_t profile=0,reload=0,geometry=0; Vector3 low{0},high{0}; bool valid=false; };
        std::map<std::tuple<int,std::string,int,bool>,LODBoundsRecord> lodBounds;
        struct LODState { int level=-1, source=-1; uint64_t revision=0; };
        std::unordered_map<uint64_t,std::unordered_map<entt::entity,LODState>> lodViews;
        std::unordered_map<entt::entity,int> renderModels,shadowModels,levels;
        std::vector<Candidate> candidates;
        StaticVisibility visibility;
        std::vector<StaticVisibility::Bounds> staticBounds;
        std::vector<unsigned char> hiddenCandidates;
        std::vector<TransparentModelEntry> opaqueScratch, transparentScratch;
        struct Input {
            StaticModelBatchKey key;
            Matrix4 matrix;
            Color color;
            entt::entity entity;
            bool graph;
            uint64_t geometry;
            int program;
            uint64_t materialInfo;
            int albedo;
            float materialAlpha;
            bool operator==(const Input& other) const {
                return !(key<other.key) && !(other.key<key) && matrix==other.matrix &&
                    color==other.color && entity==other.entity && graph==other.graph && geometry==other.geometry &&
                    program==other.program && materialInfo==other.materialInfo && albedo==other.albedo && materialAlpha==other.materialAlpha;
            }
        };
        struct View {
            std::vector<Input> inputs;
            std::vector<uint64_t> hiddenRooms;
            std::vector<StaticModelBatch> batches;
            std::vector<TransparentModelEntry> singles;
        };
        std::deque<View> views;
        using MergeKey = std::tuple<int, int, int, StaticModelBatchKey>;
        struct MergedChunk
        {
            std::vector<Input> inputs;
            ModelAsset geometry;
            std::vector<ModelAsset::DrawRanges3D> sourceRanges;
            ModelAsset::DrawRanges3D visibleRanges;
            bool valid = false;
            entt::entity representative = entt::null;
            ~MergedChunk() { geometry.Free(); }
        };
        using MergedChunks = std::map<MergeKey, std::unique_ptr<MergedChunk>>;
        std::map<uint64_t, MergedChunks> mergedViews;

        struct Room {
            Vector3 minimum,maximum;
            unsigned query=0;
            bool pending=false,visible=true;
            uint64_t generation=0;
        };
        std::map<uint64_t,Room> rooms;
        std::vector<Input> occluders;
        Matrix4 queryView=Matrix4(0);
        uint64_t generation=0;
        uint64_t reloadRevision=0;
        ~MeshRendererCache() {
#ifndef __EMSCRIPTEN__
            for(auto& [id,room]:rooms)if(room.query)glDeleteQueries(1,&room.query);
#endif
        }
        void UpdateRooms(const std::vector<Input>& inputs, const Matrix4& clip) {
#ifndef __EMSCRIPTEN__
            const auto revision=AssetManager::GetRenderReloadRevision();
            const bool changed=queryView!=clip || occluders!=inputs || reloadRevision!=revision;
            if(changed) {
                reloadRevision=revision;
                ++generation;queryView=clip;occluders=inputs;
                for(auto& [id,room]:rooms) {
                    room.visible=true;room.minimum=Vector3(FLT_MAX);room.maximum=Vector3(-FLT_MAX);
                }
                for(const auto& input:inputs) {
                    if(!input.key.group)continue;
                    auto* asset=AssetManager::GetModel(input.key.modelId);Vector3 lo,hi;
                    if(!asset || !asset->GetLocalBounds(lo,hi,input.key.nodeIndex,input.key.applyNodeTransform))continue;
                    auto [it,inserted]=rooms.try_emplace(input.key.group);
                    auto& room=it->second;
                    if(inserted) { room.minimum=Vector3(FLT_MAX);room.maximum=Vector3(-FLT_MAX); }
                    for(int corner=0;corner<8;++corner) {
                        const Vector3 p(input.matrix*Vector4(corner&1?hi.x:lo.x,corner&2?hi.y:lo.y,corner&4?hi.z:lo.z,1));
                        room.minimum=glm::min(room.minimum,p-Vector3(.1f));room.maximum=glm::max(room.maximum,p+Vector3(.1f));
                    }
                }
                for(auto it=rooms.begin();it!=rooms.end();) {
                    if(it->second.minimum.x>it->second.maximum.x) {
                        if(it->second.query)glDeleteQueries(1,&it->second.query);
                        it=rooms.erase(it);
                    } else ++it;
                }
            }
            for(auto& [id,room]:rooms)if(room.pending) {
                GLint available=0;glGetQueryObjectiv(room.query,GL_QUERY_RESULT_AVAILABLE,&available);
                if(available) {
                    GLuint samples=1;glGetQueryObjectuiv(room.query,GL_QUERY_RESULT,&samples);
                    room.visible=room.generation==generation ? samples!=0 : true;room.pending=false;
                }
            }
#endif
        }
    };

    const Matrix4& MeshRenderer3DSystem::RenderMatrix(entt::entity handle,const Transform& transform)
    {
        auto& cache = *m_batchCache;
        auto& record = cache.frameMatrices[handle];
        if (record.frame == cache.preparationFrame) return record.matrix;
        const Matrix4* parentMatrix = nullptr;
        entt::entity parentHandle = entt::null;
        uint64_t parentRevision = 0;
        if (transform.parent)
            if (auto* parent = transform.parent->TryGetComponent<Transform>()) {
                parentHandle = transform.parent.GetHandle();
                parentMatrix = &RenderMatrix(parentHandle, *parent);
                parentRevision = cache.frameMatrices.at(parentHandle).revision;
            }
        if (!record.revision || record.parent != parentHandle || record.parentRevision != parentRevision ||
            record.position != transform.position || record.rotation != transform.rotation ||
            record.scale != transform.scale || record.prefixEnabled != transform.useLocalMatrixPrefix ||
            (transform.useLocalMatrixPrefix && record.prefix != transform.localMatrixPrefix)) {
            record.parent = parentHandle; record.parentRevision = parentRevision;
            record.position = transform.position; record.rotation = transform.rotation; record.scale = transform.scale;
            record.prefixEnabled = transform.useLocalMatrixPrefix; record.prefix = transform.localMatrixPrefix;
            record.matrix = parentMatrix ? *parentMatrix * transform.GetLocalMatrix() : transform.GetLocalMatrix();
            record.revision = ++cache.transformRevision;
        }
        record.frame = cache.preparationFrame;
        return record.matrix;
    }

    void MeshRenderer3DSystem::Create()
    {
        int id = AssetManager::LoadShader("assets/shaders/model3d");
        Shader *shader = AssetManager::Get<ShaderAsset>(id)->GetShader();

        if (!shader->IsLinked())
        {
            shader->AddAttribute("vertexPosition");
            shader->AddAttribute("vertexNormal");
            shader->AddAttribute("vertexUV");
            shader->Link();
        }

        m_shader = shader;

        int skyboxShaderId = AssetManager::LoadShader("assets/shaders/skybox");
        Shader *skyboxShader = AssetManager::Get<ShaderAsset>(skyboxShaderId)->GetShader();
        if (!skyboxShader->IsLinked())
        {
            skyboxShader->AddAttribute("aPos");
            skyboxShader->Link();
        }

        m_skyboxShader = skyboxShader;

        int shadowShaderId = AssetManager::LoadShader("assets/shaders/model3d_shadow");
        Shader *shadowShader = AssetManager::Get<ShaderAsset>(shadowShaderId)->GetShader();
        if (!shadowShader->IsLinked())
        {
            shadowShader->AddAttribute("vertexPosition");
            shadowShader->AddAttribute("vertexNormal");
            shadowShader->AddAttribute("vertexUV");
            shadowShader->Link();
        }

        m_shadowShader = shadowShader;

        int colliderShaderId = AssetManager::LoadShader("assets/shaders/debug_collider");
        Shader *colliderShader = AssetManager::Get<ShaderAsset>(colliderShaderId)->GetShader();
        if (!colliderShader->IsLinked())
        {
            colliderShader->AddAttribute("vertexPosition");
            colliderShader->Link();
        }

        m_colliderDebugShader = colliderShader;
        CreateShadowMap();
        CreateSkyboxGeometry();
        CreateColliderDebugGeometry();
    }

    bool MeshRenderer3DSystem::LODBounds(entt::entity handle, const Model& model, Vector3& low, Vector3& high)
    {
        auto* group=scene->GetRegistry().try_get<LODGroup>(handle);
        if(!group || !group->enabled)return false;
        auto profile=GetLODProfile(group->profilePath);
        if(!profile)return false;
        auto chain=std::find_if(profile->chains.begin(),profile->chains.end(),[&](const auto& value){return value.sourceId==model.modelId;});
        if(chain==profile->chains.end())return false;
        auto& bounds=m_batchCache->lodBounds[{model.modelId,group->profilePath,model.nodeIndex,model.applyNodeTransform}];
        auto* base=AssetManager::GetModel(model.modelId);
        if(bounds.profile!=profile->revision || bounds.reload!=AssetManager::GetRenderReloadRevision() || bounds.geometry!=base->GetGeometryRevision()) {
            bounds.valid=base->GetLocalBounds(bounds.low,bounds.high,model.nodeIndex,model.applyNodeTransform);
            if(bounds.valid)for(const auto& level:chain->levels)if(auto* asset=AssetManager::GetModel(level.modelId)) {
                Vector3 a,b;if(asset->GetLocalBounds(a,b,model.nodeIndex,model.applyNodeTransform)){bounds.low=glm::min(bounds.low,a);bounds.high=glm::max(bounds.high,b);}
            }
            bounds.profile=profile->revision;bounds.reload=AssetManager::GetRenderReloadRevision();bounds.geometry=base->GetGeometryRevision();
        }
        low=bounds.low;high=bounds.high;return bounds.valid;
    }

    int MeshRenderer3DSystem::RenderModelId(entt::entity handle, const Model& model) const
    {
        auto found = m_batchCache->renderModels.find(handle);
        return found == m_batchCache->renderModels.end() ? model.modelId : found->second;
    }
    int MeshRenderer3DSystem::ShadowModelId(entt::entity handle, const Model& model) const
    {
        auto found = m_batchCache->shadowModels.find(handle);
        return found == m_batchCache->shadowModels.end() ? model.modelId : found->second;
    }
    Color MeshRenderer3DSystem::LODColor(entt::entity handle, Color color) const
    {
        auto* group = scene->GetRegistry().try_get<LODGroup>(handle);
        if (!group || !group->debugColors) return color;
        static const Color colors[] = {Color(1,.3f,.3f,1),Color(1,.9f,.2f,1),Color(.2f,.8f,1,1),Color(.8f,.3f,1,1)};
        auto found = m_batchCache->levels.find(handle);
        return color * colors[found == m_batchCache->levels.end() ? 0 : std::clamp(found->second,0,3)];
    }
    void MeshRenderer3DSystem::SelectLODs(entt::registry& registry, const Matrix4& view, const Matrix4& projection, uint64_t viewKey)
    {
        auto& cache = *m_batchCache;
        cache.renderModels.clear();cache.shadowModels.clear();cache.levels.clear();
        auto& states = cache.lodViews[viewKey];
        if (cache.lodViews.size() > 16)
            for (auto it=cache.lodViews.begin();it!=cache.lodViews.end();++it) if (it->first!=viewKey) { cache.lodViews.erase(it);break; }
        std::unordered_set<entt::entity> live;
        const char* disabled = std::getenv("CANIS_DISABLE_LOD");
        const bool enabled = !disabled || std::string(disabled)!="1";
        for (const auto& candidate : cache.candidates)
        {
            auto handle=candidate.handle;live.insert(handle);
            auto& model=registry.get<Model>(handle);
            auto* group=registry.try_get<LODGroup>(handle);
            if(group){group->activeLevel=0;group->projectedHeight=0;}
            if (!group || !group->enabled || !enabled || m_capturingProbe || model.nodeIndex>=0 || candidate.asset->HasDeformingGeometry() || registry.try_get<ModelAnimation>(handle)) continue;
            if(auto* material=registry.try_get<Material>(handle)) {
                if(!material->materialIds.empty())continue;
                if(material->materialId>=0)if(auto* asset=AssetManager::GetMaterial(material->materialId);asset && asset->shaderId>=0)
                    if(auto* shader=AssetManager::Get<ShaderAsset>(asset->shaderId);shader && shader->GetShader()!=m_shader && shader->HasVertexDeformation())continue;
            }
            auto profile=GetLODProfile(group->profilePath);
            if (!profile) continue;
            auto chain=std::find_if(profile->chains.begin(),profile->chains.end(),[&](const auto& source){return source.sourceId==model.modelId;});
            if (chain==profile->chains.end()) continue;
            auto& state=states[handle];
            if(state.source!=model.modelId || state.revision!=profile->revision)state.level=-1;
            auto& transform=registry.get<Transform>(handle);
            Vector3 low,high;
            if (!LODBounds(handle,model,low,high)) continue;
            float height=ProjectedLODHeight(low,high,RenderMatrix(handle,transform),view,projection);
            if(m_stereoFrame)for(int eye=0;eye<2;++eye)
                height=std::max(height,ProjectedLODHeight(low,high,RenderMatrix(handle,transform),m_stereoViews[eye],m_stereoProjections[eye]));
            height*=std::clamp(group->bias,.01f,100.f);
            int chosen=group->forcedLevel>=0 ? std::min(group->forcedLevel,int(chain->levels.size())-1)
                : SelectLOD(chain->levels,height,state.level,group->hysteresis);
            state={chosen,model.modelId,profile->revision};
            group->activeLevel=chosen;group->projectedHeight=height;
            bool culled=group->forcedLevel<0 && group->cullHeight>0 && height<group->cullHeight;
            cache.renderModels[handle]=culled ? -1 : chain->levels[chosen].modelId;
            cache.shadowModels[handle]=chain->levels[std::min(chosen+std::max(0,group->shadowBias),int(chain->levels.size())-1)].modelId;
            cache.levels[handle]=chosen;
            RenderMetrics::LOD(chosen,culled);
        }
        for(auto it=states.begin();it!=states.end();)if(!live.contains(it->first))it=states.erase(it);else++it;
    }

    void MeshRenderer3DSystem::Ready() {}

    void MeshRenderer3DSystem::OnDestroy()
    {
        DestroyLightingProbes();m_probeSettings.clear();m_probesDirty=true;
        m_batchCache.reset();
        if (m_materialSampler) glDeleteSamplers(1, &m_materialSampler);
        m_materialSampler = 0;
        if (m_trailVbo) glDeleteBuffers(1, &m_trailVbo);
        if (m_trailVao) glDeleteVertexArrays(1, &m_trailVao);
        m_trailVao = m_trailVbo = 0;
        m_trailShader = nullptr;
        if (m_skyboxVbo != 0)
            glDeleteBuffers(1, &m_skyboxVbo);
        if (m_skyboxVao != 0)
            glDeleteVertexArrays(1, &m_skyboxVao);
        if (m_colliderDebugVbo != 0)
            glDeleteBuffers(1, &m_colliderDebugVbo);
        if (m_colliderDebugVao != 0)
            glDeleteVertexArrays(1, &m_colliderDebugVao);
        if (m_debugGizmoVbo != 0)
            glDeleteBuffers(1, &m_debugGizmoVbo);
        if (m_debugGizmoVao != 0)
            glDeleteVertexArrays(1, &m_debugGizmoVao);

        DestroyShadowMap();
        if(m_softDepthTexture) glDeleteTextures(1,&m_softDepthTexture);
        m_softDepthTexture=0; m_softDepthWidth=m_softDepthHeight=0;
        m_skyboxVbo = 0;
        m_skyboxVao = 0;
        if (m_cloudNoiseTexture) glDeleteTextures(1, &m_cloudNoiseTexture);
        m_cloudNoiseTexture = 0;
        m_colliderDebugVbo = 0;
        m_colliderDebugVao = 0;
        m_debugGizmoVbo = 0;
        m_debugGizmoVao = 0;
        m_skyboxShader = nullptr;
        m_shadowShader = nullptr;
        m_colliderDebugShader = nullptr;
        m_shader = nullptr;
    }

    void MeshRenderer3DSystem::CreateSkyboxGeometry()
    {
        if (m_skyboxVao == 0)
            glGenVertexArrays(1, &m_skyboxVao);

        if (m_skyboxVbo == 0)
            glGenBuffers(1, &m_skyboxVbo);

        glBindVertexArray(m_skyboxVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kSkyboxVertices), kSkyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void MeshRenderer3DSystem::CreateColliderDebugGeometry()
    {
        if (m_colliderDebugVao == 0)
            glGenVertexArrays(1, &m_colliderDebugVao);

        if (m_colliderDebugVbo == 0)
            glGenBuffers(1, &m_colliderDebugVbo);

        glBindVertexArray(m_colliderDebugVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_colliderDebugVbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void MeshRenderer3DSystem::CreateDebugGizmoGeometry()
    {
        if (m_debugGizmoVao == 0)
            glGenVertexArrays(1, &m_debugGizmoVao);

        if (m_debugGizmoVbo == 0)
            glGenBuffers(1, &m_debugGizmoVbo);

        glBindVertexArray(m_debugGizmoVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_debugGizmoVbo);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void MeshRenderer3DSystem::DrawDebugGizmoLines(const Matrix4 &_projection, const Matrix4 &_view)
    {
        if (scene == nullptr || !scene->HasEditorCamera3DOverride() || m_colliderDebugShader == nullptr)
            return;

        const std::vector<DebugGizmoLine> &debugGizmoLines = scene->GetDebugGizmoLines();
        if (debugGizmoLines.empty())
            return;

        if (m_debugGizmoVao == 0 || m_debugGizmoVbo == 0)
            CreateDebugGizmoGeometry();

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Forward-compatible core contexts only support width 1.
        glLineWidth(1.0f);

        m_colliderDebugShader->Use();
        m_colliderDebugShader->SetMat4("P", _projection);
        m_colliderDebugShader->SetMat4("V", _view);

        glBindVertexArray(m_debugGizmoVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_debugGizmoVbo);

        for (const DebugGizmoLine &line : debugGizmoLines)
        {
            const Vector3 vertices[2] = { line.start, line.end };
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
            m_colliderDebugShader->SetVec4("lineColor", line.color);
            glDrawArrays(GL_LINES, 0, 2);
        }

        m_colliderDebugShader->UnUse();

        glDepthMask(GL_TRUE);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void MeshRenderer3DSystem::DrawColliderDebugLines(entt::registry &_registry, const Matrix4 &_projection, const Matrix4 &_view)
    {
        if (scene == nullptr || !scene->GetShowColliders() || m_colliderDebugShader == nullptr)
            return;

        if (m_colliderDebugVao == 0 || m_colliderDebugVbo == 0)
            CreateColliderDebugGeometry();

        std::vector<Vector3> lines = {};
        lines.reserve(1024);

        auto transformView = _registry.view<Transform>();
        for (const entt::entity entityHandle : transformView)
        {
            Transform &transform = transformView.get<Transform>(entityHandle);
            Entity *entity = transform.entity;
            if (entity == nullptr || !transform.IsActiveInHierarchy())
                continue;

            const Matrix4 modelMatrix = transform.GetModelMatrix();

            if (BoxCollider *boxCollider = _registry.try_get<BoxCollider>(entityHandle))
            {
                if (boxCollider->active)
                {
                    const Vector3 halfSize = boxCollider->size * 0.5f;
                    AppendBoundsBoxLines(lines, modelMatrix, boxCollider->offset - halfSize, boxCollider->offset + halfSize);
                }
            }

            if (SphereCollider *sphereCollider = _registry.try_get<SphereCollider>(entityHandle))
            {
                if (sphereCollider->active)
                {
                    AppendCircleLines(lines, modelMatrix, sphereCollider->offset, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), sphereCollider->radius);
                    AppendCircleLines(lines, modelMatrix, sphereCollider->offset, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), sphereCollider->radius);
                    AppendCircleLines(lines, modelMatrix, sphereCollider->offset, Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), sphereCollider->radius);
                }
            }

            if (CapsuleCollider *capsuleCollider = _registry.try_get<CapsuleCollider>(entityHandle))
            {
                if (capsuleCollider->active)
                {
                    constexpr float pi = 3.14159265359f;
                    const float halfHeight = capsuleCollider->halfHeight;
                    const float radius = capsuleCollider->radius;
                    const Vector3 top = capsuleCollider->offset + Vector3(0.0f, halfHeight, 0.0f);
                    const Vector3 bottom = capsuleCollider->offset + Vector3(0.0f, -halfHeight, 0.0f);

                    AppendCircleLines(lines, modelMatrix, top, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), radius);
                    AppendCircleLines(lines, modelMatrix, bottom, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), radius);

                    const std::array<Vector3, 4> cardinalOffsets = {
                        Vector3(radius, 0.0f, 0.0f),
                        Vector3(-radius, 0.0f, 0.0f),
                        Vector3(0.0f, 0.0f, radius),
                        Vector3(0.0f, 0.0f, -radius)
                    };

                    for (const Vector3 &offset : cardinalOffsets)
                        AppendLine(lines, TransformPoint(modelMatrix, bottom + offset), TransformPoint(modelMatrix, top + offset));

                    AppendArcLines(lines, modelMatrix, top, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), radius, 0.0f, pi);
                    AppendArcLines(lines, modelMatrix, bottom, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), radius, pi, pi * 2.0f);
                    AppendArcLines(lines, modelMatrix, top, Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f), radius, 0.0f, pi);
                    AppendArcLines(lines, modelMatrix, bottom, Vector3(0.0f, 0.0f, 1.0f), Vector3(0.0f, 1.0f, 0.0f), radius, pi, pi * 2.0f);
                }
            }

            if (MeshCollider *meshCollider = _registry.try_get<MeshCollider>(entityHandle))
            {
                if (meshCollider->active)
                {
                    const i32 modelId = ResolveMeshColliderModelId(_registry, entityHandle, meshCollider);
                    ModelAsset *model = AssetManager::GetModel(modelId);
                    if (model != nullptr)
                    {
                        const i32 nodeIndex = ResolveMeshColliderNodeIndex(_registry, entityHandle, meshCollider);
                        const bool applyNodeTransform = ResolveMeshColliderApplyNodeTransform(_registry, entityHandle, meshCollider);
                        std::vector<Vector3> vertices = {};
                        std::vector<u32> indices = {};
                        if (model->BuildTriangleMesh(vertices, indices, nodeIndex, applyNodeTransform))
                            AppendTriangleMeshLines(lines, modelMatrix, vertices, indices);
                    }
                }
            }

            if (ConvexMeshCollider *collider = _registry.try_get<ConvexMeshCollider>(entityHandle))
            {
                if (collider->active)
                {
                    const i32 modelId = ResolveConvexMeshColliderModelId(_registry, entityHandle, collider);
                    ModelAsset *model = AssetManager::GetModel(modelId);
                    if (model != nullptr)
                    {
                        const i32 nodeIndex = ResolveConvexMeshColliderNodeIndex(_registry, entityHandle, collider);
                        const bool applyNodeTransform = ResolveConvexMeshColliderApplyNodeTransform(_registry, entityHandle, collider);
                        std::vector<Vector3> vertices = {};
                        std::vector<u32> indices = {};
                        if (model->BuildTriangleMesh(vertices, indices, nodeIndex, applyNodeTransform))
                        {
                            Matrix4 colliderMatrix = glm::translate(modelMatrix, collider->offset);
                            colliderMatrix = glm::scale(colliderMatrix, collider->scale);
                            AppendConvexHullLines(lines, colliderMatrix, vertices, collider->convexRadius);
                        }
                    }
                }
            }
        }

        if (lines.empty())
            return;

        glBindVertexArray(m_colliderDebugVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_colliderDebugVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(lines.size() * sizeof(Vector3)), lines.data(), GL_DYNAMIC_DRAW);

        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        // Forward-compatible core contexts only support width 1.
        glLineWidth(1.0f);

        m_colliderDebugShader->Use();
        m_colliderDebugShader->SetMat4("P", _projection);
        m_colliderDebugShader->SetMat4("V", _view);
        m_colliderDebugShader->SetVec4("lineColor", Vector4(0.15f, 0.85f, 1.0f, 0.9f));
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(lines.size()));
        m_colliderDebugShader->UnUse();

        glDepthMask(GL_TRUE);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    void MeshRenderer3DSystem::CreateShadowMap()
    {
        if (m_shadowFramebuffer != 0 && m_shadowDepthTexture != 0)
            return;

        if (m_shadowDepthTexture != 0)
        {
            glDeleteTextures(1, &m_shadowDepthTexture);
            m_shadowDepthTexture = 0;
        }

        if (m_shadowFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_shadowFramebuffer);
            m_shadowFramebuffer = 0;
        }

        glGenFramebuffers(1, &m_shadowFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);

        glGenTextures(1, &m_shadowDepthTexture);
        glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT24,
            kDirectionalShadowMapSize,
            kDirectionalShadowMapSize,
            0,
            GL_DEPTH_COMPONENT,
            GL_UNSIGNED_INT,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        #if defined(__EMSCRIPTEN__)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        #else
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        #endif
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_shadowDepthTexture, 0);
        #if defined(__EMSCRIPTEN__)
        const GLenum drawBuffer = GL_NONE;
        glDrawBuffers(1, &drawBuffer);
        #else
        glDrawBuffer(GL_NONE);
        #endif
        glReadBuffer(GL_NONE);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            Debug::Warning("Directional shadow framebuffer incomplete.");

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void MeshRenderer3DSystem::DestroyShadowMap()
    {
        if(m_staticShadowTexture)glDeleteTextures(1,&m_staticShadowTexture);
        if(m_staticShadowFramebuffer)glDeleteFramebuffers(1,&m_staticShadowFramebuffer);
        m_staticShadowTexture=m_staticShadowFramebuffer=0;
        m_staticShadowValid=false;m_staticShadowCasters.clear();
        if (m_shadowDepthTexture != 0)
        {
            glDeleteTextures(1, &m_shadowDepthTexture);
            m_shadowDepthTexture = 0;
        }

        if (m_shadowFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_shadowFramebuffer);
            m_shadowFramebuffer = 0;
        }
    }

    void MeshRenderer3DSystem::RenderDirectionalShadowMap(
        entt::registry &_registry,
        const Matrix4 &_projection,
        const Matrix4 &_view,
        const Vector3 &_cameraPosition,
        float _cameraFarClip,
        const Vector3 &_directionalLightDirection,
        bool _useDirectionalLight)
    {
        RenderMetrics::Scope timing("Directional shadows");
        (void)_projection;
        (void)_view;

        if (m_shadowShader == nullptr || m_shadowFramebuffer == 0 || m_shadowDepthTexture == 0 || !_useDirectionalLight)
        {
            m_shadowLightSpaceMatrix = Matrix4(1.0f);
            return;
        }

        if (!m_shadowShader->IsLinked())
        {
            m_shadowShader->AddAttribute("vertexPosition");
            m_shadowShader->AddAttribute("vertexNormal");
            m_shadowShader->AddAttribute("vertexUV");
            m_shadowShader->Link();
        }

        const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);
        const Vector3 lightUp = (std::abs(glm::dot(_directionalLightDirection, worldUp)) > 0.95f)
            ? Vector3(0.0f, 0.0f, 1.0f)
            : worldUp;
        const float shadowExtent = std::clamp(_cameraFarClip * 0.1f, 20.0f, 120.0f);
        const float shadowDistance = std::max(50.0f, shadowExtent * 2.0f);
        // Snap the camera-centered volume in a fixed light basis. This keeps
        // world-space shadow texels stationary during sub-texel camera motion.
        const Matrix4 lightOrientation = glm::lookAt(Vector3(0.0f), _directionalLightDirection, lightUp);
        Vector3 centerInLight = Vector3(lightOrientation * Vector4(_cameraPosition, 1.0f));
        const float worldUnitsPerTexel = (shadowExtent * 2.0f) / kDirectionalShadowMapSize;
        // Integral texel blocks keep world-space texels aligned while allowing a
        // static shadow bake to survive normal head motion inside an 8m region.
        const float stableStep=std::max(1.f,std::floor(8.f/worldUnitsPerTexel))*worldUnitsPerTexel;
        for(int axis=0;axis<3;++axis)
            centerInLight[axis]=std::round(centerInLight[axis]/stableStep)*stableStep;
        const Vector3 shadowCenter = Vector3(glm::inverse(lightOrientation) * Vector4(centerInLight, 1.0f));
        const Vector3 lightPosition = shadowCenter - (_directionalLightDirection * shadowDistance);

        const Matrix4 lightView = glm::lookAt(lightPosition, shadowCenter, lightUp);
        const Matrix4 lightProjection = glm::ortho(
            -shadowExtent,
            shadowExtent,
            -shadowExtent,
            shadowExtent,
            0.1f,
            shadowDistance * 2.0f);
        m_shadowLightSpaceMatrix = lightProjection * lightView;

        GLint previousViewport[4] = { 0, 0, 0, 0 };
        GLint previousFramebuffer = 0;
        glGetIntegerv(GL_VIEWPORT, previousViewport);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);

        const bool depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
        const bool blendEnabled = glIsEnabled(GL_BLEND);
        const bool cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        const bool polygonOffsetEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);

#ifndef __EMSCRIPTEN__
        const bool shadingRateEnabled = glewGetExtension("GL_NV_shading_rate_image") && glIsEnabled(GL_SHADING_RATE_IMAGE_NV);
        if (shadingRateEnabled) glDisable(GL_SHADING_RATE_IMAGE_NV);
#endif
        const bool scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffer);
        glViewport(0, 0, kDirectionalShadowMapSize, kDirectionalShadowMapSize);
        glClear(GL_DEPTH_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        // Thin/open roof and wall meshes must cast from either side. Front-face
        // culling stores the far side of closed meshes and drops single-sided roofs.
        glDisable(GL_CULL_FACE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        // Cover the depth slope within the bilinear filter footprint.
        glPolygonOffset(1.1f, 1.0f);

        m_shadowShader->Use();
        m_shadowShader->SetMat4("lightSpaceMatrix", m_shadowLightSpaceMatrix);
        RenderMetrics::ShadowPass(true);
        using ShadowKey=std::tuple<i32,i32,bool>;
        std::map<ShadowKey,std::vector<Matrix4>> shadowBatches;
        const bool shadowInstancing=RenderMetrics::InstancingEnabled() &&
            glGetUniformLocation(m_shadowShader->GetProgramID(),"useInstanceMatrix")>=0;

        auto modelView = _registry.view<Transform, Model>();
        auto isStatic=[&](const auto& candidate) {
            const auto& model=_registry.get<Model>(candidate.handle);
            return StaticRenderTransform(_registry, candidate.handle, model) && !candidate.asset->HasDeformingGeometry() &&
                !_registry.try_get<ModelAnimation>(candidate.handle);
        };
        const char* disabledCache=std::getenv("CANIS_DISABLE_STATIC_SHADOW_CACHE");
        bool cacheEnabled=(!scene || scene->GetBakingSettings().staticShadows) && (!disabledCache || std::string(disabledCache)!="1");
        std::vector<StaticShadowCaster> staticCasters;
        for(const auto& candidate:m_batchCache->candidates) {
            const auto& model=_registry.get<Model>(candidate.handle);
            if(model.castShadow && isStatic(candidate))
                staticCasters.push_back({AssetManager::GetModel(ShadowModelId(candidate.handle,model)),AssetManager::GetModel(ShadowModelId(candidate.handle,model))->GetGeometryRevision(),model.nodeIndex,
                    model.applyNodeTransform,RenderMatrix(candidate.handle,_registry.get<Transform>(candidate.handle))});
        }
        if(cacheEnabled && !m_staticShadowTexture) {
            glGenTextures(1,&m_staticShadowTexture);glBindTexture(GL_TEXTURE_2D,m_staticShadowTexture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,kDirectionalShadowMapSize,kDirectionalShadowMapSize,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            glGenFramebuffers(1,&m_staticShadowFramebuffer);glBindFramebuffer(GL_FRAMEBUFFER,m_staticShadowFramebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,m_staticShadowTexture,0);
#if defined(__EMSCRIPTEN__)
            const GLenum cacheDrawBuffer=GL_NONE;glDrawBuffers(1,&cacheDrawBuffer);
#else
            glDrawBuffer(GL_NONE);
#endif
            glReadBuffer(GL_NONE);
            cacheEnabled=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
            glBindFramebuffer(GL_FRAMEBUFFER,m_shadowFramebuffer);
            if(!cacheEnabled) {
                glDeleteTextures(1,&m_staticShadowTexture);m_staticShadowTexture=0;
                glDeleteFramebuffers(1,&m_staticShadowFramebuffer);m_staticShadowFramebuffer=0;
                m_staticShadowValid=false;
            }
        }
        const bool cacheHit=cacheEnabled && m_staticShadowValid && staticCasters==m_staticShadowCasters &&
            m_shadowLightSpaceMatrix==m_staticShadowMatrix && m_staticShadowProgram==m_shadowShader->GetProgramID();
        if(cacheHit) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER,m_staticShadowFramebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_shadowFramebuffer);
            glBlitFramebuffer(0,0,kDirectionalShadowMapSize,kDirectionalShadowMapSize,0,0,kDirectionalShadowMapSize,kDirectionalShadowMapSize,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER,m_shadowFramebuffer);
        }
        m_batchCache->visibility.Query(m_shadowLightSpaceMatrix,m_batchCache->candidates.size(),m_batchCache->hiddenCandidates);
        // Cache contains only static depth; dynamic casters are composed every frame.
        for(int phase=0;phase<2;++phase) {
        if(phase==0 && cacheHit)continue;
        shadowBatches.clear();
        size_t candidateIndex=0;
        for (const auto& candidate : m_batchCache->candidates)
        {
            const bool hidden=m_batchCache->hiddenCandidates[candidateIndex++];
            const auto entityHandle = candidate.handle;
            Transform &transform = modelView.get<Transform>(entityHandle);
            Model &modelRenderer = modelView.get<Model>(entityHandle);
            ModelAsset *model = AssetManager::GetModel(ShadowModelId(entityHandle,modelRenderer));
            if (!modelRenderer.castShadow || isStatic(candidate)!=(phase==0)) continue;

            if (hidden || (candidate.cullable && BoundsOutsideFrustum(m_shadowLightSpaceMatrix*RenderMatrix(entityHandle,transform),candidate.minimum,candidate.maximum))) {
                RenderMetrics::Cull(true);continue;
            }
            if (shadowInstancing && !model->HasDeformingGeometry() &&
                !_registry.try_get<ModelAnimation>(entityHandle)) {
                shadowBatches[{ShadowModelId(entityHandle,modelRenderer),modelRenderer.nodeIndex,modelRenderer.applyNodeTransform}].push_back(RenderMatrix(entityHandle,transform));
                continue;
            }

            const ModelAsset::Pose3D *pose = nullptr;
            if (ModelAnimation *animation = _registry.try_get<ModelAnimation>(entityHandle))
            {
                const bool attachedModelPose =
                    animation->poseModelId == modelRenderer.modelId &&
                    animation->poseGeometryRevision ==
                        model->GetGeometryRevision();
                const bool compatibleSourcePose =
                    animation->sourceModelId >= 0 &&
                    animation->poseModelId == animation->sourceModelId &&
                    animation->pose.localNodeMatrices.size() ==
                        static_cast<std::size_t>(model->GetNodeCount());
                if (animation->poseInitialized &&
                    (attachedModelPose || compatibleSourcePose))
                    pose = &animation->pose;
            }

            model->Draw(
                *m_shadowShader,
                RenderMatrix(entityHandle,transform),
                pose,
                -1,
                Color(1.0f),
                nullptr,
                modelRenderer.nodeIndex,
                modelRenderer.applyNodeTransform);
        }

        for (const auto& [key,matrices]:shadowBatches)
            if(auto* model=AssetManager::GetModel(std::get<0>(key)))
                model->DrawInstanced(*m_shadowShader,matrices,-1,Color(1.f),std::get<1>(key),std::get<2>(key));
        if(phase==0 && cacheEnabled) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER,m_shadowFramebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER,m_staticShadowFramebuffer);
            glBlitFramebuffer(0,0,kDirectionalShadowMapSize,kDirectionalShadowMapSize,0,0,kDirectionalShadowMapSize,kDirectionalShadowMapSize,GL_DEPTH_BUFFER_BIT,GL_NEAREST);
            glBindFramebuffer(GL_FRAMEBUFFER,m_shadowFramebuffer);
            m_staticShadowCasters=staticCasters;m_staticShadowMatrix=m_shadowLightSpaceMatrix;
            m_staticShadowProgram=m_shadowShader->GetProgramID();m_staticShadowValid=true;
        }
        }
        if(!cacheEnabled)m_staticShadowValid=false;
        RenderMetrics::ShadowPass(false);
        m_shadowShader->UnUse();

        if (polygonOffsetEnabled)
            glEnable(GL_POLYGON_OFFSET_FILL);
        else
            glDisable(GL_POLYGON_OFFSET_FILL);

        if (cullFaceEnabled)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);

        if (blendEnabled)
            glEnable(GL_BLEND);
        else
            glDisable(GL_BLEND);

        if (depthTestEnabled)
            glEnable(GL_DEPTH_TEST);
        else
            glDisable(GL_DEPTH_TEST);

        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
        glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
        if (scissorWasEnabled) glEnable(GL_SCISSOR_TEST);
#ifndef __EMSCRIPTEN__
        if (shadingRateEnabled) glEnable(GL_SHADING_RATE_IMAGE_NV);
#endif
    }

    LightingScenarioBlend MeshRenderer3DSystem::RenderLightingScenario() const
    {
        if (m_probeCaptureScenario) return {m_probeCaptureScenario,m_probeCaptureScenario,0};
        return scene->GetLightingScenarioBlend();
    }

    void MeshRenderer3DSystem::CreateCloudNoiseTexture()
    {
        if (m_cloudNoiseTexture) return;
        // A deterministic, seamless density/detail volume is shared on the CPU
        // and uploaded once per renderer. No procedural hash work in ray steps.
        constexpr int side = 64;
        static const auto pixels = [] {
            std::vector<unsigned char> data(side * side * side * 2);
            auto random = [](int x, int y, int z) {
                uint32_t value = uint32_t(x) * 1597334677u ^ uint32_t(y) * 3812015801u ^ uint32_t(z) * 2798796415u;
                value = (value ^ (value >> 16)) * 2246822519u;
                value = (value ^ (value >> 13)) * 3266489917u;
                return float(value ^ (value >> 16)) / 4294967295.f;
            };
            auto noise = [&](int x, int y, int z, int period) {
                const Vector3 p = (Vector3(x, y, z) + Vector3(.5f)) * (float(period) / side);
                const auto base = glm::ivec3(glm::floor(p));
                const Vector3 fraction = glm::fract(p);
                const Vector3 weight = fraction * fraction * (Vector3(3) - 2.f * fraction);
                float value = 0;
                for (int corner = 0; corner < 8; ++corner)
                {
                    const int a = corner & 1, b = (corner >> 1) & 1, c = (corner >> 2) & 1;
                    value += random((base.x + a) % period, (base.y + b) % period, (base.z + c) % period) *
                        (a ? weight.x : 1 - weight.x) * (b ? weight.y : 1 - weight.y) * (c ? weight.z : 1 - weight.z);
                }
                return value;
            };
            for (int z = 0; z < side; ++z) for (int y = 0; y < side; ++y) for (int x = 0; x < side; ++x)
            {
                const size_t offset = size_t((z * side + y) * side + x) * 2;
                const float shape = .55f * noise(x, y, z, 4) + .3f * noise(x, y, z, 8) + .15f * noise(x, y, z, 16);
                data[offset] = static_cast<unsigned char>(glm::clamp(shape * 255.f, 0.f, 255.f));
                data[offset + 1] = static_cast<unsigned char>(glm::clamp(noise(x, y, z, 32) * 255.f, 0.f, 255.f));
            }
            return data;
        }();
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &previousTexture);
        glGenTextures(1, &m_cloudNoiseTexture);
        glBindTexture(GL_TEXTURE_3D, m_cloudNoiseTexture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RG8, side, side, side, 0, GL_RG, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);
        glBindTexture(GL_TEXTURE_3D, previousTexture);
    }

    void MeshRenderer3DSystem::DrawSkybox(const Matrix4 &_projection, const Matrix4 &_view)
    {
        RenderMetrics::Scope timing("Skybox");
        if (m_skyboxShader == nullptr || m_skyboxVao == 0)
            return;

        if (!m_skyboxShader->IsLinked())
        {
            m_skyboxShader->AddAttribute("aPos");
            m_skyboxShader->Link();
        }

        const auto lighting=RenderLightingScenario();
        auto skyAsset=[&](const LightingScenario* scenario) -> SkyboxAsset* {
            const UUID uuid=scenario ? UUID(scenario->skyboxUUID) : scene->GetEnvironmentSkyboxUUID();
            std::string path=scenario ? scenario->skyboxPath : std::string{};
            if (path.empty() && uint64_t(uuid)!=0) path=AssetManager::GetPath(uuid);
            if (path.empty() || path=="Path was not found in AssetLibrary") return nullptr;
            auto* sky=AssetManager::GetSkybox(path);
            return sky && sky->IsLoaded() ? sky : nullptr;
        };
        SkyboxAsset* skybox=skyAsset(lighting.from);
        SkyboxAsset* target=lighting.to ? skyAsset(lighting.to) : skybox;
        auto clouds = scene->GetCloudVolumeSettings();
        clouds.Sanitize();
        const float t = lighting.from && lighting.to ? glm::clamp(lighting.blend, 0.f, 1.f) : 0;
        const float coverage = lighting.from ? glm::mix(lighting.from->cloudCoverage,
            lighting.to ? lighting.to->cloudCoverage : lighting.from->cloudCoverage, t) : .4f;
        const float density = lighting.from ? glm::mix(lighting.from->cloudDensity,
            lighting.to ? lighting.to->cloudDensity : lighting.from->cloudDensity, t) : 1;
        const bool drawClouds = clouds.enabled && coverage > .0001f && density > .0001f;
        if (!lighting.from && !skybox && !target && !drawClouds) return;

        const Matrix4 skyboxView = Matrix4(glm::mat3(_view));

        // The sky runs after opaque depth and before transparent rain. Preserve
        // material samplers/program state when called between those passes.
        GLint samplers[3]{}, previousNoise = 0;
        for (int unit = 0; unit < 3; ++unit) { glGetIntegeri_v(GL_SAMPLER_BINDING, unit, &samplers[unit]); glBindSampler(unit, 0); }
        glActiveTexture(GL_TEXTURE2);
        glGetIntegerv(GL_TEXTURE_BINDING_3D, &previousNoise);
        if (drawClouds) CreateCloudNoiseTexture();
        glBindTexture(GL_TEXTURE_3D, drawClouds ? m_cloudNoiseTexture : 0);
        const bool culling = glIsEnabled(GL_CULL_FACE);
        glDisable(GL_CULL_FACE);

        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);

        m_skyboxShader->Use();
        m_skyboxShader->SetMat4("projection", _projection);
        m_skyboxShader->SetMat4("view", skyboxView);
        m_skyboxShader->SetInt("skybox", 0);
        m_skyboxShader->SetInt("skyboxTarget", 1);
        m_skyboxShader->SetBool("skyboxAvailable",skybox!=nullptr);
        m_skyboxShader->SetBool("skyboxTargetAvailable",target!=nullptr);
        m_skyboxShader->SetFloat("skyboxBlend",lighting.from && lighting.to ? lighting.blend : 0);
        m_skyboxShader->SetFloat("skyboxIntensity",lighting.from ? lighting.from->skyboxIntensity : 1);
        m_skyboxShader->SetFloat("skyboxTargetIntensity",lighting.to ? lighting.to->skyboxIntensity : 1);
        m_skyboxShader->SetBool("canisProbeCapture",m_capturingProbe);
        m_skyboxShader->SetBool("cloudsEnabled", drawClouds);
        m_skyboxShader->SetInt("cloudNoise", 2);
        m_skyboxShader->SetVec3("cloudCameraPosition", Vector3(glm::inverse(_view)[3]));
        m_skyboxShader->SetVec3("cloudBoundsMin", Vector3(clouds.center.x - clouds.halfExtents.x,
            clouds.baseHeight, clouds.center.y - clouds.halfExtents.y));
        m_skyboxShader->SetVec3("cloudBoundsMax", Vector3(clouds.center.x + clouds.halfExtents.x,
            clouds.baseHeight + clouds.thickness, clouds.center.y + clouds.halfExtents.y));
        m_skyboxShader->SetFloat("cloudNoiseScale", clouds.noiseScale);
        const double cloudTime = m_capturingProbe ? 0 : scene->GetCloudSimulationTime();
        // Keep drift bounded without a discontinuity: the volume repeats exactly
        // every noise period. Double precision keeps long-running sessions stable.
        const double period = 1.0 / double(clouds.noiseScale);
        m_skyboxShader->SetVec2("cloudWindOffset", Vector2(std::fmod(cloudTime * clouds.windVelocity.x, period),
            std::fmod(cloudTime * clouds.windVelocity.y, period)));
        m_skyboxShader->SetInt("cloudSteps", clouds.steps);
        m_skyboxShader->SetFloat("cloudCoverage", glm::clamp(coverage, 0.f, 1.f));
        m_skyboxShader->SetFloat("cloudDensity", glm::clamp(density, 0.f, 8.f));
        m_skyboxShader->SetFloat("cloudExtinction", clouds.extinction);
        auto sunlight = GatherDirectionalLight(scene->GetRegistry());
        Vector3 ambient = Vector3(scene->GetEnvironmentAmbientLight()) * scene->GetEnvironmentAmbientLightIntensity();
        if (lighting.from) {
            const auto& a = *lighting.from;
            const auto& b = lighting.to ? *lighting.to : a;
            sunlight.direction = glm::mix(a.sunDirection, b.sunDirection, t);
            sunlight.color = glm::mix(Vector3(a.sunColor), Vector3(b.sunColor), t);
            sunlight.intensity = glm::mix(a.sunEnabled ? a.sunIntensity : 0.f, b.sunEnabled ? b.sunIntensity : 0.f, t);
            ambient = glm::mix(Vector3(a.ambientLight) * a.ambientIntensity, Vector3(b.ambientLight) * b.ambientIntensity, t);
        }
        if (glm::length(sunlight.direction) < .0001f) sunlight.direction = Vector3(0, -1, 0);
        m_skyboxShader->SetVec3("cloudLightDirection", -glm::normalize(sunlight.direction));
        m_skyboxShader->SetVec3("cloudLightColor", sunlight.color * (sunlight.enabled || lighting.from ? sunlight.intensity : 0.f));
        m_skyboxShader->SetVec3("cloudAmbientColor", glm::max(ambient, Vector3(.008f, .012f, .02f)));

        glBindVertexArray(m_skyboxVao);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skybox ? skybox->GetTexture() : 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP,target ? target->GetTexture() : 0);
        glActiveTexture(GL_TEXTURE0);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glBindVertexArray(0);
        m_skyboxShader->UnUse();

        glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_3D, previousNoise);
        for (int unit = 0; unit < 3; ++unit) glBindSampler(unit, samplers[unit]);
        glActiveTexture(GL_TEXTURE0);
        if (culling) glEnable(GL_CULL_FACE);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);
    }

    void MeshRenderer3DSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        if (m_shader == nullptr)
            return;
        // Keep probe texture units private, including nested cubemap renders.
        struct ProbeBindingScope {
            GLint active=0, textures[6]{}, samplers[6]{}, arrays[6]{}, lightingBuffer=0, lightingSampler=0;
            int slots=6;
            ProbeBindingScope() {
#if !defined(__EMSCRIPTEN__)
                if(GLEW_VERSION_4_0 || GLEW_ARB_texture_cube_map_array)slots=1;
#endif
                glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
                for(int i=0;i<slots;++i) {
                    glActiveTexture(GL_TEXTURE8+i);glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP,&textures[i]);
#if !defined(__EMSCRIPTEN__)
                    if(GLEW_VERSION_4_0 || GLEW_ARB_texture_cube_map_array)glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP_ARRAY,&arrays[i]);
#endif
                    glGetIntegeri_v(GL_SAMPLER_BINDING,8+i,&samplers[i]);
                }
#if !defined(__EMSCRIPTEN__)
                glActiveTexture(GL_TEXTURE14);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&lightingBuffer);
                glGetIntegeri_v(GL_SAMPLER_BINDING,14,&lightingSampler);
#endif
                glActiveTexture(active);
            }
            ~ProbeBindingScope() {
                for(int i=0;i<slots;++i) {
                    glActiveTexture(GL_TEXTURE8+i);glBindTexture(GL_TEXTURE_CUBE_MAP,textures[i]);
#if !defined(__EMSCRIPTEN__)
                    if(GLEW_VERSION_4_0 || GLEW_ARB_texture_cube_map_array)glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY,arrays[i]);
#endif
                    glBindSampler(8+i,samplers[i]);
                }
#if !defined(__EMSCRIPTEN__)
                glActiveTexture(GL_TEXTURE14);glBindTexture(GL_TEXTURE_BUFFER,lightingBuffer);glBindSampler(14,lightingSampler);
#endif
                glActiveTexture(active);
            }
        } probeBindings;
        if(!m_capturingProbe)UpdateLightingProbes(_registry);
        // Smooth magnification and trilinear minification for 3D materials.
        // Samplers leave shared texture objects (including pixel-art UI) unchanged.
        if (!m_materialSampler)
        {
            glGenSamplers(1, &m_materialSampler);
            glSamplerParameteri(m_materialSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glSamplerParameteri(m_materialSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(m_materialSampler, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glSamplerParameteri(m_materialSampler, GL_TEXTURE_WRAP_T, GL_REPEAT);
#ifndef __EMSCRIPTEN__
            if (GLEW_EXT_texture_filter_anisotropic)
            {
                GLfloat maximum = 1.0f;
                glGetFloatv(0x84FF /* GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT */, &maximum);
                glSamplerParameterf(m_materialSampler, 0x84FE /* GL_TEXTURE_MAX_ANISOTROPY_EXT */, std::min(maximum, 8.0f));
            }
#endif
        }
        struct MaterialSamplerScope
        {
            GLint previous[6]{};
            GLint previousShadowSampler = 0;
            explicit MaterialSamplerScope(GLuint sampler)
            {
                // Imported normal maps occupy unit 5 and need the same smooth
                // filtering as albedo/PBR maps, especially on wet highlights.
                for (GLuint unit = 0; unit < 6; ++unit)
                {
                    glGetIntegeri_v(GL_SAMPLER_BINDING, unit, &previous[unit]);
                    glBindSampler(unit, sampler);
                }
                glGetIntegeri_v(GL_SAMPLER_BINDING, 6, &previousShadowSampler);
                glBindSampler(6, 0);
            }
            ~MaterialSamplerScope()
            {
                for (GLuint unit = 0; unit < 6; ++unit)
                    glBindSampler(unit, static_cast<GLuint>(previous[unit]));
                glBindSampler(6, static_cast<GLuint>(previousShadowSampler));
            }
        };
        if(!m_batchCache)m_batchCache=std::make_shared<MeshRendererCache>();
        // Scene simulation and bone attachments finish before BeginStereoFrame.
        // Freeze shared eligibility and world transforms for the two eye submissions;
        // visibility, distance sorting, and shader state remain per-view.
        if (!m_stereoFrame || !m_stereoPrepared) {
            m_effectTime += _deltaTime;
            Profiler::Scope preparation("Render frame preparation", RenderMetrics::ProfileCategory());
            auto& cache = *m_batchCache;
            ++cache.preparationFrame;
            cache.candidates.clear();
            cache.staticBounds.clear();
            auto models = _registry.view<Transform, Model>();
            cache.candidates.reserve(models.size_hint());
            for (auto handle : models) {
                auto& transform = models.get<Transform>(handle);
                auto& model = models.get<Model>(handle);
                if(m_capturingProbe && !StaticRenderTransform(_registry, handle, model))continue;
                Entity* entity = model.entity ? model.entity : transform.entity;
                if ((entity && !entity->Active()) || !transform.IsActiveInHierarchy() || model.modelId < 0) continue;
                if (auto* blockout = _registry.try_get<BlockoutShape>(handle))
                    if (!blockout->active || (!blockout->visibleInGame && !scene->HasEditorCamera3DOverride())) continue;
                auto* asset = AssetManager::GetModel(model.modelId);
                if (!asset) continue;
                const auto& matrix=RenderMatrix(handle, transform);
                Vector3 minimum{0},maximum{0};
                const bool cullable=CullBounds(_registry,handle,*asset,model,m_shader,minimum,maximum);
                if(cullable)LODBounds(handle,model,minimum,maximum);
                // Moving rigid models use per-view bounds tests. Keep the spatial
                // tree static so one moving actor cannot rebuild the city BVH.
                if(cullable && !asset->HasDeformingGeometry() && !_registry.try_get<ModelAnimation>(handle) && StaticRenderTransform(_registry, handle, model)) {
                    auto& record=cache.frameMatrices.at(handle);
                    if(record.boundsRevision!=record.revision || record.localMinimum!=minimum || record.localMaximum!=maximum) {
                        Vector3 lo(FLT_MAX),hi(-FLT_MAX);
                        bool finite=matrix[0][3]==0 && matrix[1][3]==0 && matrix[2][3]==0 && matrix[3][3]==1;
                        for(int corner=0;corner<8;++corner) {
                            Vector3 point(matrix*Vector4(corner&1?maximum.x:minimum.x,corner&2?maximum.y:minimum.y,corner&4?maximum.z:minimum.z,1));
                            finite=finite && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
                            lo=glm::min(lo,point);hi=glm::max(hi,point);
                        }
                        record.boundsRevision=record.revision;
                        record.localMinimum=minimum;record.localMaximum=maximum;
                        record.worldMinimum=lo-Vector3(.01f);record.worldMaximum=hi+Vector3(.01f);
                        record.finiteBounds=finite;
                    }
                    if(record.finiteBounds)cache.staticBounds.push_back({cache.candidates.size(),record.worldMinimum,record.worldMaximum});
                }
                cache.candidates.push_back({handle, asset, EntityUsesTransparency(_registry, handle),cullable,minimum,maximum});
            }
            cache.visibility.Update(cache.staticBounds);
            // Bound memory across entity churn, including generation-bearing handles.
            for (auto it = cache.frameMatrices.begin(); it != cache.frameMatrices.end();)
                if (it->second.frame != cache.preparationFrame) it = cache.frameMatrices.erase(it);
                else ++it;
            m_stereoPrepared = m_stereoFrame;
        }

        if (!m_shader->IsLinked())
        {
            m_shader->AddAttribute("vertexPosition");
            m_shader->AddAttribute("vertexNormal");
            m_shader->AddAttribute("vertexUV");
            m_shader->Link();
        }

        Matrix4 projection = Matrix4(1.0f);
        Matrix4 view = Matrix4(1.0f);
        Vector3 cameraPosition = Vector3(0.0f, 0.0f, 0.0f);
        uint64_t selectedCameraKey=0;
        float cameraNearClip = 0.1f;
        float cameraFarClip = 100.0f;

        if(m_capturingProbe) {
            projection=m_probeProjection;view=m_probeView;cameraPosition=m_probePosition;
            cameraNearClip=m_probeNear;cameraFarClip=m_probeFar;
        }
        else if (scene->GetVRCamera().enabled)
        {
            const auto& camera = scene->GetVRCamera();
            projection = camera.projection;
            view = camera.view;
            cameraNearClip = camera.nearClip;
            cameraFarClip = camera.farClip;
            cameraPosition = Vector3(glm::inverse(view)[3]);
        }
        else if (scene->HasEditorCamera3DOverride())
        {
            projection = scene->GetEditorCamera3DProjection();
            view = scene->GetEditorCamera3DView();
            const Matrix4 invView = glm::inverse(view);
            cameraPosition = Vector3(invView[3][0], invView[3][1], invView[3][2]);
            cameraNearClip = 0.05f;
            cameraFarClip = 2000.0f;
        }
        else
        {
            Camera *camera = nullptr;
            Transform *cameraTransform = nullptr;

            auto cameraView = _registry.view<Camera, Transform>();
            for (const entt::entity entityHandle : cameraView)
            {
                Camera &candidateCamera = cameraView.get<Camera>(entityHandle);
                Transform &candidateTransform = cameraView.get<Transform>(entityHandle);

                Entity *entity = candidateCamera.entity;
                if (entity == nullptr)
                    entity = candidateTransform.entity;

                if (entity == nullptr || !candidateTransform.IsActiveInHierarchy())
                    continue;

                if (candidateCamera.primary)
                {
                    camera = &candidateCamera;
                    cameraTransform = &candidateTransform;
                    selectedCameraKey=uint64_t(entt::to_integral(entityHandle))+1;
                    break;
                }

                if (camera == nullptr)
                {
                    camera = &candidateCamera;
                    cameraTransform = &candidateTransform;
                    selectedCameraKey=uint64_t(entt::to_integral(entityHandle))+1;
                }
            }

            if (camera == nullptr || cameraTransform == nullptr)
            {
                scene->ClearLastRenderCamera();
                return;
            }

            const float aspect = (window->GetScreenHeight() > 0)
                ? (static_cast<float>(window->GetScreenWidth()) / static_cast<float>(window->GetScreenHeight()))
                : 1.0f;
            projection = glm::perspective(DEG2RAD * camera->fovDegrees, aspect, camera->nearClip, camera->farClip);
            cameraNearClip = camera->nearClip;
            cameraFarClip = camera->farClip;

            const Vector3 eye = cameraTransform->GetGlobalPosition();
            const Vector3 target = eye + cameraTransform->GetForward();
            const Vector3 up = cameraTransform->GetUp();
            view = glm::lookAt(eye, target, up);
            cameraPosition = eye;
        }

        uint64_t lodViewKey=scene->HasEditorCamera3DOverride() ? UINT64_MAX-1 : selectedCameraKey;
        if(m_stereoFrame)lodViewKey=UINT64_MAX-2;
        SelectLODs(_registry,view,projection,lodViewKey);
        if(!m_capturingProbe)scene->SetLastRenderCamera(view, projection, cameraPosition, cameraNearClip, cameraFarClip);

        const auto lighting=RenderLightingScenario();
        DirectionalLightState directionalLight = GatherDirectionalLight(_registry);
        if (lighting.from && lighting.to) {
            const auto& a=*lighting.from; const auto& b=*lighting.to;
            const float t=lighting.blend;
            const Vector3 direction=glm::mix(a.sunDirection,b.sunDirection,t);
            directionalLight.direction=glm::length(direction)>.0001f ? glm::normalize(direction) : Vector3(0,-1,0);
            directionalLight.color=glm::mix(Vector3(a.sunColor),Vector3(b.sunColor),t);
            directionalLight.intensity=glm::mix(a.sunEnabled?a.sunIntensity:0.f,b.sunEnabled?b.sunIntensity:0.f,t);
            directionalLight.enabled=directionalLight.intensity>0;
            directionalLight.mode=LightMode::Baked;
        }
        std::vector<PointLightState> pointLights = GatherPointLights(_registry);
        const bool directionalShadows=directionalLight.enabled && directionalLight.mode!=LightMode::Baked;

        const bool stereo = m_stereoFrame && scene->GetVRCamera().enabled;
        if (!stereo || !m_stereoShadowReady)
        {
            RenderDirectionalShadowMap(
                _registry,
                projection,
                view,
                stereo ? m_stereoShadowCenter : cameraPosition,
                cameraFarClip,
                directionalLight.direction,
                directionalShadows);
            if (stereo) m_stereoShadowReady = true;
        }

#ifndef __EMSCRIPTEN__
        if (m_stereoPass == 1) {
            // glViewport in the shared shadow pass resets ALL indexed viewports.
            const GLfloat w=float(window->GetScreenWidth()), h=float(window->GetScreenHeight());
            const GLfloat viewports[]={0,0,w,h,w,0,w,h};
            glViewportArrayv(0,2,viewports);
        }
#endif
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        MaterialSamplerScope materialSamplers(m_materialSampler);

        Shader *currentShader = nullptr;
        bool softDepthReady = false;
        GLint softDepthViewport[4]{};

        auto modelView = _registry.view<Transform, Model>();
        auto& opaqueEntities=m_batchCache->opaqueScratch;
        auto& transparentEntities=m_batchCache->transparentScratch;
        opaqueEntities.clear();transparentEntities.clear();
        opaqueEntities.reserve(modelView.size_hint());
        transparentEntities.reserve(modelView.size_hint());

        const Matrix4 clip=projection*view;
        m_batchCache->visibility.Query(clip,m_batchCache->candidates.size(),m_batchCache->hiddenCandidates);
        size_t candidateIndex=0;
        for (const auto& candidate : m_batchCache->candidates)
        {
            const bool hidden=m_batchCache->hiddenCandidates[candidateIndex++];
            const auto entityHandle = candidate.handle;
            Transform &transform = modelView.get<Transform>(entityHandle);
            Model &modelRenderer = modelView.get<Model>(entityHandle);
            int visualId=RenderModelId(entityHandle,modelRenderer);
            if(visualId<0){RenderMetrics::Cull(false);continue;}
            ModelAsset *model = AssetManager::GetModel(visualId);

            if ((m_stereoPass == 1 && candidate.transparent) ||
                (m_stereoPass == 2 && !candidate.transparent)) continue;
            // A center/left-eye visibility query is not a conservative stereo frustum.
            // Submit a superset in the experimental pass; per-eye remainder keeps normal culling.
            if (m_stereoPass != 1 && (hidden || (candidate.cullable && BoundsOutsideFrustum(clip*RenderMatrix(entityHandle,transform),candidate.minimum,candidate.maximum)))) {
                RenderMetrics::Cull(false);continue;
            }

            const Vector3 offset = Vector3(RenderMatrix(entityHandle,transform)[3]) - cameraPosition;
            const float distanceSquared = glm::dot(offset, offset);

            if (candidate.transparent)
            {
                transparentEntities.push_back(TransparentModelEntry{
                    .entityHandle = entityHandle,
                    .distanceSquared = distanceSquared
                });
            }
            else
            {
                opaqueEntities.push_back(TransparentModelEntry{
                    .entityHandle = entityHandle,
                    .distanceSquared = distanceSquared
                });
            }
        }

        // Preserve registry order for static batches; camera motion must not
        // reorder and re-upload otherwise unchanged instance buffers.

        std::sort(
            transparentEntities.begin(),
            transparentEntities.end(),
            [](const TransparentModelEntry &_a, const TransparentModelEntry &_b)
            {
                return _a.distanceSquared > _b.distanceSquared;
            });

        auto wetness = scene->GetSurfaceWetnessSettings();
        wetness.Sanitize();
        // Reflection captures describe the stable dry environment. Surface
        // wetting remains a cheap shading change while those captures blend.
        if (m_capturingProbe) wetness.amount = 0.f;
        auto useShaderForMaterial = [&](MaterialAsset *_materialAsset) -> Shader*
        {
            Shader *activeShader = m_shader;
            if (_materialAsset != nullptr && _materialAsset->shaderId >= 0)
            {
                if (ShaderAsset *shaderAsset = AssetManager::Get<ShaderAsset>(_materialAsset->shaderId))
                {
                    if (!shaderAsset->GetShader()->IsLinked())
                        shaderAsset->GetShader()->Link();
                    activeShader = shaderAsset->GetShader();
                }
            }

            if (m_stereoPass == 1) {
                activeShader = activeShader->StereoVariant();
                if (!activeShader) throw std::runtime_error("unsupported opaque shader interface");
            }
            if (currentShader != activeShader)
            {
                if (currentShader != nullptr)
                    currentShader->UnUse();

                currentShader = activeShader;
                currentShader->Use();
                BindLightingProbes(*currentShader);
                currentShader->SetBool("disableBakedLighting", !scene->GetBakingSettings().lightmaps);
                currentShader->SetMat4("P", projection);
                currentShader->SetMat4("V", view);
                if (m_stereoPass == 1) {
                    for(int eye=0;eye<2;++eye) {
                        currentShader->SetMat4("stereoView["+std::to_string(eye)+"]",m_stereoViews[eye]);
                        currentShader->SetMat4("stereoProjection["+std::to_string(eye)+"]",m_stereoProjections[eye]);
                        currentShader->SetVec3("stereoEyes["+std::to_string(eye)+"]",m_stereoEyes[eye]);
                    }
                }
                currentShader->SetVec3("cameraPosition", cameraPosition);
                currentShader->SetFloat("softTime", m_effectTime);
                currentShader->SetBool("weatherWetnessEnabled", wetness.enabled);
                currentShader->SetFloat("weatherWetness", wetness.amount);
                currentShader->SetFloat("weatherDryRoughness", wetness.dryRoughness);
                currentShader->SetFloat("weatherWetRoughness", wetness.wetRoughness);
                currentShader->SetFloat("weatherDarkening", wetness.darkening);
                if(currentShader->GetUniformLocation("softSceneDepth",0)>=0) {
                    currentShader->SetBool("softDepthAvailable",softDepthReady);
                    currentShader->SetInt("softSceneDepth",15);
                    currentShader->SetVec4("softDepthViewport",Vector4(softDepthViewport[0],softDepthViewport[1],softDepthViewport[2],softDepthViewport[3]));
                    currentShader->SetMat4("softInverseProjection",glm::inverse(projection));
                }
                const Color ambientLight = lighting.from && lighting.to ?
                    Color(glm::mix(Vector3(lighting.from->ambientLight)*lighting.from->ambientIntensity,
                                   Vector3(lighting.to->ambientLight)*lighting.to->ambientIntensity,lighting.blend),1) : scene->GetEnvironmentAmbientLight();
                currentShader->SetVec3("ambientLightColor", ambientLight.r, ambientLight.g, ambientLight.b);
                currentShader->SetFloat("ambientLightIntensity", lighting.from && lighting.to ?
                    1.f : scene->GetEnvironmentAmbientLightIntensity());

                currentShader->SetBool("canisDirectCapture", m_capturingProbe);
                currentShader->SetInt("directionalLightMode", directionalLight.mode);
                currentShader->SetBool("useDirectionalLight", directionalLight.enabled);
                currentShader->SetVec3("directionalLightDirection", directionalLight.direction);
                currentShader->SetVec3("directionalLightColor", directionalLight.color);
                currentShader->SetFloat("directionalLightIntensity", directionalLight.intensity);
                currentShader->SetBool("useDirectionalShadow", directionalShadows && m_shadowDepthTexture != 0);
                currentShader->SetMat4("directionalLightSpaceMatrix", m_shadowLightSpaceMatrix);
                // Material maps occupy 0-5, embedded emission uses 7 and probes
                // use 8-11. Normal maps must never replace the shadow texture.
                currentShader->SetInt("directionalShadowMap", 6);
                glActiveTexture(GL_TEXTURE6);
                glBindTexture(GL_TEXTURE_2D, m_shadowDepthTexture);
                // Raw depth/manual PCF and hardware comparison samplers require
                // different texture state. Mixing them produces undefined reads.
                const bool comparison = currentShader->GetUniformType("directionalShadowMap") == GL_SAMPLER_2D_SHADOW;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, comparison ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, comparison ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, comparison ? GL_LINEAR : GL_NEAREST);

                currentShader->SetInt("pointLightCount", static_cast<int>(pointLights.size()));
                for (size_t lightIndex = 0; lightIndex < pointLights.size(); ++lightIndex)
                {
                    const PointLightState &light = pointLights[lightIndex];
                    const std::string indexString = std::to_string(lightIndex);
                    currentShader->SetVec3("pointLightPositions[" + indexString + "]", light.position);
                    currentShader->SetVec3("pointLightColors[" + indexString + "]", light.color);
                    currentShader->SetFloat("pointLightIntensities[" + indexString + "]", light.intensity);
                    currentShader->SetFloat("pointLightRanges[" + indexString + "]", light.range);
                    currentShader->SetInt("pointLightModes[" + indexString + "]", light.mode);
                    currentShader->SetVec3("pointLightDirections[" + indexString + "]", light.direction);
                    currentShader->SetVec2("pointLightCones[" + indexString + "]", light.cone);
                }
            }

            return currentShader;
        };

        const int batchTiming=Profiler::Get().Begin("Render batch validation", RenderMetrics::ProfileCategory(),Profiler::Now());
        const char* disableRooms=std::getenv("CANIS_DISABLE_ROOM_CULLING");
        bool roomQueries=false;
#ifndef __EMSCRIPTEN__
        roomQueries=!m_capturingProbe && m_stereoPass == 0 && !scene->HasEditorCamera3DOverride() && m_shadowShader && m_skyboxVao &&
            (!disableRooms || std::string(disableRooms)!="1") && (GLEW_VERSION_3_3 || GLEW_ARB_occlusion_query2);
#endif
        std::vector<MeshRendererCache::Input> batchInputs;
        batchInputs.reserve(opaqueEntities.size());
        // Transient rigid batches are separate from cached static batches:
        // moving a robot must not invalidate the city's instance buffers.
        std::map<StaticModelBatchKey, StaticModelBatch> dynamicBatches;
        std::vector<TransparentModelEntry> remainingOpaqueEntities = {};
        remainingOpaqueEntities.reserve(opaqueEntities.size());
        std::unordered_map<entt::entity,uint64_t> groupMembership;
        if(roomQueries)groupMembership.reserve(opaqueEntities.size());
        auto findGroup=[&](auto&& self,entt::entity handle)->uint64_t {
            if(auto found=groupMembership.find(handle);found!=groupMembership.end())return found->second;
            uint64_t groupId=0;
            if(auto* group=_registry.try_get<RenderVisibilityGroup>(handle);group && group->enabled)
                groupId=uint64_t(Entity(*scene,handle).GetUUID());
            else if(auto* t=_registry.try_get<Transform>(handle);t && t->parent)
                groupId=self(self,t->parent.GetHandle());
            groupMembership.emplace(handle,groupId);
            return groupId;
        };

        const char* disableMerge = std::getenv("CANIS_DISABLE_STATIC_MERGING");
        const bool mergeEnabled = !m_capturingProbe && m_stereoPass != 1 && RenderMetrics::BatchCacheEnabled() &&
            (!disableMerge || std::string(disableMerge) != "1");
        std::unordered_set<entt::entity> visibleOpaque;
        for (const auto& entry : opaqueEntities) visibleOpaque.insert(entry.entityHandle);
        std::vector<TransparentModelEntry> batchEntities;
        if (mergeEnabled)
        {
            // Include hidden stationary sources when preparing shared buffers.
            // Turning the camera then changes index ranges, never vertex uploads.
            for (const auto& candidate : m_batchCache->candidates)
            {
                if (candidate.transparent) continue;
                const auto handle = candidate.handle;
                const auto& model = modelView.get<Model>(handle);
                if (!visibleOpaque.contains(handle) && !StaticRenderTransform(_registry, handle, model)) continue;
                if (RenderModelId(handle, model) < 0) continue;
                const auto offset = Vector3(RenderMatrix(handle, modelView.get<Transform>(handle))[3]) - cameraPosition;
                batchEntities.push_back({handle, glm::dot(offset, offset)});
            }
        }
        else batchEntities = opaqueEntities;
        std::vector<MeshRendererCache::Input> mergeInputs;
        for (const TransparentModelEntry &entry : batchEntities)
        {
            const bool visible = visibleOpaque.contains(entry.entityHandle);
            Transform &transform = modelView.get<Transform>(entry.entityHandle);
            Model &modelRenderer = modelView.get<Model>(entry.entityHandle);
            Material *material = _registry.try_get<Material>(entry.entityHandle);
            MaterialAsset *materialAsset = nullptr;
            const i32 materialId = (material != nullptr) ? material->materialId : -1;
            if (materialId >= 0)
                materialAsset = AssetManager::GetMaterial(materialId);

            bool usesInstancedModelShader = true;
            bool graph=false;
            bool mergeSafe=true;
            int program=m_shader->GetProgramID();
            if (materialAsset != nullptr && materialAsset->shaderId >= 0)
            {
                usesInstancedModelShader = false;
                if (ShaderAsset *shaderAsset = AssetManager::Get<ShaderAsset>(materialAsset->shaderId)) {
                    program=shaderAsset->GetShader()->GetProgramID();
                    mergeSafe=shaderAsset->GetShader()==m_shader || shaderAsset->GetShader()->SupportsStaticBatching();
                    graph=shaderAsset->SupportsGraphInstancing();
                    usesInstancedModelShader = graph || shaderAsset->GetShader() == m_shader ||
                        shaderAsset->GetShader()->SupportsStaticBatching();
                }
            }

            const bool canBatch = !modelRenderer.bakedVisibility &&
                RenderMetrics::InstancingEnabled() &&
                !_registry.try_get<Terrain>(entry.entityHandle) &&
                !AssetManager::GetModel(RenderModelId(entry.entityHandle,modelRenderer))->HasDeformingGeometry() &&
                usesInstancedModelShader &&
                _registry.try_get<ModelAnimation>(entry.entityHandle) == nullptr &&
                (material == nullptr || (material->materialIds.empty() && !HasMaterialFields(material->materialFields))) &&
                (materialAsset == nullptr || graph || !HasMaterialFields(materialAsset->materialFields));

            if (!canBatch)
            {
                if (visible) remainingOpaqueEntities.push_back(entry);
                continue;
            }

            const Color modelColor = graph ? Color(1.f) : LODColor(entry.entityHandle,modelRenderer.color);
            const Color materialColor = (!graph && material != nullptr) ? material->color : Color(1.0f);
            StaticModelBatchKey key = {};
            key.modelId = RenderModelId(entry.entityHandle,modelRenderer);
            if(auto level=m_batchCache->levels.find(entry.entityHandle);level!=m_batchCache->levels.end())key.lodLevel=level->second;
            key.materialId = materialId;
            key.nodeIndex = modelRenderer.nodeIndex;
            key.applyNodeTransform = modelRenderer.applyNodeTransform;
            key.weatherSurface = modelRenderer.weatherSurface;
            key.modelColorR = modelColor.r;
            key.modelColorG = modelColor.g;
            key.modelColorB = modelColor.b;
            key.modelColorA = modelColor.a;
            key.materialColorR = materialColor.r;
            key.materialColorG = materialColor.g;
            key.materialColorB = materialColor.b;
            key.materialColorA = materialColor.a;
            const bool stationary = StaticRenderTransform(_registry, entry.entityHandle, modelRenderer);
            if ((roomQueries || mergeEnabled) && stationary) key.group=findGroup(findGroup,entry.entityHandle);

            if (!stationary)
            {
                auto& batch = dynamicBatches[key];
                batch.key = key;
                batch.graph = graph;
                batch.modelMatrices.push_back(RenderMatrix(entry.entityHandle, transform));
                batch.colors.push_back(LODColor(entry.entityHandle, modelRenderer.color) *
                    (material ? material->color : Color(1.f)));
                batch.sourceEntries.push_back(entry);
                continue;
            }

            auto& destination = mergeEnabled && !graph && mergeSafe
                ? mergeInputs : batchInputs;
            if (&destination == &batchInputs && !visible) continue;
            destination.push_back({key,RenderMatrix(entry.entityHandle,transform),
                LODColor(entry.entityHandle,modelRenderer.color) * (material ? material->color : Color(1.f)),entry.entityHandle,graph,
                AssetManager::GetModel(key.modelId)->GetGeometryRevision(),program,
                materialAsset ? uint64_t(materialAsset->info) : 0,
                materialAsset ? materialAsset->albedoId : -1,
                materialAsset ? materialAsset->color.a : 1.f});
        }

        // Repeated assets retain instancing. Unique baked meshes share buffers
        // by material and 32 m cell; their individual culling results select ranges.
        std::unordered_map<int, size_t> modelUses;
        for (const auto& input : mergeInputs) ++modelUses[input.key.modelId];
        std::map<MeshRendererCache::MergeKey, std::vector<MeshRendererCache::Input>> chunkInputs;
        for (const auto& input : mergeInputs)
        {
            if (modelUses[input.key.modelId] > 1)
            {
                if (visibleOpaque.contains(input.entity)) batchInputs.push_back(input);
                continue;
            }
            auto key = input.key;
            key.modelId = -1;
            key.nodeIndex = -1;
            key.applyNodeTransform = true;
            const auto& transformRecord = m_batchCache->frameMatrices.at(input.entity);
            const Vector3 center = transformRecord.finiteBounds
                ? (transformRecord.worldMinimum + transformRecord.worldMaximum) * .5f : Vector3(input.matrix[3]);
            chunkInputs[{int(std::floor(center.x / 32)), int(std::floor(center.y / 32)),
                         int(std::floor(center.z / 32)), key}].push_back(input);
        }
        auto& mergedViews = m_batchCache->mergedViews;
        if (!mergedViews.contains(lodViewKey) && mergedViews.size() >= 4)
            mergedViews.erase(mergedViews.begin());
        auto& chunks = mergedViews[lodViewKey];
        for (auto it = chunks.begin(); it != chunks.end();)
            if (!chunkInputs.contains(it->first)) it = chunks.erase(it);
            else ++it;
        for (auto& [key, inputs] : chunkInputs)
        {
            auto& chunkPointer = chunks[key];
            if (!chunkPointer) chunkPointer = std::make_unique<MeshRendererCache::MergedChunk>();
            auto& chunk = *chunkPointer;
            if (chunk.inputs != inputs)
            {
                chunk.geometry.Free();
                chunk.inputs = inputs;
                std::vector<ModelAsset::StaticSource3D> sources;
                for (const auto& input : inputs)
                    sources.push_back({AssetManager::GetModel(input.key.modelId), input.matrix,
                                       input.key.nodeIndex, input.key.applyNodeTransform});
                chunk.valid = inputs.size() > 1 && chunk.geometry.BuildStaticBatch(sources, chunk.sourceRanges);
                chunk.visibleRanges.clear();
                if (chunk.valid) chunk.visibleRanges.resize(chunk.sourceRanges.front().size());
            }
            for (auto& ranges : chunk.visibleRanges) ranges.clear();
            chunk.representative = entt::null;
            for (size_t i = 0; i < inputs.size(); ++i)
            {
                if (!visibleOpaque.contains(inputs[i].entity)) continue;
                if (!chunk.valid) { batchInputs.push_back(inputs[i]); continue; }
                chunk.representative = inputs[i].entity;
                for (size_t group = 0; group < chunk.visibleRanges.size(); ++group)
                {
                    auto& ranges = chunk.visibleRanges[group];
                    for (const auto& range : chunk.sourceRanges[i][group])
                    {
                        if (!ranges.empty() && ranges.back().first + ranges.back().count == range.first)
                            ranges.back().count += range.count;
                        else ranges.push_back(range);
                    }
                }
            }
        }

        if(!m_batchCache)m_batchCache=std::make_shared<MeshRendererCache>();
        for(const auto& input:batchInputs) {
            auto* material=input.key.materialId>=0 ? AssetManager::GetMaterial(input.key.materialId) : nullptr;
            auto* shader=material && material->shaderId>=0 ? AssetManager::Get<ShaderAsset>(material->shaderId) : nullptr;
            if(shader && shader->GetShader()!=m_shader && !shader->GetShader()->SupportsStaticBatching() && shader->HasVertexDeformation())roomQueries=false;
        }
        std::vector<uint64_t> hiddenRooms;
        if(roomQueries) {
            auto roomInputs = batchInputs;
            for (const auto& [key, chunk] : chunks)
                if (chunk->valid) roomInputs.insert(roomInputs.end(), chunk->inputs.begin(), chunk->inputs.end());
            m_batchCache->UpdateRooms(roomInputs,projection*view);
            for(const auto& [id,room]:m_batchCache->rooms)if(!room.visible)hiddenRooms.push_back(id);
        }
        for(const auto& input:batchInputs)
            if(std::binary_search(hiddenRooms.begin(),hiddenRooms.end(),input.key.group))RenderMetrics::RoomCull(1);
        auto& views=m_batchCache->views;
        auto cached=views.end();
        if(RenderMetrics::BatchCacheEnabled())
            cached=std::find_if(views.begin(),views.end(),[&](const auto& item){return item.inputs==batchInputs && item.hiddenRooms==hiddenRooms;});
        if(cached==views.end()) {
        RenderMetrics::BatchBuild();
        if(views.size()==4)views.pop_front();
        views.emplace_back();cached=std::prev(views.end());cached->inputs=std::move(batchInputs);
        cached->hiddenRooms=hiddenRooms;
        std::map<StaticModelBatchKey, StaticModelBatch> staticBatchMap;
        std::vector<std::pair<float,const MeshRendererCache::Input*>> orderedInputs;
        orderedInputs.reserve(cached->inputs.size());
        for(const auto& input:cached->inputs) {
            if(std::binary_search(hiddenRooms.begin(),hiddenRooms.end(),input.key.group))continue;
            const Vector3 offset=Vector3(input.matrix[3])-cameraPosition;
            orderedInputs.emplace_back(glm::dot(offset,offset),&input);
        }
        // Sort only on rebuild: front-to-back depth rejection without per-frame buffer churn.
        std::stable_sort(orderedInputs.begin(),orderedInputs.end(),[](const auto& a,const auto& b){return a.first<b.first;});
        for(const auto& [distance,inputPointer]:orderedInputs) {
            const auto& input=*inputPointer;
            auto key=input.key;
            // Visible rooms share material batches; room membership alone must not add draw calls.
            key.group=0;
            auto& batch=staticBatchMap[key];batch.key=key;batch.graph=input.graph;
            batch.colors.push_back(input.color);batch.modelMatrices.push_back(input.matrix);
            batch.sourceEntries.push_back({input.entity,0});
            batch.distanceSquared=std::max(batch.distanceSquared,distance);
        }
        auto& staticBatches=cached->batches;
        staticBatches.reserve(staticBatchMap.size());
        for (auto &batchPair : staticBatchMap)
        {
            StaticModelBatch &batch = batchPair.second;
            if (batch.modelMatrices.size() > 1u)
            {
                staticBatches.push_back(std::move(batch));
            }
            else
            {
                cached->singles.insert(
                    cached->singles.end(),
                    batch.sourceEntries.begin(),
                    batch.sourceEntries.end());
            }
        }
        std::sort(staticBatches.begin(),staticBatches.end(),[](const auto& a,const auto& b){return a.distanceSquared<b.distanceSquared;});
        }
        auto& staticBatches=cached->batches;
        for(auto entry:cached->singles) {
            const auto offset=Vector3(RenderMatrix(entry.entityHandle,_registry.get<Transform>(entry.entityHandle))[3])-cameraPosition;
            entry.distanceSquared=glm::dot(offset,offset);remainingOpaqueEntities.push_back(entry);
        }
        for (const auto& [key, batch] : dynamicBatches)
            if (batch.modelMatrices.size() == 1)
                remainingOpaqueEntities.push_back(batch.sourceEntries.front());

        std::sort(
            remainingOpaqueEntities.begin(),
            remainingOpaqueEntities.end(),
            [](const TransparentModelEntry &_a, const TransparentModelEntry &_b)
            {
                return _a.distanceSquared < _b.distanceSquared;
            });

        Profiler::Get().End(batchTiming,Profiler::Now());

        std::array<unsigned int, 4> boundSurfaceTextures;
        boundSurfaceTextures.fill(~0u);
        const char* disableSurfaceCache = std::getenv("CANIS_DISABLE_SURFACE_CACHE");
        const bool surfaceCacheEnabled = !disableSurfaceCache || std::string(disableSurfaceCache) != "1";
        auto bindSurfaceTexture = [&](unsigned int unit, i32 assetId)
        {
            auto* texture = assetId >= 0 ? AssetManager::GetTexture(assetId) : nullptr;
            const unsigned int id = texture ? texture->GetGLTexture().id : 0;
            auto& bound = boundSurfaceTextures[unit - 1];
            if (surfaceCacheEnabled && bound == id) return;
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, id);
            bound = id;
        };

        auto drawEntity = [&](const entt::entity entityHandle, ModelAsset* merged = nullptr,
                              const ModelAsset::DrawRanges3D* ranges = nullptr) -> void
        {
            Transform &transform = modelView.get<Transform>(entityHandle);
            Model &modelRenderer = modelView.get<Model>(entityHandle);
            Entity *entity = modelRenderer.entity;
            if (entity == nullptr)
                entity = transform.entity;

            if ((entity != nullptr && !entity->Active()) ||
                !transform.IsActiveInHierarchy() ||
                modelRenderer.modelId < 0)
                return;

            ModelAsset *model = merged ? merged : AssetManager::GetModel(RenderModelId(entityHandle,modelRenderer));
            if (model == nullptr)
                return;

            MaterialAsset *materialAsset = nullptr;
            Material *material = _registry.try_get<Material>(entityHandle);
            if (material != nullptr && material->materialId >= 0)
                materialAsset = AssetManager::GetMaterial(material->materialId);

            model->SetLightingScenarioBlend(lighting.from ? lighting.from->id : "", lighting.to ? lighting.to->id : "", lighting.blend);
            useShaderForMaterial(materialAsset);

            const ModelAsset::Pose3D *pose = nullptr;
            if (ModelAnimation *animation = _registry.try_get<ModelAnimation>(entityHandle))
            {
                const bool attachedModelPose =
                    animation->poseModelId == modelRenderer.modelId &&
                    animation->poseGeometryRevision ==
                        model->GetGeometryRevision();
                const bool compatibleSourcePose =
                    animation->sourceModelId >= 0 &&
                    animation->poseModelId == animation->sourceModelId &&
                    animation->pose.localNodeMatrices.size() ==
                        static_cast<std::size_t>(model->GetNodeCount());
                if (animation->poseInitialized &&
                    (attachedModelPose || compatibleSourcePose))
                    pose = &animation->pose;
            }

            Color baseColor = LODColor(entityHandle,modelRenderer.color);
            i32 overrideTextureId = -1;
            i32 specularTextureId = -1;
            i32 roughnessTextureId = -1;
            i32 metallicTextureId = -1;
            i32 emissionTextureId = -1;
            Color emissionColor = Color(1.0f);
            float emissionIntensity = 0.0f;
            float specularValue = 0.5f;
            float roughnessValue = 0.5f;
            float metallicValue = 0.0f;

            glDisable(GL_CULL_FACE);
            // Material uniforms live on the shared shader program. Restore
            // shader defaults before applying this entity's overrides so a
            // tiled surface (for example, the dining floor's uvScale) cannot
            // leak into models rendered afterward.
            currentShader->SetVec2("uvScale", Vector2(1.0f));
            int nextCustomTextureUnit = 6;
            if (materialAsset != nullptr)
            {
                if ((materialAsset->info & MATERIAL_HAS_COLOR) != 0u)
                    baseColor *= materialAsset->color;

                if (materialAsset->albedoId >= 0)
                    overrideTextureId = materialAsset->albedoId;
                if (materialAsset->specularId >= 0)
                    specularTextureId = materialAsset->specularId;
                if (materialAsset->roughnessId >= 0)
                    roughnessTextureId = materialAsset->roughnessId;
                if (materialAsset->metallicId >= 0)
                    metallicTextureId = materialAsset->metallicId;
                if (materialAsset->emissionId >= 0)
                    emissionTextureId = materialAsset->emissionId;

                emissionColor = materialAsset->emissionColor;
                emissionIntensity = materialAsset->emissionIntensity;
                specularValue = materialAsset->specularValue;
                roughnessValue = materialAsset->roughnessValue;
                metallicValue = materialAsset->metallicValue;

                if ((materialAsset->info & MATERIAL_BACK_FACE_CULLING) != 0u)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                }
                else if ((materialAsset->info & MATERIAL_FRONT_FACE_CULLING) != 0u)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);
                }

                nextCustomTextureUnit = materialAsset->materialFields.Use(*currentShader, nextCustomTextureUnit);

                if (material != nullptr)
                    nextCustomTextureUnit = material->materialFields.Use(*currentShader, nextCustomTextureUnit);
            }
            else if (material != nullptr)
            {
                nextCustomTextureUnit = material->materialFields.Use(*currentShader, nextCustomTextureUnit);
            }

            if (Terrain *terrainComponent = _registry.try_get<Terrain>(entityHandle))
            {
                TerrainAsset *terrainAsset = AssetManager::GetTerrain(AssetManager::ResolvePath(terrainComponent->terrain));
                if (terrainAsset != nullptr)
                {
                    const GLTexture splatTexture = terrainAsset->GetSplatmapTexture();
                    currentShader->SetInt("splatMap", nextCustomTextureUnit);
                    glActiveTexture(GL_TEXTURE0 + nextCustomTextureUnit);
                    glBindTexture(GL_TEXTURE_2D, splatTexture.id);
                    nextCustomTextureUnit++;
                }
            }

            if (material != nullptr)
                baseColor *= material->color;

            currentShader->SetFloat("TIME", static_cast<float>(Time::TimeSinceLaunch()) / 1000.0f);
            currentShader->SetFloat("specularValue", specularValue);
            currentShader->SetFloat("roughnessValue", roughnessValue);
            currentShader->SetFloat("metallicValue", metallicValue);
            currentShader->SetBool("weatherSurface", modelRenderer.weatherSurface);
            currentShader->SetVec4("emissionColor", emissionColor);
            currentShader->SetFloat("emissionIntensity", emissionIntensity);

            currentShader->SetBool("useSpecularMap", specularTextureId >= 0);
            currentShader->SetBool("useRoughnessMap", roughnessTextureId >= 0);
            currentShader->SetBool("useMetallicMap", metallicTextureId >= 0);
            currentShader->SetBool("useEmissionMap", emissionTextureId >= 0);
            currentShader->SetInt("specularMap", 1);
            currentShader->SetInt("roughnessMap", 2);
            currentShader->SetInt("metallicMap", 3);
            currentShader->SetInt("emissionMap", 4);

            bindSurfaceTexture(1, specularTextureId);
            bindSurfaceTexture(2, roughnessTextureId);
            bindSurfaceTexture(3, metallicTextureId);
            bindSurfaceTexture(4, emissionTextureId);

            glActiveTexture(GL_TEXTURE0);

            std::vector<MaterialAsset*> slotMaterialOverrides = {};
            if (material != nullptr && !material->materialIds.empty())
            {
                const i32 slotCount = model->GetMaterialSlotCount();
                if (slotCount > 0)
                {
                    slotMaterialOverrides.resize(static_cast<size_t>(slotCount), nullptr);
                    const size_t copyCount = std::min(slotMaterialOverrides.size(), material->materialIds.size());
                    for (size_t slotIndex = 0; slotIndex < copyCount; ++slotIndex)
                    {
                        const i32 slotMaterialId = material->materialIds[slotIndex];
                        if (slotMaterialId >= 0)
                            slotMaterialOverrides[slotIndex] = AssetManager::GetMaterial(slotMaterialId);
                    }
                }
            }

            VisibilityView visibility;
            visibility.occlusionEnabled=!m_capturingProbe && scene->GetBakingSettings().occlusion;
            visibility.count=m_stereoPass==1?2:1;
            for(int eye=0;eye<visibility.count;++eye) {
                visibility.clip[eye]=m_stereoPass==1?m_stereoProjections[eye]*m_stereoViews[eye]:projection*view;
                visibility.eye[eye]=m_stereoPass==1?m_stereoEyes[eye]:cameraPosition;
            }
            const char* disabledBake=std::getenv("CANIS_DISABLE_BAKED_VISIBILITY");
            const bool useVisibility=modelRenderer.staticModel && modelRenderer.bakedVisibility && !pose &&
                !_registry.try_get<ModelAnimation>(entityHandle) && !EntityUsesTransparency(_registry,entityHandle) && (!disabledBake || std::string(disabledBake)!="1");
            auto level=m_batchCache->levels.find(entityHandle);
            RenderMetrics::SetLODLevel(level==m_batchCache->levels.end()?0:level->second);
            model->Draw(
                *currentShader,
                merged ? Matrix4(1) : RenderMatrix(entityHandle,transform),
                pose,
                overrideTextureId,
                baseColor,
                slotMaterialOverrides.empty() ? nullptr : &slotMaterialOverrides,
                merged ? -1 : modelRenderer.nodeIndex,
                merged ? true : modelRenderer.applyNodeTransform,
                materialAsset,
                useVisibility?&visibility:nullptr,
                ranges);
            // Model material-slot overrides can bind these units themselves.
            if (!slotMaterialOverrides.empty()) boundSurfaceTextures.fill(~0u);
            RenderMetrics::SetLODLevel(-1);
        };

        auto drawStaticBatch = [&](const StaticModelBatch &_batch) -> void
        {
            RenderMetrics::SetLODLevel(_batch.key.lodLevel);
            ModelAsset *model = AssetManager::GetModel(_batch.key.modelId);
            if (model == nullptr)
                return;

            MaterialAsset *materialAsset = nullptr;
            if (_batch.key.materialId >= 0)
                materialAsset = AssetManager::GetMaterial(_batch.key.materialId);

            model->SetLightingScenarioBlend(lighting.from ? lighting.from->id : "", lighting.to ? lighting.to->id : "", lighting.blend);
            useShaderForMaterial(materialAsset);
            if (_batch.graph && materialAsset) materialAsset->materialFields.Use(*currentShader,6);

            Color baseColor = Color(
                _batch.key.modelColorR,
                _batch.key.modelColorG,
                _batch.key.modelColorB,
                _batch.key.modelColorA);
            i32 overrideTextureId = -1;
            i32 specularTextureId = -1;
            i32 roughnessTextureId = -1;
            i32 metallicTextureId = -1;
            i32 emissionTextureId = -1;
            Color emissionColor = Color(1.0f);
            float emissionIntensity = 0.0f;
            float specularValue = 0.5f;
            float roughnessValue = 0.5f;
            float metallicValue = 0.0f;

            glDisable(GL_CULL_FACE);
            // Static batches use the same shader as regular model draws and
            // therefore need the same per-draw uniform reset.
            currentShader->SetVec2("uvScale", Vector2(1.0f));
            if (materialAsset != nullptr)
            {
                if ((materialAsset->info & MATERIAL_HAS_COLOR) != 0u)
                    baseColor *= materialAsset->color;

                if (materialAsset->albedoId >= 0)
                    overrideTextureId = materialAsset->albedoId;
                if (materialAsset->specularId >= 0)
                    specularTextureId = materialAsset->specularId;
                if (materialAsset->roughnessId >= 0)
                    roughnessTextureId = materialAsset->roughnessId;
                if (materialAsset->metallicId >= 0)
                    metallicTextureId = materialAsset->metallicId;
                if (materialAsset->emissionId >= 0)
                    emissionTextureId = materialAsset->emissionId;

                emissionColor = materialAsset->emissionColor;
                emissionIntensity = materialAsset->emissionIntensity;
                specularValue = materialAsset->specularValue;
                roughnessValue = materialAsset->roughnessValue;
                metallicValue = materialAsset->metallicValue;

                if ((materialAsset->info & MATERIAL_BACK_FACE_CULLING) != 0u)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_BACK);
                }
                else if ((materialAsset->info & MATERIAL_FRONT_FACE_CULLING) != 0u)
                {
                    glEnable(GL_CULL_FACE);
                    glCullFace(GL_FRONT);
                }
            }

            baseColor *= Color(
                _batch.key.materialColorR,
                _batch.key.materialColorG,
                _batch.key.materialColorB,
                _batch.key.materialColorA);

            currentShader->SetFloat("TIME", static_cast<float>(Time::TimeSinceLaunch()) / 1000.0f);
            currentShader->SetFloat("specularValue", specularValue);
            currentShader->SetFloat("roughnessValue", roughnessValue);
            currentShader->SetFloat("metallicValue", metallicValue);
            currentShader->SetBool("weatherSurface", _batch.key.weatherSurface);
            currentShader->SetVec4("emissionColor", emissionColor);
            currentShader->SetFloat("emissionIntensity", emissionIntensity);

            currentShader->SetBool("useSpecularMap", specularTextureId >= 0);
            currentShader->SetBool("useRoughnessMap", roughnessTextureId >= 0);
            currentShader->SetBool("useMetallicMap", metallicTextureId >= 0);
            currentShader->SetBool("useEmissionMap", emissionTextureId >= 0);
            currentShader->SetInt("specularMap", 1);
            currentShader->SetInt("roughnessMap", 2);
            currentShader->SetInt("metallicMap", 3);
            currentShader->SetInt("emissionMap", 4);

            bindSurfaceTexture(1, specularTextureId);
            bindSurfaceTexture(2, roughnessTextureId);
            bindSurfaceTexture(3, metallicTextureId);
            bindSurfaceTexture(4, emissionTextureId);

            glActiveTexture(GL_TEXTURE0);

            model->DrawInstanced(
                *currentShader,
                _batch.modelMatrices,
                overrideTextureId,
                baseColor,
                _batch.key.nodeIndex,
                _batch.key.applyNodeTransform,
                _batch.graph ? &_batch.colors : nullptr);
        };

        glDepthMask(GL_TRUE);
        {
        RenderMetrics::Scope timing("Opaque geometry");
        for (const auto& [key, chunk] : chunks)
            if (chunk->valid && chunk->representative != entt::null)
            {
                if (std::binary_search(hiddenRooms.begin(), hiddenRooms.end(), chunk->inputs.front().key.group))
                    RenderMetrics::RoomCull(1);
                else drawEntity(chunk->representative, &chunk->geometry, &chunk->visibleRanges);
            }
        for (const StaticModelBatch &batch : staticBatches)
        {
            drawStaticBatch(batch);
            RenderMetrics::SetLODLevel(-1);
        }
        for (const auto& [key, batch] : dynamicBatches)
        {
            if (batch.modelMatrices.size() <= 1)
                continue;
            drawStaticBatch(batch);
            RenderMetrics::SetLODLevel(-1);
        }
#ifndef __EMSCRIPTEN__
        if(roomQueries) {
            RenderMetrics::Scope timing("Room visibility queries");
            const GLboolean cullEnabled=glIsEnabled(GL_CULL_FACE);
            glColorMask(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE);glDepthMask(GL_FALSE);glDepthFunc(GL_LEQUAL);glDisable(GL_CULL_FACE);
            m_shadowShader->Use();m_shadowShader->SetBool("useInstanceMatrix",false);
            m_shadowShader->SetMat4("lightSpaceMatrix",projection*view);glBindVertexArray(m_skyboxVao);
            for(auto& [id,room]:m_batchCache->rooms) {
                if(room.pending || room.minimum.x>room.maximum.x)continue;
                bool near=false;
                for(int corner=0;corner<8;++corner) {
                    const auto p=projection*view*Vector4(corner&1?room.maximum.x:room.minimum.x,corner&2?room.maximum.y:room.minimum.y,corner&4?room.maximum.z:room.minimum.z,1);
                    near|=!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || !std::isfinite(p.w) || p.z+p.w<=.001f;
                }
                if(near) { room.visible=true;continue; }
                if(!room.query)glGenQueries(1,&room.query);
                m_shadowShader->SetMat4("M",glm::scale(glm::translate(Matrix4(1),(room.minimum+room.maximum)*.5f),(room.maximum-room.minimum)*.5f));
                glBeginQuery(GL_ANY_SAMPLES_PASSED,room.query);glDrawArrays(GL_TRIANGLES,0,36);glEndQuery(GL_ANY_SAMPLES_PASSED);
                room.pending=true;room.generation=m_batchCache->generation;
            }
            glBindVertexArray(0);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);
            if(cullEnabled)glEnable(GL_CULL_FACE);
            if(currentShader)currentShader->Use();else m_shadowShader->UnUse();
        }
#endif
        for (const TransparentModelEntry &entry : remainingOpaqueEntities)
            drawEntity(entry.entityHandle);
        }

        if (m_stereoPass != 1) {
            if (currentShader) { currentShader->UnUse(); currentShader = nullptr; }
            DrawSkybox(projection, view);
        }

        {
        RenderMetrics::Scope timing("Transparent geometry");
        // Copy opaque depth only for shaders that request soft intersections.
        // A separate texture avoids sampling a framebuffer's attached depth image.
        bool wantsDepth=false;
        for(const auto& entry:transparentEntities) {
            auto* material=_registry.try_get<Material>(entry.entityHandle);
            if(!material || material->materialId<0) continue;
            auto* asset=AssetManager::GetMaterial(material->materialId);
            if(!asset || asset->shaderId<0) continue;
            auto* shaderAsset=AssetManager::Get<ShaderAsset>(asset->shaderId);
            if(!shaderAsset) continue;
            auto* shader=shaderAsset->GetShader();
            if(!shader->IsLinked())shader->Link();
            if(shader->GetUniformLocation("softSceneDepth",0)>=0){wantsDepth=true;break;}
        }
        GLint savedTexture=0,savedSampler=0,savedActive=GL_TEXTURE0,savedRead=0;
        if(wantsDepth) {
            RenderMetrics::Scope depthTiming("Soft volume depth copy");
            glGetIntegerv(GL_VIEWPORT,softDepthViewport);
            GLint samples=0,drawFramebuffer=0;glGetIntegerv(GL_SAMPLES,&samples);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFramebuffer);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&savedRead);
            glGetIntegerv(GL_ACTIVE_TEXTURE,&savedActive);
            glActiveTexture(GL_TEXTURE15);glGetIntegerv(GL_TEXTURE_BINDING_2D,&savedTexture);
            glGetIntegeri_v(GL_SAMPLER_BINDING,15,&savedSampler);glBindSampler(15,0);
            if(samples<=1 && softDepthViewport[2]>0 && softDepthViewport[3]>0) {
                if(!m_softDepthTexture)glGenTextures(1,&m_softDepthTexture);
                glBindTexture(GL_TEXTURE_2D,m_softDepthTexture);
                if(m_softDepthWidth!=softDepthViewport[2] || m_softDepthHeight!=softDepthViewport[3]) {
                    m_softDepthWidth=softDepthViewport[2];m_softDepthHeight=softDepthViewport[3];
                    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,m_softDepthWidth,m_softDepthHeight,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                }
                glBindFramebuffer(GL_READ_FRAMEBUFFER,drawFramebuffer);
                glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,softDepthViewport[0],softDepthViewport[1],m_softDepthWidth,m_softDepthHeight);
                glBindFramebuffer(GL_READ_FRAMEBUFFER,savedRead);
                softDepthReady=true;
            }
            glActiveTexture(GL_TEXTURE0);
            if(currentShader){currentShader->UnUse();currentShader=nullptr;}
        }
        glDepthMask(GL_FALSE);
        for (const TransparentModelEntry &entry : transparentEntities)
            drawEntity(entry.entityHandle);
        glDepthMask(GL_TRUE);
        if(wantsDepth) {
            glActiveTexture(GL_TEXTURE15);glBindTexture(GL_TEXTURE_2D,savedTexture);
            glBindSampler(15,savedSampler);glActiveTexture(savedActive);
        }
        }

        if (currentShader != nullptr)
        {
            glActiveTexture(GL_TEXTURE5);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
            currentShader->UnUse();
        }

        if (!m_capturingProbe && m_stereoPass != 1) {
        DrawTrails(_registry, projection, view, cameraPosition);
        {
        RenderMetrics::Scope timing("Debug geometry");
        DrawColliderDebugLines(_registry, projection, view);
        DrawDebugGizmoLines(projection, view);
        }

        }
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
    }
    void MeshRenderer3DSystem::DrawTrails(entt::registry& registry, const Matrix4& projection,
                                        const Matrix4& view, Vector3 cameraPosition)
    {
        RenderMetrics::Scope timing("Trails");
        std::vector<TrailVertex> vertices;
        for (auto [handle, trail, transform] : registry.view<TrailRenderer, Transform>().each())
        {
            if (!transform.IsActiveInHierarchy()) continue;
            auto mesh = BuildTrailMesh(trail, cameraPosition);
            vertices.insert(vertices.end(), mesh.begin(), mesh.end());
        }
        if (vertices.empty()) return;
        // Sort transparent triangles for this eye without writing to scene depth.
        std::vector<size_t> order(vertices.size()/3);
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        auto depth = [&](size_t i) {
            auto center = (vertices[i*3].position + vertices[i*3+1].position + vertices[i*3+2].position)/3.f;
            return (view * Vector4(center,1)).z;
        };
        std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return depth(a) < depth(b); });
        std::vector<TrailVertex> sorted;
        sorted.reserve(vertices.size());
        for (size_t i : order) sorted.insert(sorted.end(), vertices.begin()+i*3, vertices.begin()+i*3+3);
        if (!m_trailShader)
        {
            auto id = AssetManager::LoadShader("assets/shaders/trail");
            m_trailShader = AssetManager::Get<ShaderAsset>(id)->GetShader();
            if (!m_trailShader->IsLinked())
            {
                m_trailShader->AddAttribute("vertexPosition");
                m_trailShader->AddAttribute("vertexColor");
                m_trailShader->Link();
            }
            glGenVertexArrays(1, &m_trailVao);
            glGenBuffers(1, &m_trailVbo);
            glBindVertexArray(m_trailVao);
            glBindBuffer(GL_ARRAY_BUFFER, m_trailVbo);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, position));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(TrailVertex), (void*)offsetof(TrailVertex, color));
        }
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        m_trailShader->Use();
        m_trailShader->SetMat4("P", projection);
        m_trailShader->SetMat4("V", view);
        glBindVertexArray(m_trailVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_trailVbo);
        glBufferData(GL_ARRAY_BUFFER, sorted.size()*sizeof(TrailVertex), sorted.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(sorted.size()));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
        m_trailShader->UnUse();
        glDepthMask(GL_TRUE);
    }
} // end of Canis namespace
