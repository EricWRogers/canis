#include <Canis/ECS/Systems/JoltPhysics3DSystem.hpp>

#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Components.hpp>
#include <Canis/Math.hpp>
#include <Canis/Profiler.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/AllowedDOFs.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionGroup.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <mutex>
#include <type_traits>
#include <tuple>
#include <string_view>
#include <unordered_map>
#include <thread>
#include <vector>

namespace Canis
{
    namespace
    {
        constexpr float kFixedTimeStep = 1.0f / 60.0f;
        constexpr float kDefaultRaycastDistance = 100000.0f;
        constexpr int kMaxSubSteps = 4;
        constexpr int kCollisionSteps = 1;

        constexpr JPH::uint kMaxBodies = 65536;
        constexpr JPH::uint kNumBodyMutexes = 0;
        constexpr JPH::uint kMaxBodyPairs = 65536;
        constexpr JPH::uint kMaxContactConstraints = 10240;

        int g_joltInitRefCount = 0;

        namespace Layers
        {
            static constexpr JPH::ObjectLayer NON_MOVING = 0;
            static constexpr JPH::ObjectLayer MOVING = 1;
            static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
        } // namespace Layers

        namespace BroadPhaseLayers
        {
            static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
            static constexpr JPH::BroadPhaseLayer MOVING(1);
            static constexpr JPH::uint NUM_LAYERS = 2;
        } // namespace BroadPhaseLayers

        class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
            {
                switch (inObject1)
                {
                case Layers::NON_MOVING:
                    return inObject2 == Layers::MOVING;
                case Layers::MOVING:
                    return true;
                default:
                    return false;
                }
            }
        };

        class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
        {
        public:
            BPLayerInterfaceImpl()
            {
                m_objectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
                m_objectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
            }

            JPH::uint GetNumBroadPhaseLayers() const override
            {
                return BroadPhaseLayers::NUM_LAYERS;
            }

            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
            {
                return m_objectToBroadPhase[inLayer];
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
            {
                switch ((JPH::BroadPhaseLayer::Type)inLayer)
                {
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
                    return "NON_MOVING";
                case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
                    return "MOVING";
                default:
                    return "UNKNOWN";
                }
            }
#endif

        private:
            JPH::BroadPhaseLayer m_objectToBroadPhase[Layers::NUM_LAYERS];
        };

        class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
        public:
            bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
            {
                switch (inLayer1)
                {
                case Layers::NON_MOVING:
                    return inLayer2 == BroadPhaseLayers::MOVING;
                case Layers::MOVING:
                    return true;
                default:
                    return false;
                }
            }
        };

        void InitJoltGlobal()
        {
            if (g_joltInitRefCount++ > 0)
                return;

            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }

        void ShutdownJoltGlobal()
        {
            if (g_joltInitRefCount <= 0)
                return;

            if (--g_joltInitRefCount > 0)
                return;

            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }

        inline JPH::RVec3 ToJoltPosition(const Vector3 &_value)
        {
            return JPH::RVec3((double)_value.x, (double)_value.y, (double)_value.z);
        }

        inline Vector3 ToCanisPosition(const JPH::RVec3 &_value)
        {
            return Vector3((float)_value.GetX(), (float)_value.GetY(), (float)_value.GetZ());
        }

        inline JPH::Vec3 ToJoltVec3(const Vector3 &_value)
        {
            return JPH::Vec3(_value.x, _value.y, _value.z);
        }

        JPH::Quat ToJoltRotation(const Quaternion &_rotation)
        {
            const Quaternion quat = glm::normalize(_rotation);
            return JPH::Quat(quat.x, quat.y, quat.z, quat.w);
        }

        Quaternion ToCanisRotation(const JPH::Quat &_quat)
        {
            return glm::normalize(Quaternion(_quat.GetW(), _quat.GetX(), _quat.GetY(), _quat.GetZ()));
        }

        bool NearlyEqual(const Vector3 &_a, const Vector3 &_b, float _epsilon = 0.0005f)
        {
            return glm::all(glm::lessThanEqual(glm::abs(_a - _b), Vector3(_epsilon)));
        }

        bool NearlyEqual(const Quaternion &_a, const Quaternion &_b, float _epsilon = 0.0000001f)
        {
            // Quaternion dot products measure half the rotation angle.  The
            // position tolerance is much too large here: 0.0005 treats yaw
            // edits smaller than roughly 3.6 degrees as unchanged.  A script
            // rotating a dynamic body a little each frame would consequently
            // have its edit overwritten by the physics pose unless it also
            // moved the body.
            return 1.0f - std::fabs(glm::dot(glm::normalize(_a), glm::normalize(_b))) <= _epsilon;
        }

        bool NearlyZero(const Vector3 &_value, float _epsilon = 0.000001f)
        {
            return glm::all(glm::lessThanEqual(glm::abs(_value), Vector3(_epsilon)));
        }

        JPH::RefConst<JPH::Shape> ApplyLocalShapeOffset(
            const JPH::RefConst<JPH::Shape> &_shape,
            const Vector3 &_scaledOffset,
            const char *_debugName)
        {
            if (_shape.GetPtr() == nullptr || NearlyZero(_scaledOffset))
                return _shape;

            JPH::RotatedTranslatedShapeSettings shapeSettings(
                ToJoltVec3(_scaledOffset),
                JPH::Quat::sIdentity(),
                _shape.GetPtr());

            JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();
            if (shapeResult.HasError())
            {
                Debug::Log("Jolt %s offset shape error: %s", _debugName, shapeResult.GetError().c_str());
                return nullptr;
            }

            return shapeResult.Get();
        }

        JPH::EMotionType ToMotionType(int _motionType)
        {
            switch (_motionType)
            {
            case RigidbodyMotionType::STATIC:
                return JPH::EMotionType::Static;
            case RigidbodyMotionType::KINEMATIC:
                return JPH::EMotionType::Kinematic;
            case RigidbodyMotionType::DYNAMIC:
            default:
                return JPH::EMotionType::Dynamic;
            }
        }

        JPH::ObjectLayer ToObjectLayer(JPH::EMotionType _motionType)
        {
            return (_motionType == JPH::EMotionType::Static) ? Layers::NON_MOVING : Layers::MOVING;
        }

        JPH::EAllowedDOFs BuildAllowedDOFs(const Rigidbody &_rigidbody)
        {
            // Constrain contact impulses in the solver, before rotation can accumulate.
            auto allowed = JPH::EAllowedDOFs::TranslationX |
                JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
            if (!_rigidbody.lockRotationX) allowed |= JPH::EAllowedDOFs::RotationX;
            if (!_rigidbody.lockRotationY) allowed |= JPH::EAllowedDOFs::RotationY;
            if (!_rigidbody.lockRotationZ) allowed |= JPH::EAllowedDOFs::RotationZ;
            return allowed;
        }

        bool ApplyLockedRotationAxes(
            const Transform &_desiredTransform,
            const Rigidbody &_rigidbody,
            Vector3 &_worldRotation,
            Vector3 &_angularVelocity)
        {
            if (!_rigidbody.lockRotationX && !_rigidbody.lockRotationY && !_rigidbody.lockRotationZ)
                return false;

            const Vector3 desiredWorldRotation = glm::eulerAngles(_desiredTransform.GetGlobalRotation());
            bool corrected = false;

            if (_rigidbody.lockRotationX)
            {
                corrected = corrected || std::abs(_worldRotation.x - desiredWorldRotation.x) > 0.0005f || std::abs(_angularVelocity.x) > 0.0005f;
                _worldRotation.x = desiredWorldRotation.x;
                _angularVelocity.x = 0.0f;
            }

            if (_rigidbody.lockRotationY)
            {
                corrected = corrected || std::abs(_worldRotation.y - desiredWorldRotation.y) > 0.0005f || std::abs(_angularVelocity.y) > 0.0005f;
                _worldRotation.y = desiredWorldRotation.y;
                _angularVelocity.y = 0.0f;
            }

            if (_rigidbody.lockRotationZ)
            {
                corrected = corrected || std::abs(_worldRotation.z - desiredWorldRotation.z) > 0.0005f || std::abs(_angularVelocity.z) > 0.0005f;
                _worldRotation.z = desiredWorldRotation.z;
                _angularVelocity.z = 0.0f;
            }

            return corrected;
        }

        void SetTransformFromWorldPose(Transform &_transform, const Vector3 &_worldPosition, const Quaternion &_worldRotation)
        {
            if (_transform.parent != nullptr)
            {
                if (_transform.parent->HasComponent<Transform>())
                {
                    Transform& parentTransform = _transform.parent->GetComponent<Transform>();
                    const Matrix4 inverseParent = glm::inverse(parentTransform.GetModelMatrix());
                    const Vector4 localPosition4 = inverseParent * Vector4(_worldPosition, 1.0f);
                    _transform.position = Vector3(localPosition4.x, localPosition4.y, localPosition4.z);
                    _transform.rotation = glm::normalize(glm::inverse(parentTransform.GetGlobalRotation()) * _worldRotation);
                    return;
                }
            }

            _transform.position = _worldPosition;
            _transform.rotation = _worldRotation;
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

        auto RigidbodySettings(const Rigidbody& body)
        {
            return std::make_tuple(body.motionType, body.mass, body.friction, body.restitution,
                body.linearDamping, body.angularDamping, body.useGravity, body.gravityFactor,
                body.isSensor, body.layer, body.mask, body.allowSleeping,
                body.lockRotationX, body.lockRotationY, body.lockRotationZ);
        }

        // Compare authored values directly: public component fields can change without
        // an EnTT update signal. Runtime velocities/contact lists are intentionally absent.
        template<typename Path> struct BodySettingsData
        {
            decltype(RigidbodySettings(Rigidbody{})) body;
            Vector3 worldScale{1.0f};
            int shape = 0;
            Vector3 offset{0.0f}, size{1.0f};
            bool customCenterOfMass=false;
            Vector3 centerOfMass{0};
            float radius = 0.0f, halfHeight = 0.0f;
            bool attached = false, applyNodeTransform = true;
            Path modelPath;
            i32 modelId = -1, nodeIndex = -1;
            u64 geometryRevision = 0;
            template<typename OtherPath> bool operator==(const BodySettingsData<OtherPath>& other) const
            {
                // Matrix decomposition introduces rounding when a hierarchy rotates.
                // Compare against the scale of the existing shape (never the previous
                // frame), so accumulated scale edits still trigger a rebuild.
                for (int axis = 0; axis < 3; ++axis) {
                    const float a = worldScale[axis], b = other.worldScale[axis];
                    if (!std::isfinite(a) || !std::isfinite(b) ||
                        std::abs(a - b) > 2.e-6f * std::max(std::abs(a), std::abs(b)))
                        return false;
                }
                if(customCenterOfMass!=other.customCenterOfMass ||
                    (customCenterOfMass && !NearlyEqual(centerOfMass,other.centerOfMass,2e-5f)))return false;
                // Authored collider and body settings retain exact comparison.
                return std::tie(body, shape, offset, size, radius, halfHeight, attached,
                           applyNodeTransform, modelPath, modelId, nodeIndex, geometryRevision) ==
                       std::tie(other.body, other.shape, other.offset, other.size, other.radius,
                           other.halfHeight, other.attached, other.applyNodeTransform,
                           other.modelPath, other.modelId, other.nodeIndex, other.geometryRevision);
            }
            BodySettingsData() = default;
            template<typename OtherPath> explicit BodySettingsData(const BodySettingsData<OtherPath>& other)
                : body(other.body), worldScale(other.worldScale), shape(other.shape), offset(other.offset),
                  size(other.size), customCenterOfMass(other.customCenterOfMass), centerOfMass(other.centerOfMass),
                  radius(other.radius), halfHeight(other.halfHeight), attached(other.attached),
                  applyNodeTransform(other.applyNodeTransform), modelPath(other.modelPath), modelId(other.modelId),
                  nodeIndex(other.nodeIndex), geometryRevision(other.geometryRevision) {}
        };
        using BodySettings = BodySettingsData<std::string>;
        using BodySettingsView = BodySettingsData<std::string_view>;

        BodySettingsView CaptureSettings(entt::registry& registry, entt::entity handle,
            const Vector3& worldScale, const Rigidbody& body,
            const BoxCollider* box, const SphereCollider* sphere, const CapsuleCollider* capsule,
            const MeshCollider* mesh, const ConvexMeshCollider* convex)
        {
            // The comparison borrows the authored path. Only a created/rebuilt
            // body owns a snapshot, avoiding thousands of allocations per frame.
            BodySettingsView settings;
            settings.body = RigidbodySettings(body);
            settings.worldScale = worldScale;
            if (box) {
                settings.shape = 1; settings.offset = box->offset; settings.size = box->size;
            } else if (sphere) {
                settings.shape = 2; settings.offset = sphere->offset; settings.radius = sphere->radius;
            } else if (capsule) {
                settings.shape = 3; settings.offset = capsule->offset;
                settings.radius = capsule->radius; settings.halfHeight = capsule->halfHeight;
            } else if (mesh) {
                settings.shape = 4; settings.attached = mesh->useAttachedModel;
                settings.modelPath = mesh->modelPath;
                settings.modelId = ResolveMeshColliderModelId(registry, handle, mesh);
                settings.nodeIndex = ResolveMeshColliderNodeIndex(registry, handle, mesh);
                settings.applyNodeTransform = ResolveMeshColliderApplyNodeTransform(registry, handle, mesh);
            } else if (convex) {
                settings.shape = 5; settings.attached = convex->useAttachedModel;
                settings.modelPath = convex->modelPath;
                settings.offset = convex->offset; settings.size = convex->scale;
                settings.radius = convex->convexRadius;
                settings.modelId = ResolveConvexMeshColliderModelId(registry, handle, convex);
                settings.nodeIndex = ResolveConvexMeshColliderNodeIndex(registry, handle, convex);
                settings.applyNodeTransform = ResolveConvexMeshColliderApplyNodeTransform(registry, handle, convex);
            }
            if (settings.shape >= 4)
                if (const auto* model = AssetManager::GetModel(settings.modelId))
                    settings.geometryRevision = model->GetGeometryRevision();
            return settings;
        }

        JPH::RefConst<JPH::Shape> BuildShape(
            entt::registry &_registry,
            entt::entity _entityHandle,
            const Transform &_transform,
            const BoxCollider *_boxCollider,
            const SphereCollider *_sphereCollider,
            const CapsuleCollider *_capsuleCollider,
            const MeshCollider *_meshCollider,
            const ConvexMeshCollider *_convexMeshCollider)
        {
            const Vector3 signedGlobalScale = _transform.GetGlobalScale();
            const Vector3 globalScale = glm::abs(signedGlobalScale);

            if (_boxCollider != nullptr)
            {
                const Vector3 halfExtents = glm::max((_boxCollider->size * globalScale) * 0.5f, Vector3(0.01f));
                JPH::BoxShapeSettings shapeSettings(ToJoltVec3(halfExtents));
                JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();
                if (shapeResult.HasError())
                {
                    Debug::Log("Jolt BoxCollider shape error: %s", shapeResult.GetError().c_str());
                    return nullptr;
                }

                JPH::RefConst<JPH::Shape> shape = shapeResult.Get();
                return ApplyLocalShapeOffset(shape, _boxCollider->offset * signedGlobalScale, "BoxCollider");
            }

            if (_sphereCollider != nullptr)
            {
                const float scale = glm::max(globalScale.x, glm::max(globalScale.y, globalScale.z));
                const float radius = glm::max(0.01f, _sphereCollider->radius * scale);
                JPH::SphereShapeSettings shapeSettings(radius);
                JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();
                if (shapeResult.HasError())
                {
                    Debug::Log("Jolt SphereCollider shape error: %s", shapeResult.GetError().c_str());
                    return nullptr;
                }

                JPH::RefConst<JPH::Shape> shape = shapeResult.Get();
                return ApplyLocalShapeOffset(shape, _sphereCollider->offset * signedGlobalScale, "SphereCollider");
            }

            if (_capsuleCollider != nullptr)
            {
                const float radiusScale = glm::max(globalScale.x, globalScale.z);
                const float radius = glm::max(0.01f, _capsuleCollider->radius * radiusScale);
                const float halfHeight = glm::max(0.01f, _capsuleCollider->halfHeight * globalScale.y);
                JPH::CapsuleShapeSettings shapeSettings(halfHeight, radius);
                JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();
                if (shapeResult.HasError())
                {
                    Debug::Log("Jolt CapsuleCollider shape error: %s", shapeResult.GetError().c_str());
                    return nullptr;
                }

                JPH::RefConst<JPH::Shape> shape = shapeResult.Get();
                return ApplyLocalShapeOffset(shape, _capsuleCollider->offset * signedGlobalScale, "CapsuleCollider");
            }

            if (_meshCollider != nullptr)
            {
                const i32 modelId = ResolveMeshColliderModelId(_registry, _entityHandle, _meshCollider);
                const i32 nodeIndex = ResolveMeshColliderNodeIndex(_registry, _entityHandle, _meshCollider);
                const bool applyNodeTransform = ResolveMeshColliderApplyNodeTransform(_registry, _entityHandle, _meshCollider);
                const ModelAsset *model = AssetManager::GetModel(modelId);
                if (model == nullptr)
                {
                    Debug::Log("Jolt MeshCollider shape error: no source model for entity.");
                    return nullptr;
                }

                std::vector<Vector3> vertices = {};
                std::vector<u32> indices = {};
                if (!model->BuildTriangleMesh(vertices, indices, nodeIndex, applyNodeTransform) || indices.size() < 3)
                {
                    Debug::Log("Jolt MeshCollider shape error: source model has no triangle mesh.");
                    return nullptr;
                }

                JPH::TriangleList triangles;
                triangles.reserve(indices.size() / 3u);
                for (size_t i = 0; i + 2 < indices.size(); i += 3)
                {
                    const Vector3 &v0 = vertices[indices[i + 0]];
                    const Vector3 &v1 = vertices[indices[i + 1]];
                    const Vector3 &v2 = vertices[indices[i + 2]];
                    triangles.push_back(JPH::Triangle(
                        JPH::Float3(v0.x * globalScale.x, v0.y * globalScale.y, v0.z * globalScale.z),
                        JPH::Float3(v1.x * globalScale.x, v1.y * globalScale.y, v1.z * globalScale.z),
                        JPH::Float3(v2.x * globalScale.x, v2.y * globalScale.y, v2.z * globalScale.z)));
                }

                JPH::MeshShapeSettings shapeSettings(triangles);
                JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();
                if (shapeResult.HasError())
                {
                    Debug::Log("Jolt MeshCollider shape error: %s", shapeResult.GetError().c_str());
                    return nullptr;
                }
                return shapeResult.Get();
            }

            if (_convexMeshCollider != nullptr)
            {
                const i32 modelId = ResolveConvexMeshColliderModelId(_registry, _entityHandle, _convexMeshCollider);
                const i32 nodeIndex = ResolveConvexMeshColliderNodeIndex(_registry, _entityHandle, _convexMeshCollider);
                const bool applyNodeTransform = ResolveConvexMeshColliderApplyNodeTransform(_registry, _entityHandle, _convexMeshCollider);
                const ModelAsset *model = AssetManager::GetModel(modelId);
                if (model == nullptr)
                {
                    Debug::Log("Jolt ConvexMeshCollider shape error: no source model for entity.");
                    return nullptr;
                }

                std::vector<Vector3> vertices = {};
                std::vector<u32> indices = {};
                if (!model->BuildTriangleMesh(vertices, indices, nodeIndex, applyNodeTransform) || vertices.size() < 4)
                {
                    Debug::Log("Jolt ConvexMeshCollider shape error: source model has fewer than four vertices.");
                    return nullptr;
                }

                std::vector<JPH::Vec3> points = {};
                points.reserve(vertices.size());
                const Vector3 colliderScale = _convexMeshCollider->scale * signedGlobalScale;
                for (const Vector3 &vertex : vertices)
                {
                    points.emplace_back(
                        vertex.x * colliderScale.x,
                        vertex.y * colliderScale.y,
                        vertex.z * colliderScale.z);
                }

                const Vector3 absoluteColliderScale = glm::abs(_convexMeshCollider->scale) * globalScale;
                const float radiusScale = glm::min(
                    absoluteColliderScale.x,
                    glm::min(absoluteColliderScale.y, absoluteColliderScale.z));
                const float convexRadius = glm::max(0.0f, _convexMeshCollider->convexRadius * radiusScale);
                JPH::ConvexHullShapeSettings shapeSettings(
                    points.data(),
                    static_cast<int>(points.size()),
                    convexRadius);
                JPH::Shape::ShapeResult shapeResult = shapeSettings.Create();
                if (shapeResult.HasError())
                {
                    Debug::Log("Jolt ConvexMeshCollider shape error: %s", shapeResult.GetError().c_str());
                    return nullptr;
                }
                JPH::RefConst<JPH::Shape> shape = shapeResult.Get();
                return ApplyLocalShapeOffset(
                    shape,
                    _convexMeshCollider->offset * signedGlobalScale,
                    "ConvexMeshCollider");
            }

            return nullptr;
        }

        uint64_t ToBodyUserData(entt::entity _entityHandle)
        {
            return static_cast<uint64_t>(entt::to_integral(_entityHandle));
        }

        entt::entity ToEntity(uint64_t _userData)
        {
            using EntityValue = std::underlying_type_t<entt::entity>;
            return static_cast<entt::entity>(static_cast<EntityValue>(_userData));
        }

        Entity* ResolveEntity(entt::registry &_registry, entt::entity _entityHandle)
        {
            if (!_registry.valid(_entityHandle))
                return nullptr;

            if (Rigidbody *rigidbody = _registry.try_get<Rigidbody>(_entityHandle))
                return rigidbody->entity;

            if (Transform *transform = _registry.try_get<Transform>(_entityHandle))
                return transform->entity;

            if (BoxCollider *boxCollider = _registry.try_get<BoxCollider>(_entityHandle))
                return boxCollider->entity;

            if (SphereCollider *sphereCollider = _registry.try_get<SphereCollider>(_entityHandle))
                return sphereCollider->entity;

            if (CapsuleCollider *capsuleCollider = _registry.try_get<CapsuleCollider>(_entityHandle))
                return capsuleCollider->entity;

            if (MeshCollider *meshCollider = _registry.try_get<MeshCollider>(_entityHandle))
                return meshCollider->entity;

            if (ConvexMeshCollider *collider = _registry.try_get<ConvexMeshCollider>(_entityHandle))
                return collider->entity;

            return nullptr;
        }

        template <typename Collider>
        void ClearColliderContactVectors(entt::registry &_registry)
        {
            auto colliderView = _registry.view<Collider>();
            for (const entt::entity entityHandle : colliderView)
            {
                Collider &collider = colliderView.template get<Collider>(entityHandle);
                collider.entered.clear();
                collider.exited.clear();
                collider.stayed.clear();
            }
        }

        void ClearColliderContactVectors(entt::registry &_registry)
        {
            ClearColliderContactVectors<BoxCollider>(_registry);
            ClearColliderContactVectors<SphereCollider>(_registry);
            ClearColliderContactVectors<CapsuleCollider>(_registry);
            ClearColliderContactVectors<MeshCollider>(_registry);
            ClearColliderContactVectors<CombineMeshCollider>(_registry);
            ClearColliderContactVectors<ConvexMeshCollider>(_registry);
        }

        template <typename Func>
        void ForEachCollider(entt::registry &_registry, entt::entity _entityHandle, Func &&_func)
        {
            if (BoxCollider *boxCollider = _registry.try_get<BoxCollider>(_entityHandle))
                _func(*boxCollider);

            if (SphereCollider *sphereCollider = _registry.try_get<SphereCollider>(_entityHandle))
                _func(*sphereCollider);

            if (CapsuleCollider *capsuleCollider = _registry.try_get<CapsuleCollider>(_entityHandle))
                _func(*capsuleCollider);

            if (MeshCollider *meshCollider = _registry.try_get<MeshCollider>(_entityHandle))
                _func(*meshCollider);
            if (auto* combined = _registry.try_get<CombineMeshCollider>(_entityHandle))
                _func(*combined);

            if (ConvexMeshCollider *collider = _registry.try_get<ConvexMeshCollider>(_entityHandle))
                _func(*collider);
        }
    } // namespace

    struct JoltPhysics3DSystem::Impl
    {
        struct FrameContactEvent
        {
            entt::entity self = entt::null;
            entt::entity other = entt::null;
        };

        struct DirectedBodyPairKey
        {
            JPH::BodyID selfBodyID = {};
            JPH::BodyID otherBodyID = {};

            bool operator==(const DirectedBodyPairKey &_other) const = default;
        };

        struct DirectedBodyPairKeyHash
        {
            size_t operator()(const DirectedBodyPairKey &_key) const
            {
                size_t hash = 0;
                hash = HashCombine(hash, std::hash<JPH::BodyID>{}(_key.selfBodyID));
                hash = HashCombine(hash, std::hash<JPH::BodyID>{}(_key.otherBodyID));
                return hash;
            }
        };

        struct ActiveDirectedPair
        {
            entt::entity self = entt::null;
            entt::entity other = entt::null;
            int subShapeCount = 0;
        };

        struct SubShapeContactData
        {
            entt::entity body1 = entt::null;
            entt::entity body2 = entt::null;
        };

        struct ContactFrameData
        {
            std::vector<FrameContactEvent> entered = {};
            std::vector<FrameContactEvent> exited = {};
            std::vector<FrameContactEvent> stayed = {};
        };

        class ContactListenerImpl final : public JPH::ContactListener
        {
        public:
            JPH::ValidateResult OnContactValidate(
                const JPH::Body &inBody1,
                const JPH::Body &inBody2,
                JPH::RVec3Arg inBaseOffset,
                const JPH::CollideShapeResult &inCollisionResult) override
            {
                (void)inBaseOffset;
                (void)inCollisionResult;

                const JPH::CollisionGroup &group1 = inBody1.GetCollisionGroup();
                const JPH::CollisionGroup &group2 = inBody2.GetCollisionGroup();

                const u32 layer1 = static_cast<u32>(group1.GetGroupID());
                const u32 mask1 = static_cast<u32>(group1.GetSubGroupID());
                const u32 layer2 = static_cast<u32>(group2.GetGroupID());
                const u32 mask2 = static_cast<u32>(group2.GetSubGroupID());

                if ((mask1 & layer2) == 0u || (mask2 & layer1) == 0u)
                    return JPH::ValidateResult::RejectAllContactsForThisBodyPair;

                return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
            }

            void OnContactAdded(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
            {
                (void)ioSettings;
                RegisterContact(JPH::SubShapeIDPair(inBody1.GetID(), inManifold.mSubShapeID1, inBody2.GetID(), inManifold.mSubShapeID2), inBody1, inBody2);
            }

            void OnContactPersisted(const JPH::Body &inBody1, const JPH::Body &inBody2, const JPH::ContactManifold &inManifold, JPH::ContactSettings &ioSettings) override
            {
                (void)ioSettings;
                RegisterContact(JPH::SubShapeIDPair(inBody1.GetID(), inManifold.mSubShapeID1, inBody2.GetID(), inManifold.mSubShapeID2), inBody1, inBody2);
            }

            void OnContactRemoved(const JPH::SubShapeIDPair &inSubShapePair) override
            {
                std::scoped_lock lock(m_mutex);

                auto contactIt = m_subShapeContacts.find(inSubShapePair);
                if (contactIt == m_subShapeContacts.end())
                    return;

                const SubShapeContactData contact = contactIt->second;
                m_subShapeContacts.erase(contactIt);

                RemoveDirectedPair(inSubShapePair.GetBody1ID(), inSubShapePair.GetBody2ID(), contact.body1, contact.body2);
                RemoveDirectedPair(inSubShapePair.GetBody2ID(), inSubShapePair.GetBody1ID(), contact.body2, contact.body1);
            }

            ContactFrameData ConsumeFrameData()
            {
                std::scoped_lock lock(m_mutex);

                ContactFrameData frameData;
                frameData.entered.swap(m_enteredThisFrame);
                frameData.exited.swap(m_exitedThisFrame);
                frameData.stayed.reserve(m_activeDirectedPairs.size());

                for (const auto &entry : m_activeDirectedPairs)
                {
                    frameData.stayed.push_back(FrameContactEvent{
                        .self = entry.second.self,
                        .other = entry.second.other
                    });
                }

                return frameData;
            }

            void Reset()
            {
                std::scoped_lock lock(m_mutex);
                m_subShapeContacts.clear();
                m_activeDirectedPairs.clear();
                m_enteredThisFrame.clear();
                m_exitedThisFrame.clear();
            }

        private:
            void RegisterContact(const JPH::SubShapeIDPair &_pair, const JPH::Body &_body1, const JPH::Body &_body2)
            {
                std::scoped_lock lock(m_mutex);

                auto [contactIt, inserted] = m_subShapeContacts.emplace(_pair, SubShapeContactData{
                    .body1 = ToEntity(_body1.GetUserData()),
                    .body2 = ToEntity(_body2.GetUserData())
                });

                if (!inserted)
                    return;

                AddDirectedPair(_body1.GetID(), _body2.GetID(), contactIt->second.body1, contactIt->second.body2);
                AddDirectedPair(_body2.GetID(), _body1.GetID(), contactIt->second.body2, contactIt->second.body1);
            }

            void AddDirectedPair(const JPH::BodyID &_selfBodyID, const JPH::BodyID &_otherBodyID, entt::entity _selfEntity, entt::entity _otherEntity)
            {
                DirectedBodyPairKey key{
                    .selfBodyID = _selfBodyID,
                    .otherBodyID = _otherBodyID
                };

                auto pairIt = m_activeDirectedPairs.find(key);
                if (pairIt == m_activeDirectedPairs.end())
                {
                    m_activeDirectedPairs.emplace(key, ActiveDirectedPair{
                        .self = _selfEntity,
                        .other = _otherEntity,
                        .subShapeCount = 1
                    });

                    m_enteredThisFrame.push_back(FrameContactEvent{
                        .self = _selfEntity,
                        .other = _otherEntity
                    });
                    return;
                }

                pairIt->second.subShapeCount++;
            }

            void RemoveDirectedPair(const JPH::BodyID &_selfBodyID, const JPH::BodyID &_otherBodyID, entt::entity _selfEntity, entt::entity _otherEntity)
            {
                DirectedBodyPairKey key{
                    .selfBodyID = _selfBodyID,
                    .otherBodyID = _otherBodyID
                };

                auto pairIt = m_activeDirectedPairs.find(key);
                if (pairIt == m_activeDirectedPairs.end())
                    return;

                pairIt->second.subShapeCount--;
                if (pairIt->second.subShapeCount > 0)
                    return;

                m_exitedThisFrame.push_back(FrameContactEvent{
                    .self = _selfEntity,
                    .other = _otherEntity
                });
                m_activeDirectedPairs.erase(pairIt);
            }

            std::mutex m_mutex = {};
            std::unordered_map<JPH::SubShapeIDPair, SubShapeContactData> m_subShapeContacts = {};
            std::unordered_map<DirectedBodyPairKey, ActiveDirectedPair, DirectedBodyPairKeyHash> m_activeDirectedPairs = {};
            std::vector<FrameContactEvent> m_enteredThisFrame = {};
            std::vector<FrameContactEvent> m_exitedThisFrame = {};
        };

        struct BodyRuntimeData
        {
            JPH::BodyID bodyID;
            BodySettings settings;
            uint64_t seenFrame = 0, syncedWorldRevision = 0, syncedParentRevision = 0;
            Vector3 syncedLocalPosition = Vector3(0.0f);
            Quaternion syncedLocalRotation = Quaternion(Vector3(0.0f));
            bool hasSyncedTransform = false;
            Vector3 previousWorldPosition = Vector3(0.0f);
            Quaternion previousWorldRotation = Quaternion(Vector3(0.0f));
            Vector3 currentWorldPosition = Vector3(0.0f);
            Quaternion currentWorldRotation = Quaternion(Vector3(0.0f));
            bool hasPhysicsPose = false;
        };

        struct CachedTransform
        {
            uint64_t frame = 0, revision = 0, parentRevision = 0;
            entt::entity parent = entt::null;
            Vector3 localPosition{0}, localScale{1}, position{0}, scale{1};
            Quaternion localRotation{1,0,0,0}, rotation{1,0,0,0};
            bool prefixEnabled = false;
            Matrix4 prefix{1}, matrix{1};
        };
        std::unordered_map<entt::entity, CachedTransform> transforms;
        uint64_t syncFrame = 0, transformRevision = 0;
        struct CombinedSource
        {
            entt::entity handle;
            int model, node;
            bool apply;
            uint64_t revision;
            Matrix4 local;
            bool operator==(const CombinedSource& other) const
            {
                if(handle!=other.handle || model!=other.model || node!=other.node || apply!=other.apply || revision!=other.revision)return false;
                for(int column=0;column<4;column++)for(int row=0;row<4;row++)
                    if(std::abs(local[column][row]-other.local[column][row])>2e-5f)return false;
                return true;
            }
        };
        struct CombinedRuntime
        {
            std::vector<CombinedSource> sources;
            std::vector<CombinedSource> pendingSources;
            std::vector<entt::entity> traversal;
            JPH::RefConst<JPH::Shape> shape;
            Matrix4 rootBasis{0};
            float weld=-1;
            uint64_t request=0, revision=0;
        };
        std::unordered_map<entt::entity,CombinedRuntime> combinedMeshes;
        struct VehicleRuntime
        {
            JPH::Ref<JPH::VehicleConstraint> constraint;
            JPH::BodyID body;
            std::array<float,6> geometry;
            std::array<JPH::Ref<JPH::WheelSettingsWV>,4> wheels;
            std::array<float,11> tuning{};
            float throttle=0,steering=0;
        };
        std::unordered_map<entt::entity,VehicleRuntime> vehicles;
        void RemoveVehicle(entt::entity handle)
        {
            auto it=vehicles.find(handle);if(it==vehicles.end())return;
            physicsSystem->RemoveStepListener(it->second.constraint.GetPtr());
            physicsSystem->RemoveConstraint(it->second.constraint.GetPtr());vehicles.erase(it);
        }
        void SyncVehicles(entt::registry& registry,float dt)
        {
            std::vector<entt::entity> stale;
            for(auto& [handle,runtime]:vehicles)
                if(!registry.valid(handle)||!registry.all_of<WheeledVehicle,Rigidbody>(handle)||
                    registry.get<Rigidbody>(handle).motionType!=RigidbodyMotionType::DYNAMIC||
                    !bodies.contains(handle)||bodies.at(handle).bodyID!=runtime.body)stale.push_back(handle);
            for(auto handle:stale)RemoveVehicle(handle);
            for(auto handle:registry.view<WheeledVehicle,Rigidbody,Transform>())
            {
                auto& vehicle=registry.get<WheeledVehicle>(handle);
                auto found=bodies.find(handle);if(found==bodies.end())continue;
                if(registry.get<Rigidbody>(handle).motionType!=RigidbodyMotionType::DYNAMIC)continue;
                std::array<float,6> geometry{vehicle.wheelRadius,vehicle.wheelWidth,vehicle.axleWidth,vehicle.wheelbase,vehicle.suspensionMin,vehicle.suspensionMax};
                if(auto it=vehicles.find(handle);it!=vehicles.end() && it->second.geometry!=geometry)RemoveVehicle(handle);
                if(!vehicles.contains(handle))
                {
                    JPH::VehicleConstraintSettings configuration;configuration.mForward=JPH::Vec3(0,0,-1);
                    auto* controller=new JPH::WheeledVehicleControllerSettings();controller->mEngine.mMaxTorque=std::max(1.f,vehicle.engineTorque);
                    for(int axle=0;axle<2;axle++)
                    {
                        JPH::VehicleDifferentialSettings differential;differential.mLeftWheel=axle*2;differential.mRightWheel=axle*2+1;differential.mEngineTorqueRatio=.5f;
                        controller->mDifferentials.push_back(differential);
                        JPH::VehicleAntiRollBar bar;bar.mLeftWheel=axle*2;bar.mRightWheel=axle*2+1;bar.mStiffness=0;
                        configuration.mAntiRollBars.push_back(bar);
                    }
                    configuration.mController=controller;
                    std::array<JPH::Ref<JPH::WheelSettingsWV>,4> wheelSettings;
                    for(int index=0;index<4;index++)
                    {
                        auto* wheel=new JPH::WheelSettingsWV();
                        wheelSettings[index]=wheel;
                        wheel->mPosition=JPH::Vec3((index%2?1:-1)*vehicle.axleWidth*.5f,.05f,(index<2?-1:1)*vehicle.wheelbase*.5f);
                        wheel->mWheelForward=JPH::Vec3(0,0,-1);
                        wheel->mRadius=std::max(.05f,vehicle.wheelRadius);wheel->mWidth=std::max(.03f,vehicle.wheelWidth);
                        wheel->mSuspensionMinLength=std::max(.02f,vehicle.suspensionMin);
                        wheel->mSuspensionMaxLength=std::max(wheel->mSuspensionMinLength+.01f,vehicle.suspensionMax);
                        wheel->mSuspensionSpring.mFrequency=std::max(.1f,vehicle.springFrequency);wheel->mSuspensionSpring.mDamping=std::max(0.f,vehicle.springDamping);
                        wheel->mMaxSteerAngle=glm::radians(vehicle.steeringDegrees)*(index<2?1.f:vehicle.rearSteerRatio);
                        wheel->mMaxHandBrakeTorque=index<2?0:4000;
                        configuration.mWheels.push_back(wheel);
                    }
                    JPH::BodyLockWrite lock(physicsSystem->GetBodyLockInterface(),found->second.bodyID);if(!lock.Succeeded())continue;
                    auto* constraint=new JPH::VehicleConstraint(lock.GetBody(),configuration);
                    constraint->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterCastCylinder(ToObjectLayer(JPH::EMotionType::Dynamic)));
                    physicsSystem->AddConstraint(constraint);physicsSystem->AddStepListener(constraint);
                    vehicles.emplace(handle,VehicleRuntime{constraint,found->second.bodyID,geometry,wheelSettings});
                    ++vehicle.rebuildCount;
                }
                auto& runtime=vehicles.at(handle);
                auto* controller=static_cast<JPH::WheeledVehicleController*>(runtime.constraint->GetController());
                std::array<float,11> tuning{vehicle.springFrequency,vehicle.springDamping,vehicle.steeringDegrees,vehicle.rearSteerRatio,
                    vehicle.engineTorque,vehicle.engineMaxRPM,vehicle.brakeTorque,vehicle.handbrakeTorque,vehicle.longitudinalGrip,vehicle.lateralGrip,vehicle.antiRollStiffness};
                if(runtime.tuning!=tuning)
                {
                    // These wheel settings are owned by this constraint. Updating
                    // them between simulation steps preserves wheel spin and RPM.
                    controller->GetEngine().mMaxTorque=std::max(0.f,vehicle.engineTorque);
                    controller->GetEngine().mMaxRPM=std::max(1001.f,vehicle.engineMaxRPM);
                    controller->GetEngine().ClampRPM();
                    for(int i=0;i<4;i++)
                    {
                        auto& wheel=*runtime.wheels[i];
                        wheel.mSuspensionSpring.mFrequency=std::max(.1f,vehicle.springFrequency);
                        wheel.mSuspensionSpring.mDamping=std::max(0.f,vehicle.springDamping);
                        wheel.mMaxSteerAngle=glm::radians(std::clamp(vehicle.steeringDegrees,0.f,80.f))*(i<2?1.f:std::clamp(vehicle.rearSteerRatio,-1.f,1.f));
                        wheel.mMaxBrakeTorque=std::max(0.f,vehicle.brakeTorque);
                        wheel.mMaxHandBrakeTorque=i<2?0:std::max(0.f,vehicle.handbrakeTorque);
                        const float longitudinal=std::max(0.f,vehicle.longitudinalGrip),lateral=std::max(0.f,vehicle.lateralGrip);
                        wheel.mLongitudinalFriction.Clear();wheel.mLongitudinalFriction.AddPoint(0,0);
                        wheel.mLongitudinalFriction.AddPoint(.06f,1.2f*longitudinal);wheel.mLongitudinalFriction.AddPoint(.2f,longitudinal);
                        wheel.mLateralFriction.Clear();wheel.mLateralFriction.AddPoint(0,0);
                        wheel.mLateralFriction.AddPoint(3,1.2f*lateral);wheel.mLateralFriction.AddPoint(20,lateral);
                    }
                    for(auto& bar:runtime.constraint->GetAntiRollBars())bar.mStiffness=std::max(0.f,vehicle.antiRollStiffness);
                    runtime.tuning=tuning;
                    bodyInterface->ActivateBody(found->second.bodyID);
                }
                float speedKph=bodyInterface->GetLinearVelocity(found->second.bodyID).Length()*3.6f;
                float steeringRatio=glm::mix(1.f,std::clamp(vehicle.highSpeedSteeringRatio,0.f,1.f),
                    std::clamp(speedKph/std::max(1.f,vehicle.steeringFadeSpeedKph),0.f,1.f));
                auto respond=[dt](float previous,float target,float rate) {
                    return rate>0?glm::mix(previous,target,1.f-std::exp(-rate*std::max(0.f,dt))):target;
                };
                runtime.throttle=vehicle.brake>0?0:respond(runtime.throttle,std::clamp(vehicle.throttle,-1.f,1.f),vehicle.throttleResponse);
                runtime.steering=respond(runtime.steering,std::clamp(vehicle.steering,-1.f,1.f)*steeringRatio,vehicle.steeringResponse);
                vehicle.appliedThrottle=runtime.throttle;vehicle.appliedSteering=runtime.steering;
                controller->SetDriverInput(runtime.throttle,runtime.steering,std::clamp(vehicle.brake,0.f,1.f),std::clamp(vehicle.handbrake,0.f,1.f));
                if(std::abs(vehicle.throttle)>.01f||std::abs(vehicle.steering)>.01f)bodyInterface->ActivateBody(found->second.bodyID);
            }
        }
        void SyncVehicleWheels(entt::registry& registry)
        {
            for(auto& [handle,runtime]:vehicles)
            {
                auto& vehicle=registry.get<WheeledVehicle>(handle);vehicle.groundedWheels=0;
                vehicle.speed=bodyInterface->GetLinearVelocity(runtime.body).Length();
                const auto center=bodyInterface->GetShape(runtime.body)->GetCenterOfMass();
                vehicle.effectiveCenterOfMass={center.GetX(),center.GetY(),center.GetZ()};
                for(int i=0;i<4;i++)
                {
                    if(runtime.constraint->GetWheel(i)->HasContact())++vehicle.groundedWheels;
                    if(i>=int(vehicle.wheels.size())||!vehicle.wheels[i]||!vehicle.wheels[i].HasComponent<Transform>())continue;
                    auto matrix=runtime.constraint->GetWheelLocalTransform(i,JPH::Vec3(1,0,0),JPH::Vec3(0,1,0));
                    auto& transform=vehicle.wheels[i].GetComponent<Transform>();
                    auto position=matrix.GetTranslation();transform.position={position.GetX(),position.GetY(),position.GetZ()};
                    transform.rotation=ToCanisRotation(matrix.GetRotation().GetQuaternion());
                }
            }
        }

        JPH::RefConst<JPH::Shape> CombinedShape(entt::registry& registry,entt::entity root,Transform& rootTransform,CombineMeshCollider& component)
        {
            const auto& world=WorldTransform(registry,root,rootTransform);
            const Matrix4 inverse=glm::inverse(world.matrix);
            auto& cache=combinedMeshes[root];
            auto& sources=cache.pendingSources;sources.clear();
            auto& stack=cache.traversal;stack.clear();
            for(auto child:rootTransform.children)if(child)stack.push_back(child.GetHandle());
            while(!stack.empty())
            {
                auto child=stack.back();stack.pop_back();
                auto* transform=registry.try_get<Transform>(child);
                if(!transform || !transform->IsActiveInHierarchy())continue;
                if(auto* body=registry.try_get<Rigidbody>(child);body && body->motionType!=RigidbodyMotionType::STATIC)continue;
                if(registry.all_of<CombineMeshCollider>(child))continue;
                if(auto* model=registry.try_get<Model>(child))
                    if(auto* asset=AssetManager::GetModel(model->modelId);asset && !asset->HasDeformingGeometry() && !registry.all_of<ModelAnimation>(child))
                        sources.push_back({child,model->modelId,model->nodeIndex,model->applyNodeTransform,asset->GetGeometryRevision(),inverse*WorldTransform(registry,child,*transform).matrix});
                for(auto descendant:transform->children)if(descendant)stack.push_back(descendant.GetHandle());
            }
            std::sort(sources.begin(),sources.end(),[](auto& a,auto& b){return entt::to_integral(a.handle)<entt::to_integral(b.handle);});
            Matrix4 rigid=glm::translate(Matrix4(1),world.position)*glm::mat4_cast(world.rotation);
            Matrix4 rootScale=glm::inverse(rigid)*world.matrix;
            bool basisMatches=true;
            for(int column=0;column<4;column++)for(int row=0;row<4;row++)
                if(std::abs(rootScale[column][row]-cache.rootBasis[column][row])>2e-5f)basisMatches=false;
            if(cache.revision && sources==cache.sources && basisMatches && cache.weld==component.weldDistance && cache.request==component.rebuildVersion)return cache.shape;
            Profiler::Scope timing("Combined collider build",Profiler::Category::Physics);
            JPH::TriangleList triangles;
            const float weld=std::max(0.f,component.weldDistance);
            for(auto& source:sources)
            {
                const bool mirrored=glm::determinant(glm::mat3(rootScale*source.local))<0;
                std::vector<Vector3> vertices;std::vector<u32> indices;
                if(!AssetManager::GetModel(source.model)->BuildTriangleMesh(vertices,indices,source.node,source.apply))continue;
                for(auto& vertex:vertices)
                {
                    vertex=Vector3(rootScale*source.local*Vector4(vertex,1));
                    if(weld>.000001f)vertex=glm::round(vertex/weld)*weld;
                }
                for(size_t i=0;i+2<indices.size();i+=3)
                {
                    auto a=vertices[indices[i]],b=vertices[indices[i+1]],c=vertices[indices[i+2]];
                    if(mirrored)std::swap(b,c);
                    if(glm::length(glm::cross(b-a,c-a))<1e-8f)continue;
                    triangles.push_back(JPH::Triangle(JPH::Float3(a.x,a.y,a.z),JPH::Float3(b.x,b.y,b.z),JPH::Float3(c.x,c.y,c.z)));
                }
            }
            component.sourceModels=int(sources.size());component.triangleCount=int(triangles.size());++component.rebuildCount;
            cache.sources.swap(sources);cache.rootBasis=rootScale;cache.weld=component.weldDistance;cache.request=component.rebuildVersion;++cache.revision;
            if(triangles.empty()){cache.shape=nullptr;return nullptr;}
            auto result=JPH::MeshShapeSettings(triangles).Create();
            if(result.HasError()){Debug::Warning("CombineMeshCollider: %s",result.GetError().c_str());cache.shape=nullptr;}
            else cache.shape=result.Get();
            return cache.shape;
        }

        const CachedTransform& WorldTransform(entt::registry& registry, entt::entity handle, const Transform& transform)
        {
            auto& cached = transforms[handle];
            if (cached.frame == syncFrame) return cached;
            const CachedTransform* parent = nullptr;
            entt::entity parentHandle = entt::null;
            parentHandle = transform.parent.GetHandle();
            if (const auto* parentTransform = registry.try_get<Transform>(parentHandle)) {
                parent = &WorldTransform(registry, parentHandle, *parentTransform);
            } else {
                parentHandle = entt::null;
            }
            const auto parentRevision = parent ? parent->revision : 0;
            if (!cached.revision || cached.parent != parentHandle || cached.parentRevision != parentRevision ||
                cached.localPosition != transform.position || cached.localRotation != transform.rotation ||
                cached.localScale != transform.scale || cached.prefixEnabled != transform.useLocalMatrixPrefix ||
                (transform.useLocalMatrixPrefix && cached.prefix != transform.localMatrixPrefix))
            {
                cached.localPosition = transform.position;
                cached.localRotation = transform.rotation;
                cached.localScale = transform.scale;
                cached.parent = parentHandle; cached.parentRevision = parentRevision;
                cached.prefixEnabled = transform.useLocalMatrixPrefix; cached.prefix = transform.localMatrixPrefix;
                cached.matrix = parent ? parent->matrix * transform.GetLocalMatrix() : transform.GetLocalMatrix();
                cached.position = Vector3(cached.matrix[3]);
                glm::mat3 rotation(cached.matrix);
                for (int column = 0; column < 3; ++column) {
                    cached.scale[column] = glm::length(Vector3(rotation[column]));
                    if (cached.scale[column] > 0.000001f) rotation[column] /= cached.scale[column];
                }
                if (glm::determinant(rotation) < 0.0f) rotation[0] = -rotation[0];
                cached.rotation = glm::normalize(glm::quat_cast(rotation));
                cached.revision = ++transformRevision;
            }
            cached.frame = syncFrame;
            return cached;
        }

        std::unique_ptr<BPLayerInterfaceImpl> broadPhaseLayerInterface = nullptr;
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> objectVsBroadPhaseLayerFilter = nullptr;
        std::unique_ptr<ObjectLayerPairFilterImpl> objectLayerPairFilter = nullptr;
        std::unique_ptr<ContactListenerImpl> contactListener = nullptr;
        std::unique_ptr<JPH::PhysicsSystem> physicsSystem = nullptr;
        std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator = nullptr;
        std::unique_ptr<JPH::JobSystemThreadPool> jobSystem = nullptr;
        JPH::BodyInterface *bodyInterface = nullptr;
        std::unordered_map<entt::entity, BodyRuntimeData> bodies = {};
        std::vector<entt::entity> movingBodies, staleBodies;
        float accumulator = 0.0f;
        bool initialized = false;

        void Create()
        {
            if (initialized)
                return;

            InitJoltGlobal();

            broadPhaseLayerInterface = std::make_unique<BPLayerInterfaceImpl>();
            objectVsBroadPhaseLayerFilter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
            objectLayerPairFilter = std::make_unique<ObjectLayerPairFilterImpl>();

            physicsSystem = std::make_unique<JPH::PhysicsSystem>();
            physicsSystem->Init(
                kMaxBodies,
                kNumBodyMutexes,
                kMaxBodyPairs,
                kMaxContactConstraints,
                *broadPhaseLayerInterface,
                *objectVsBroadPhaseLayerFilter,
                *objectLayerPairFilter);

            contactListener = std::make_unique<ContactListenerImpl>();
            physicsSystem->SetContactListener(contactListener.get());

            tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);

            const JPH::uint workerCount = (std::thread::hardware_concurrency() > 1)
                ? (std::thread::hardware_concurrency() - 1)
                : 1;
            jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, workerCount);
            bodyInterface = &physicsSystem->GetBodyInterface();
            accumulator = 0.0f;
            initialized = true;
        }

        void Destroy()
        {
            if (!initialized)
                return;

            while(!vehicles.empty())RemoveVehicle(vehicles.begin()->first);

            for (auto &entry : bodies)
                RemoveBodyFromWorld(entry.second.bodyID);
            bodies.clear();
            transforms.clear();
            movingBodies.clear();staleBodies.clear();
            combinedMeshes.clear();

            bodyInterface = nullptr;
            if (contactListener != nullptr)
            {
                contactListener->Reset();
                if (physicsSystem != nullptr)
                    physicsSystem->SetContactListener(nullptr);
            }
            contactListener.reset();
            jobSystem.reset();
            tempAllocator.reset();
            physicsSystem.reset();
            objectLayerPairFilter.reset();
            objectVsBroadPhaseLayerFilter.reset();
            broadPhaseLayerInterface.reset();
            accumulator = 0.0f;

            ShutdownJoltGlobal();
            initialized = false;
        }

        void RemoveBodyFromWorld(const JPH::BodyID &_bodyID)
        {
            if (bodyInterface == nullptr || _bodyID.IsInvalid())
                return;

            if (bodyInterface->IsAdded(_bodyID))
                bodyInterface->RemoveBody(_bodyID);
            bodyInterface->DestroyBody(_bodyID);
        }

        void RemoveBody(entt::entity _entityHandle)
        {
            RemoveVehicle(_entityHandle);
            auto bodyIt = bodies.find(_entityHandle);
            if (bodyIt == bodies.end())
                return;

            RemoveBodyFromWorld(bodyIt->second.bodyID);
            bodies.erase(bodyIt);
        }

        void ClearPendingForces(Rigidbody &_rigidbody)
        {
            _rigidbody.pendingForce = Vector3(0.0f);
            _rigidbody.pendingAcceleration = Vector3(0.0f);
            _rigidbody.pendingImpulse = Vector3(0.0f);
            _rigidbody.pendingVelocityChange = Vector3(0.0f);
        }

        void ClearPendingLinearVelocity(Rigidbody &_rigidbody)
        {
            _rigidbody.pendingLinearVelocity = Vector3(0.0f);
            _rigidbody.hasPendingLinearVelocity = false;
        }

        void ApplyPendingLinearVelocity(const JPH::BodyID &_bodyID, Rigidbody &_rigidbody)
        {
            if (!_rigidbody.hasPendingLinearVelocity ||
                bodyInterface == nullptr ||
                _bodyID.IsInvalid() ||
                !bodyInterface->IsAdded(_bodyID))
            {
                return;
            }

            bodyInterface->SetLinearVelocity(_bodyID, ToJoltVec3(_rigidbody.pendingLinearVelocity));
            bodyInterface->ActivateBody(_bodyID);
            ClearPendingLinearVelocity(_rigidbody);
        }

        void ClearPendingAngularVelocity(Rigidbody &_rigidbody)
        {
            _rigidbody.pendingAngularVelocity = Vector3(0.0f);
            _rigidbody.hasPendingAngularVelocity = false;
        }

        void ApplyPendingAngularVelocity(const JPH::BodyID &_bodyID, Rigidbody &_rigidbody)
        {
            if (!_rigidbody.hasPendingAngularVelocity ||
                bodyInterface == nullptr ||
                _bodyID.IsInvalid() ||
                !bodyInterface->IsAdded(_bodyID))
            {
                return;
            }

            bodyInterface->SetAngularVelocity(_bodyID, ToJoltVec3(_rigidbody.pendingAngularVelocity));
            bodyInterface->ActivateBody(_bodyID);
            ClearPendingAngularVelocity(_rigidbody);
        }

        void ClearPendingKinematicMotion(Rigidbody &_rigidbody)
        {
            _rigidbody.hasPendingKinematicStop = false;
            _rigidbody.pendingTranslation = Vector3(0.0f);
            _rigidbody.pendingRotation = Quaternion(Vector3(0.0f));
        }

        void ApplyPendingKinematicMotion(const JPH::BodyID &_bodyID, Rigidbody &_rigidbody, float _deltaTime)
        {
            if (bodyInterface == nullptr || _bodyID.IsInvalid() || !bodyInterface->IsAdded(_bodyID))
                return;

            const bool hasTranslation = !NearlyZero(_rigidbody.pendingTranslation);
            const Quaternion identity = Quaternion(Vector3(0.0f));
            const bool hasRotation = std::abs(glm::dot(_rigidbody.pendingRotation, identity)) < 0.999999f;
            if (!hasTranslation && !hasRotation && !_rigidbody.hasPendingKinematicStop)
                return;

            const JPH::RVec3 currentPosition = bodyInterface->GetPosition(_bodyID);
            const Quaternion currentRotation = ToCanisRotation(bodyInterface->GetRotation(_bodyID));
            const Vector3 targetPosition = ToCanisPosition(currentPosition) + _rigidbody.pendingTranslation;
            const Quaternion targetRotation = glm::normalize(_rigidbody.pendingRotation * currentRotation);

            bodyInterface->MoveKinematic(
                _bodyID,
                ToJoltPosition(targetPosition),
                ToJoltRotation(targetRotation),
                glm::max(_deltaTime, 0.000001f));
            ClearPendingKinematicMotion(_rigidbody);
        }

        void ApplyPendingForces(const JPH::BodyID &_bodyID, Rigidbody &_rigidbody)
        {
            if (bodyInterface == nullptr || _bodyID.IsInvalid() || !bodyInterface->IsAdded(_bodyID))
                return;

            bool applied = false;
            const float mass = glm::max(0.001f, _rigidbody.mass);

            if (!NearlyZero(_rigidbody.pendingForce))
            {
                bodyInterface->AddForce(_bodyID, ToJoltVec3(_rigidbody.pendingForce));
                _rigidbody.pendingForce = Vector3(0.0f);
                applied = true;
            }

            if (!NearlyZero(_rigidbody.pendingAcceleration))
            {
                bodyInterface->AddForce(_bodyID, ToJoltVec3(_rigidbody.pendingAcceleration * mass));
                _rigidbody.pendingAcceleration = Vector3(0.0f);
                applied = true;
            }

            if (!NearlyZero(_rigidbody.pendingImpulse))
            {
                bodyInterface->AddImpulse(_bodyID, ToJoltVec3(_rigidbody.pendingImpulse));
                _rigidbody.pendingImpulse = Vector3(0.0f);
                applied = true;
            }

            if (!NearlyZero(_rigidbody.pendingVelocityChange))
            {
                bodyInterface->AddImpulse(_bodyID, ToJoltVec3(_rigidbody.pendingVelocityChange * mass));
                _rigidbody.pendingVelocityChange = Vector3(0.0f);
                applied = true;
            }

            if (applied)
                bodyInterface->ActivateBody(_bodyID);
        }

        BodyRuntimeData* EnsureBodyForEntity(entt::registry &_registry, entt::entity _entityHandle)
        {
            Transform *transform = _registry.try_get<Transform>(_entityHandle);
            Rigidbody *rigidbody = _registry.try_get<Rigidbody>(_entityHandle);
            auto* combined=_registry.try_get<CombineMeshCollider>(_entityHandle);
            Rigidbody combinedBody;
            if(combined)
            {
                combinedBody.motionType=RigidbodyMotionType::STATIC;combinedBody.useGravity=false;
                combinedBody.active=combined->active;combinedBody.friction=combined->friction;
                rigidbody=&combinedBody;
            }
            if (transform == nullptr || rigidbody == nullptr)
            {
                RemoveBody(_entityHandle);
                return nullptr;
            }

            Entity *entity = rigidbody->entity != nullptr ? rigidbody->entity : transform->entity;
            const auto* metadata = _registry.try_get<EntityMetadata>(_entityHandle);
            if (entity == nullptr || !metadata || !metadata->active || !rigidbody->active)
            {
                RemoveBody(_entityHandle);
                return nullptr;
            }

            BoxCollider *boxCollider = _registry.try_get<BoxCollider>(_entityHandle);
            if (boxCollider != nullptr && !boxCollider->active)
                boxCollider = nullptr;
            SphereCollider *sphereCollider = _registry.try_get<SphereCollider>(_entityHandle);
            if (sphereCollider != nullptr && !sphereCollider->active)
                sphereCollider = nullptr;
            CapsuleCollider *capsuleCollider = _registry.try_get<CapsuleCollider>(_entityHandle);
            if (capsuleCollider != nullptr && !capsuleCollider->active)
                capsuleCollider = nullptr;
            MeshCollider *meshCollider = _registry.try_get<MeshCollider>(_entityHandle);
            if (meshCollider != nullptr && !meshCollider->active)
                meshCollider = nullptr;
            ConvexMeshCollider *convexMeshCollider = _registry.try_get<ConvexMeshCollider>(_entityHandle);
            if (convexMeshCollider != nullptr && !convexMeshCollider->active)
                convexMeshCollider = nullptr;

            if (boxCollider == nullptr && sphereCollider == nullptr && capsuleCollider == nullptr && meshCollider == nullptr && convexMeshCollider == nullptr && combined==nullptr)
            {
                RemoveBody(_entityHandle);
                return nullptr;
            }

            const auto& world = WorldTransform(_registry, _entityHandle, *transform);
            JPH::RefConst<JPH::Shape> combinedShape;
            if(combined)combinedShape=CombinedShape(_registry,_entityHandle,*transform,*combined);
            auto settings = CaptureSettings(_registry, _entityHandle, world.scale, *rigidbody,
                boxCollider, sphereCollider, capsuleCollider, meshCollider, convexMeshCollider);
            if(!combined && rigidbody->motionType==RigidbodyMotionType::DYNAMIC)
            if(auto* vehicle=_registry.try_get<WheeledVehicle>(_entityHandle);vehicle && vehicle->centerOfMass)
            {
                // Walk local transforms instead of subtracting world positions:
                // this remains stable as the vehicle moves and rotates.
                auto marker=vehicle->centerOfMass;
                Matrix4 relative(1);
                for(int depth=0;depth<128 && marker && &marker.scene.GetRegistry()==&_registry;depth++)
                {
                    if(marker.GetHandle()==_entityHandle)
                    {
                        if(depth>0)
                        {
                            Vector3 centre=glm::mat3_cast(glm::conjugate(world.rotation))*glm::mat3(world.matrix)*Vector3(relative[3]);
                            if(std::isfinite(centre.x)&&std::isfinite(centre.y)&&std::isfinite(centre.z))
                            {settings.customCenterOfMass=true;settings.centerOfMass=centre;}
                        }
                        break;
                    }
                    auto* markerTransform=marker.TryGetComponent<Transform>();if(!markerTransform)break;
                    relative=markerTransform->GetLocalMatrix()*relative;
                    marker=markerTransform->parent;
                }
            }
            if(combined){settings.shape=6;settings.geometryRevision=combinedMeshes.at(_entityHandle).revision;}
            auto bodyIt = bodies.find(_entityHandle);
            if (bodyIt != bodies.end() && bodyIt->second.settings == settings) {
                bodyIt->second.seenFrame = syncFrame;
                return &bodyIt->second;
            }

            Profiler::Scope rebuildTiming(bodyIt != bodies.end() ? "Physics body rebuild" : "Physics body creation",
                Profiler::Category::Physics);
            if (bodyIt != bodies.end())
            {
                // Erase the retired ID too: an empty/invalid replacement shape
                // must not try to destroy that same Jolt body a second time.
                RemoveBody(_entityHandle);
            }

            JPH::RefConst<JPH::Shape> shape = combined ? combinedShape : BuildShape(
                _registry,
                _entityHandle,
                *transform,
                boxCollider,
                sphereCollider,
                capsuleCollider,
                meshCollider,
                convexMeshCollider);
            if (shape == nullptr)
            {
                RemoveBody(_entityHandle);
                return nullptr;
            }

            if(settings.customCenterOfMass)
            {
                // Jolt expects a shift from the shape's automatic centre, while
                // the marker specifies an absolute point in chassis coordinates.
                auto result=JPH::OffsetCenterOfMassShapeSettings(
                    ToJoltVec3(settings.centerOfMass)-shape->GetCenterOfMass(),shape.GetPtr()).Create();
                if(result.HasError())
                {
                    Debug::Warning("Vehicle centre of mass: %s",result.GetError().c_str());
                    return nullptr;
                }
                shape=result.Get();
            }

            const JPH::EMotionType motionType = ToMotionType(rigidbody->motionType);
            JPH::BodyCreationSettings bodySettings(
                shape.GetPtr(),
                ToJoltPosition(world.position),
                ToJoltRotation(world.rotation),
                motionType,
                ToObjectLayer(motionType));

            bodySettings.mUserData = ToBodyUserData(_entityHandle);
            bodySettings.mAllowSleeping = rigidbody->allowSleeping;
            bodySettings.mIsSensor = rigidbody->isSensor;
            bodySettings.mCollisionGroup = JPH::CollisionGroup(
                nullptr,
                rigidbody->layer,
                rigidbody->mask);
            bodySettings.mFriction = glm::max(0.0f, rigidbody->friction);
            bodySettings.mRestitution = glm::max(0.0f, rigidbody->restitution);
            bodySettings.mLinearDamping = glm::max(0.0f, rigidbody->linearDamping);
            bodySettings.mAngularDamping = glm::max(0.0f, rigidbody->angularDamping);
            bodySettings.mGravityFactor = rigidbody->useGravity ? glm::max(0.0f, rigidbody->gravityFactor) : 0.0f;
            bodySettings.mAllowedDOFs = BuildAllowedDOFs(*rigidbody);
            bodySettings.mLinearVelocity = ToJoltVec3(rigidbody->linearVelocity);
            bodySettings.mAngularVelocity = ToJoltVec3(rigidbody->angularVelocity);

            if (motionType == JPH::EMotionType::Dynamic)
            {
                bodySettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                bodySettings.mMassPropertiesOverride.mMass = glm::max(0.001f, rigidbody->mass);
            }

            const JPH::EActivation activation = (motionType == JPH::EMotionType::Static)
                ? JPH::EActivation::DontActivate
                : JPH::EActivation::Activate;

            const JPH::BodyID bodyID = bodyInterface->CreateAndAddBody(bodySettings, activation);
            if (bodyID.IsInvalid())
            {
                Debug::Log("JoltPhysics3DSystem: failed to create body for entity '%s'.", entity->GetName().c_str());
                RemoveBody(_entityHandle);
                return nullptr;
            }

            BodyRuntimeData runtimeData;
            runtimeData.bodyID = bodyID;
            runtimeData.settings = BodySettings(settings);
            runtimeData.seenFrame = syncFrame;
            runtimeData.syncedWorldRevision = world.revision;
            runtimeData.syncedParentRevision = world.parentRevision;
            runtimeData.syncedLocalPosition = transform->position;
            runtimeData.syncedLocalRotation = transform->rotation;
            runtimeData.hasSyncedTransform = true;
            runtimeData.previousWorldPosition = world.position;
            runtimeData.previousWorldRotation = world.rotation;
            runtimeData.currentWorldPosition = runtimeData.previousWorldPosition;
            runtimeData.currentWorldRotation = runtimeData.previousWorldRotation;
            runtimeData.hasPhysicsPose = true;
            bodies[_entityHandle] = runtimeData;
            return &bodies.at(_entityHandle);
        }

        void SyncCombinedColliders(entt::registry& _registry)
        {
            for(auto handle:_registry.view<CombineMeshCollider,Transform>())
            {
                if(auto* runtime=EnsureBodyForEntity(_registry,handle))
                {
                    const auto& world=transforms.at(handle);
                    if(runtime->syncedWorldRevision!=world.revision)
                    {
                        bodyInterface->SetPositionAndRotation(runtime->bodyID,ToJoltPosition(world.position),ToJoltRotation(world.rotation),JPH::EActivation::DontActivate);
                        runtime->syncedWorldRevision=world.revision;
                    }
                }
            }
            std::erase_if(combinedMeshes,[&](const auto& item){
                if(_registry.valid(item.first)&&_registry.all_of<CombineMeshCollider,Transform>(item.first))return false;
                RemoveBody(item.first);return true;
            });
        }

        const std::vector<entt::entity>& SyncBodiesBeforeStep(entt::registry &_registry, float _physicsDeltaTime)
        {
            Profiler::Scope timing("Physics body synchronization", Profiler::Category::Physics);
            movingBodies.clear();
            ++syncFrame;
            SyncCombinedColliders(_registry);

            auto rigidbodyView = _registry.view<Rigidbody, Transform>();
            for (const entt::entity entityHandle : rigidbodyView)
            {
                if(_registry.all_of<CombineMeshCollider>(entityHandle))continue;
                bool combinedChild=false;
                auto ancestor=rigidbodyView.get<Transform>(entityHandle).parent;
                for(int depth=0;ancestor && depth<128;depth++)
                {
                    if(ancestor.HasComponent<CombineMeshCollider>() && ancestor.GetComponent<CombineMeshCollider>().active){combinedChild=true;break;}
                    auto* parent=ancestor.TryGetComponent<Transform>();if(!parent)break;ancestor=parent->parent;
                }
                if(combinedChild && rigidbodyView.get<Rigidbody>(entityHandle).motionType==RigidbodyMotionType::STATIC)continue;
                BodyRuntimeData* runtime = EnsureBodyForEntity(_registry, entityHandle);
                if (!runtime)
                    continue;
                Transform* transform = &rigidbodyView.get<Transform>(entityHandle);
                Rigidbody* rigidbody = &rigidbodyView.get<Rigidbody>(entityHandle);
                BodyRuntimeData &runtimeData = *runtime;
                const JPH::EMotionType motionType = ToMotionType(rigidbody->motionType);
                if(motionType!=JPH::EMotionType::Static)movingBodies.push_back(entityHandle);

                const bool transformEdited = !runtimeData.hasSyncedTransform
                    || !NearlyEqual(transform->position, runtimeData.syncedLocalPosition)
                    || !NearlyEqual(transform->rotation, runtimeData.syncedLocalRotation);

                // Kinematic bodies are advanced by MoveKinematic. Reapplying the
                // interpolated Transform every pre-step resets that motion before
                // the next command can accumulate. Only push a kinematic pose
                // when game/editor code explicitly changed its Transform.
                const auto& world = transforms.at(entityHandle);
                const bool staticMoved = motionType == JPH::EMotionType::Static &&
                    (!runtimeData.hasPhysicsPose || runtimeData.syncedWorldRevision != world.revision);
                // Moving a door container must also move its kinematic child colliders.
                // Track the parent independently so physics interpolation of the
                // body's own Transform does not cancel MoveKinematic commands.
                const bool parentMoved = motionType == JPH::EMotionType::Kinematic &&
                    runtimeData.syncedParentRevision != world.parentRevision;
                if (staticMoved || transformEdited || parentMoved)
                {
                    const JPH::EActivation activation = (motionType == JPH::EMotionType::Static)
                        ? JPH::EActivation::DontActivate
                        : JPH::EActivation::Activate;
                    bodyInterface->SetPositionAndRotation(
                        runtimeData.bodyID,
                        ToJoltPosition(world.position),
                        ToJoltRotation(world.rotation),
                        activation);

                    runtimeData.syncedLocalPosition = transform->position;
                    runtimeData.syncedLocalRotation = transform->rotation;
                    runtimeData.hasSyncedTransform = true;
                    runtimeData.previousWorldPosition = world.position;
                    runtimeData.previousWorldRotation = world.rotation;
                    runtimeData.currentWorldPosition = runtimeData.previousWorldPosition;
                    runtimeData.currentWorldRotation = runtimeData.previousWorldRotation;
                    runtimeData.hasPhysicsPose = true;
                    runtimeData.syncedWorldRevision = world.revision;
                    runtimeData.syncedParentRevision = world.parentRevision;
                }

                if (motionType == JPH::EMotionType::Dynamic)
                {
                    if (_physicsDeltaTime > 0.0f)
                    {
                        ApplyPendingLinearVelocity(runtimeData.bodyID, *rigidbody);
                        ApplyPendingAngularVelocity(runtimeData.bodyID, *rigidbody);
                    }
                    ApplyPendingForces(runtimeData.bodyID, *rigidbody);
                    ClearPendingKinematicMotion(*rigidbody);
                }
                else if (motionType == JPH::EMotionType::Kinematic)
                {
                    ClearPendingForces(*rigidbody);
                    ClearPendingLinearVelocity(*rigidbody);
                    ClearPendingAngularVelocity(*rigidbody);
                    if (_physicsDeltaTime > 0.0f)
                        ApplyPendingKinematicMotion(runtimeData.bodyID, *rigidbody, _physicsDeltaTime);
                }
                else
                {
                    ClearPendingForces(*rigidbody);
                    ClearPendingLinearVelocity(*rigidbody);
                    ClearPendingAngularVelocity(*rigidbody);
                    ClearPendingKinematicMotion(*rigidbody);
                }
            }

            staleBodies.clear();
            for(const auto& [handle,runtime]:bodies)
                if(runtime.seenFrame!=syncFrame)staleBodies.push_back(handle);
            for(auto handle:staleBodies)RemoveBody(handle);
            // Only moving bodies need pose capture/interpolation. Static body
            // edits are still checked in the single synchronization pass above.
            std::erase_if(transforms, [&](const auto& entry) { return entry.second.frame != syncFrame; });
            return movingBodies;
        }

        void ApplyContactFrameData(entt::registry &_registry)
        {
            if (contactListener == nullptr)
                return;

            const ContactFrameData frameData = contactListener->ConsumeFrameData();

            auto applyEvents = [&](const std::vector<FrameContactEvent> &_events, auto _apply)
            {
                for (const FrameContactEvent &event : _events)
                {
                    if (!_registry.valid(event.self))
                        continue;

                    Entity *selfEntity = ResolveEntity(_registry, event.self);
                    Entity *otherEntity = ResolveEntity(_registry, event.other);
                    if (selfEntity == nullptr || otherEntity == nullptr || !selfEntity->Active() || !otherEntity->Active())
                        continue;

                    ForEachCollider(_registry, event.self, [&](auto &_collider)
                    {
                        _apply(_collider, otherEntity);
                    });
                }
            };

            applyEvents(frameData.entered, [](auto &_collider, Entity *_otherEntity)
            {
                _collider.entered.push_back(_otherEntity);
            });

            applyEvents(frameData.exited, [](auto &_collider, Entity *_otherEntity)
            {
                _collider.exited.push_back(_otherEntity);
            });

            applyEvents(frameData.stayed, [](auto &_collider, Entity *_otherEntity)
            {
                _collider.stayed.push_back(_otherEntity);
            });
        }

        void CaptureBodyPoses(entt::registry &_registry, const std::vector<entt::entity> &_activeBodies)
        {
            for (const entt::entity entityHandle : _activeBodies)
            {
                auto bodyIt = bodies.find(entityHandle);
                Rigidbody *rigidbody = _registry.try_get<Rigidbody>(entityHandle);
                if (bodyIt == bodies.end() || rigidbody == nullptr ||
                    ToMotionType(rigidbody->motionType) == JPH::EMotionType::Static)
                {
                    continue;
                }

                BodyRuntimeData &runtimeData = bodyIt->second;
                if (runtimeData.bodyID.IsInvalid() || !bodyInterface->IsAdded(runtimeData.bodyID))
                    continue;

                const Vector3 position = ToCanisPosition(bodyInterface->GetPosition(runtimeData.bodyID));
                const Quaternion rotation = ToCanisRotation(bodyInterface->GetRotation(runtimeData.bodyID));
                if (!runtimeData.hasPhysicsPose)
                {
                    runtimeData.previousWorldPosition = position;
                    runtimeData.previousWorldRotation = rotation;
                    runtimeData.hasPhysicsPose = true;
                }
                else
                {
                    runtimeData.previousWorldPosition = runtimeData.currentWorldPosition;
                    runtimeData.previousWorldRotation = runtimeData.currentWorldRotation;
                }

                runtimeData.currentWorldPosition = position;
                runtimeData.currentWorldRotation = rotation;
            }
        }

        void SyncBodiesAfterStep(
            entt::registry &_registry,
            const std::vector<entt::entity> &_activeBodies,
            float _interpolationAlpha)
        {
            Profiler::Scope timing("Physics pose interpolation", Profiler::Category::Physics);
            for (const entt::entity entityHandle : _activeBodies)
            {
                auto bodyIt = bodies.find(entityHandle);
                Transform *transform = _registry.try_get<Transform>(entityHandle);
                Rigidbody *rigidbody = _registry.try_get<Rigidbody>(entityHandle);
                if (bodyIt == bodies.end() || transform == nullptr || rigidbody == nullptr)
                    continue;

                const JPH::EMotionType motionType = ToMotionType(rigidbody->motionType);
                if (motionType == JPH::EMotionType::Static)
                    continue;

                BodyRuntimeData &runtimeData = bodyIt->second;
                if (runtimeData.bodyID.IsInvalid() || !bodyInterface->IsAdded(runtimeData.bodyID))
                    continue;

                const Vector3 worldPosition = ToCanisPosition(bodyInterface->GetPosition(runtimeData.bodyID));
                Quaternion worldRotation = ToCanisRotation(bodyInterface->GetRotation(runtimeData.bodyID));
                if (!runtimeData.hasPhysicsPose)
                {
                    runtimeData.previousWorldPosition = worldPosition;
                    runtimeData.previousWorldRotation = worldRotation;
                    runtimeData.currentWorldPosition = worldPosition;
                    runtimeData.currentWorldRotation = worldRotation;
                    runtimeData.hasPhysicsPose = true;
                }
                const JPH::Vec3 bodyLinearVelocity = bodyInterface->GetLinearVelocity(runtimeData.bodyID);
                const JPH::Vec3 bodyAngularVelocity = bodyInterface->GetAngularVelocity(runtimeData.bodyID);
                Vector3 angularVelocity = Vector3(bodyAngularVelocity.GetX(), bodyAngularVelocity.GetY(), bodyAngularVelocity.GetZ());

                Vector3 worldEuler = glm::eulerAngles(worldRotation);
                if (ApplyLockedRotationAxes(*transform, *rigidbody, worldEuler, angularVelocity))
                {
                    worldRotation = glm::normalize(Quaternion(worldEuler));
                    bodyInterface->SetPositionAndRotation(
                        runtimeData.bodyID,
                        ToJoltPosition(worldPosition),
                        ToJoltRotation(worldRotation),
                        JPH::EActivation::Activate);
                    bodyInterface->SetAngularVelocity(runtimeData.bodyID, ToJoltVec3(angularVelocity));
                    runtimeData.currentWorldPosition = worldPosition;
                    runtimeData.currentWorldRotation = worldRotation;
                }

                Quaternion interpolatedRotation = runtimeData.currentWorldRotation;
                if (glm::dot(runtimeData.previousWorldRotation, interpolatedRotation) < 0.0f)
                    interpolatedRotation = -interpolatedRotation;

                const float alpha = glm::clamp(_interpolationAlpha, 0.0f, 1.0f);
                const Vector3 interpolatedPosition = glm::mix(
                    runtimeData.previousWorldPosition,
                    runtimeData.currentWorldPosition,
                    alpha);
                interpolatedRotation = glm::normalize(glm::slerp(
                    runtimeData.previousWorldRotation,
                    interpolatedRotation,
                    alpha));

                SetTransformFromWorldPose(*transform, interpolatedPosition, interpolatedRotation);
                rigidbody->linearVelocity = Vector3(bodyLinearVelocity.GetX(), bodyLinearVelocity.GetY(), bodyLinearVelocity.GetZ());
                rigidbody->angularVelocity = angularVelocity;
                runtimeData.syncedLocalPosition = transform->position;
                runtimeData.syncedLocalRotation = transform->rotation;
                runtimeData.hasSyncedTransform = true;
            }
        }

        void Update(entt::registry &_registry, float _deltaTime)
        {
            if (physicsSystem == nullptr || bodyInterface == nullptr)
                return;

            ClearColliderContactVectors(_registry);
            accumulator += glm::max(0.0f, _deltaTime);
            accumulator = glm::min(accumulator, kFixedTimeStep * (float)kMaxSubSteps);
            const int stepCount = glm::min(
                static_cast<int>(accumulator / kFixedTimeStep),
                kMaxSubSteps);
            const float physicsDeltaTime = static_cast<float>(stepCount) * kFixedTimeStep;

            const auto& activeBodies = SyncBodiesBeforeStep(_registry, physicsDeltaTime);
            SyncVehicles(_registry,physicsDeltaTime);

            for (int step = 0; step < stepCount; ++step)
            {
                Profiler::Scope timing("Physics simulation step", Profiler::Category::Physics);
                physicsSystem->Update(kFixedTimeStep, kCollisionSteps, tempAllocator.get(), jobSystem.get());
                accumulator -= kFixedTimeStep;
                CaptureBodyPoses(_registry, activeBodies);
            }

            SyncBodiesAfterStep(
                _registry,
                activeBodies,
                accumulator / kFixedTimeStep);
            SyncVehicleWheels(_registry);
            { Profiler::Scope timing("Physics contact events", Profiler::Category::Physics);
              ApplyContactFrameData(_registry); }
        }

        std::vector<RaycastHit> RaycastAll(entt::registry &_registry, const Vector3 &_origin, const Vector3 &_direction, float _maxDistance, u32 _mask) const
        {
            std::vector<RaycastHit> hits = {};

            if (physicsSystem == nullptr)
                return hits;

            const float directionLength = glm::length(_direction);
            if (directionLength <= 0.000001f)
                return hits;

            float rayLength = _maxDistance;
            if (!std::isfinite(rayLength) || rayLength <= 0.0f)
                rayLength = kDefaultRaycastDistance;

            const Vector3 normalizedDirection = _direction / directionLength;
            const JPH::RRayCast ray(ToJoltPosition(_origin), ToJoltVec3(normalizedDirection * rayLength));

            JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
            physicsSystem->GetNarrowPhaseQuery().CastRay(ray, JPH::RayCastSettings{}, collector);
            if (!collector.HadHit())
                return hits;

            collector.Sort();

            const JPH::BodyLockInterfaceLocking &bodyLockInterface = physicsSystem->GetBodyLockInterface();

            for (const JPH::RayCastResult &hitResult : collector.mHits)
            {
                JPH::BodyLockRead bodyLock(bodyLockInterface, hitResult.mBodyID);
                if (!bodyLock.Succeeded())
                    continue;

                const JPH::Body &body = bodyLock.GetBody();
                const entt::entity entityHandle = ToEntity(body.GetUserData());
                Entity *entity = ResolveEntity(_registry, entityHandle);
                if (entity == nullptr)
                    continue;

                const Rigidbody *rigidbody = _registry.try_get<Rigidbody>(entityHandle);
                const auto* combined=_registry.try_get<CombineMeshCollider>(entityHandle);
                const u32 layer=rigidbody?u32(rigidbody->layer):combined?u32(Rigidbody::DefaultLayer):0u;
                if ((layer & _mask) == 0u)
                    continue;

                const JPH::RVec3 hitPoint = ray.GetPointOnRay(hitResult.mFraction);
                const JPH::Vec3 hitNormal = body.GetWorldSpaceSurfaceNormal(hitResult.mSubShapeID2, hitPoint);
                hits.push_back(RaycastHit{
                    .entity = entity,
                    .point = ToCanisPosition(hitPoint),
                    .normal = Vector3(hitNormal.GetX(), hitNormal.GetY(), hitNormal.GetZ()),
                    .distance = rayLength * hitResult.mFraction,
                    .fraction = hitResult.mFraction
                });
            }

            return hits;
        }

        bool Raycast(entt::registry &_registry, const Vector3 &_origin, const Vector3 &_direction, RaycastHit &_hit, float _maxDistance, u32 _mask) const
        {
            const std::vector<RaycastHit> hits = RaycastAll(_registry, _origin, _direction, _maxDistance, _mask);
            if (hits.empty())
            {
                _hit = RaycastHit{};
                return false;
            }

            _hit = hits.front();
            return true;
        }
    };

    JoltPhysics3DSystem::JoltPhysics3DSystem() : System()
    {
        m_name = type_name<JoltPhysics3DSystem>();
        m_impl = new Impl();
    }

    JoltPhysics3DSystem::~JoltPhysics3DSystem()
    {
        OnDestroy();
    }

    size_t JoltPhysics3DSystem::GetBodyCount() const
    {
        return m_impl ? m_impl->bodies.size() : 0;
    }

    void JoltPhysics3DSystem::Create()
    {
        if (m_impl == nullptr)
            m_impl = new Impl();

        m_impl->Create();
    }

    void JoltPhysics3DSystem::Ready()
    {
    }

    void JoltPhysics3DSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        if (m_impl == nullptr)
            return;

        m_impl->Update(_registry, _deltaTime);
    }

    void JoltPhysics3DSystem::UpdateCombinedCollidersInEditor(entt::registry& registry)
    {
        if (!m_impl || !m_impl->physicsSystem)
            return;
        if (m_impl->combinedMeshes.empty() && registry.view<CombineMeshCollider>().empty())
            return;
        ++m_impl->syncFrame;
        m_impl->SyncCombinedColliders(registry);
    }

    void JoltPhysics3DSystem::OnDestroy()
    {
        if (m_impl == nullptr)
            return;

        m_impl->Destroy();
        delete m_impl;
        m_impl = nullptr;
    }

    bool JoltPhysics3DSystem::Raycast(const Vector3 &_origin, const Vector3 &_direction, RaycastHit &_hit, float _maxDistance, u32 _mask) const
    {
        if (m_impl == nullptr || scene == nullptr)
        {
            _hit = RaycastHit{};
            return false;
        }

        return m_impl->Raycast(scene->GetRegistry(), _origin, _direction, _hit, _maxDistance, _mask);
    }

    bool JoltPhysics3DSystem::Raycast(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance, u32 _mask) const
    {
        RaycastHit hit = {};
        return Raycast(_origin, _direction, hit, _maxDistance, _mask);
    }

    std::vector<RaycastHit> JoltPhysics3DSystem::RaycastAll(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance, u32 _mask) const
    {
        if (m_impl == nullptr || scene == nullptr)
            return {};

        return m_impl->RaycastAll(scene->GetRegistry(), _origin, _direction, _maxDistance, _mask);
    }
} // namespace Canis
