#include <Canis/ECS/Systems/JoltPhysics3DSystem.hpp>

#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Math.hpp>

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
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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
            return JPH::EAllowedDOFs::All;
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

        size_t BuildSettingsHash(
            entt::registry &_registry,
            entt::entity _entityHandle,
            const Transform &_transform,
            const Rigidbody &_rigidbody,
            const BoxCollider *_boxCollider,
            const SphereCollider *_sphereCollider,
            const CapsuleCollider *_capsuleCollider,
            const MeshCollider *_meshCollider,
            const ConvexMeshCollider *_convexMeshCollider)
        {
            size_t hash = 0;
            hash = HashCombine(hash, std::hash<int>{}(_rigidbody.motionType));
            hash = HashCombine(hash, std::hash<float>{}(_rigidbody.mass));
            hash = HashCombine(hash, std::hash<float>{}(_rigidbody.friction));
            hash = HashCombine(hash, std::hash<float>{}(_rigidbody.restitution));
            hash = HashCombine(hash, std::hash<float>{}(_rigidbody.linearDamping));
            hash = HashCombine(hash, std::hash<float>{}(_rigidbody.angularDamping));
            hash = HashCombine(hash, std::hash<bool>{}(_rigidbody.useGravity));
            hash = HashCombine(hash, std::hash<float>{}(_rigidbody.gravityFactor));
            hash = HashCombine(hash, std::hash<bool>{}(_rigidbody.isSensor));
            hash = HashCombine(hash, std::hash<u32>{}(_rigidbody.layer));
            hash = HashCombine(hash, std::hash<u32>{}(_rigidbody.mask));
            hash = HashCombine(hash, std::hash<bool>{}(_rigidbody.allowSleeping));
            hash = HashCombine(hash, HashVector(glm::abs(_transform.GetGlobalScale())));
            hash = HashCombine(hash, HashVector(_transform.GetGlobalScale()));

            if (_boxCollider != nullptr)
            {
                hash = HashCombine(hash, 101u);
                hash = HashCombine(hash, std::hash<bool>{}(_boxCollider->active));
                hash = HashCombine(hash, HashVector(_boxCollider->offset));
                hash = HashCombine(hash, HashVector(_boxCollider->size));
            }
            else if (_sphereCollider != nullptr)
            {
                hash = HashCombine(hash, 102u);
                hash = HashCombine(hash, std::hash<bool>{}(_sphereCollider->active));
                hash = HashCombine(hash, HashVector(_sphereCollider->offset));
                hash = HashCombine(hash, std::hash<float>{}(_sphereCollider->radius));
            }
            else if (_capsuleCollider != nullptr)
            {
                hash = HashCombine(hash, 103u);
                hash = HashCombine(hash, std::hash<bool>{}(_capsuleCollider->active));
                hash = HashCombine(hash, HashVector(_capsuleCollider->offset));
                hash = HashCombine(hash, std::hash<float>{}(_capsuleCollider->halfHeight));
                hash = HashCombine(hash, std::hash<float>{}(_capsuleCollider->radius));
            }
            else if (_meshCollider != nullptr)
            {
                hash = HashCombine(hash, 104u);
                hash = HashCombine(hash, std::hash<bool>{}(_meshCollider->active));
                hash = HashCombine(hash, std::hash<bool>{}(_meshCollider->useAttachedModel));
                hash = HashCombine(hash, std::hash<std::string>{}(_meshCollider->modelPath));

                const i32 modelId = ResolveMeshColliderModelId(_registry, _entityHandle, _meshCollider);
                const i32 nodeIndex = ResolveMeshColliderNodeIndex(_registry, _entityHandle, _meshCollider);
                const bool applyNodeTransform = ResolveMeshColliderApplyNodeTransform(_registry, _entityHandle, _meshCollider);
                hash = HashCombine(hash, std::hash<int>{}(modelId));
                hash = HashCombine(hash, std::hash<int>{}(nodeIndex));
                hash = HashCombine(hash, std::hash<bool>{}(applyNodeTransform));
                if (const ModelAsset *model = AssetManager::GetModel(modelId))
                    hash = HashCombine(hash, std::hash<u64>{}(model->GetGeometryRevision()));
            }
            else if (_convexMeshCollider != nullptr)
            {
                hash = HashCombine(hash, 105u);
                hash = HashCombine(hash, std::hash<bool>{}(_convexMeshCollider->active));
                hash = HashCombine(hash, std::hash<bool>{}(_convexMeshCollider->useAttachedModel));
                hash = HashCombine(hash, std::hash<std::string>{}(_convexMeshCollider->modelPath));
                hash = HashCombine(hash, HashVector(_convexMeshCollider->offset));
                hash = HashCombine(hash, HashVector(_convexMeshCollider->scale));
                hash = HashCombine(hash, std::hash<float>{}(_convexMeshCollider->convexRadius));

                const i32 modelId = ResolveConvexMeshColliderModelId(_registry, _entityHandle, _convexMeshCollider);
                const i32 nodeIndex = ResolveConvexMeshColliderNodeIndex(_registry, _entityHandle, _convexMeshCollider);
                const bool applyNodeTransform = ResolveConvexMeshColliderApplyNodeTransform(_registry, _entityHandle, _convexMeshCollider);
                hash = HashCombine(hash, std::hash<int>{}(modelId));
                hash = HashCombine(hash, std::hash<int>{}(nodeIndex));
                hash = HashCombine(hash, std::hash<bool>{}(applyNodeTransform));
                if (const ModelAsset *model = AssetManager::GetModel(modelId))
                    hash = HashCombine(hash, std::hash<u64>{}(model->GetGeometryRevision()));
            }
            else
            {
                hash = HashCombine(hash, 100u);
            }

            return hash;
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

        entt::entity ToEntityHandle(uint64_t _userData)
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
                    .body1 = ToEntityHandle(_body1.GetUserData()),
                    .body2 = ToEntityHandle(_body2.GetUserData())
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
            size_t settingsHash = 0;
            Vector3 syncedLocalPosition = Vector3(0.0f);
            Quaternion syncedLocalRotation = Quaternion(Vector3(0.0f));
            bool hasSyncedTransform = false;
            Vector3 previousWorldPosition = Vector3(0.0f);
            Quaternion previousWorldRotation = Quaternion(Vector3(0.0f));
            Vector3 currentWorldPosition = Vector3(0.0f);
            Quaternion currentWorldRotation = Quaternion(Vector3(0.0f));
            bool hasPhysicsPose = false;
        };

        std::unique_ptr<BPLayerInterfaceImpl> broadPhaseLayerInterface = nullptr;
        std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> objectVsBroadPhaseLayerFilter = nullptr;
        std::unique_ptr<ObjectLayerPairFilterImpl> objectLayerPairFilter = nullptr;
        std::unique_ptr<ContactListenerImpl> contactListener = nullptr;
        std::unique_ptr<JPH::PhysicsSystem> physicsSystem = nullptr;
        std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator = nullptr;
        std::unique_ptr<JPH::JobSystemThreadPool> jobSystem = nullptr;
        JPH::BodyInterface *bodyInterface = nullptr;
        std::unordered_map<entt::entity, BodyRuntimeData> bodies = {};
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

            for (auto &entry : bodies)
                RemoveBodyFromWorld(entry.second.bodyID);
            bodies.clear();

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

        bool EnsureBodyForEntity(entt::registry &_registry, entt::entity _entityHandle)
        {
            Transform *transform = _registry.try_get<Transform>(_entityHandle);
            Rigidbody *rigidbody = _registry.try_get<Rigidbody>(_entityHandle);
            if (transform == nullptr || rigidbody == nullptr)
            {
                RemoveBody(_entityHandle);
                return false;
            }

            Entity *entity = rigidbody->entity != nullptr ? rigidbody->entity : transform->entity;
            if (entity == nullptr || !entity->active || !rigidbody->active)
            {
                RemoveBody(_entityHandle);
                return false;
            }

            BoxCollider *boxCollider = _registry.try_get<BoxCollider>(_entityHandle);
            SphereCollider *sphereCollider = _registry.try_get<SphereCollider>(_entityHandle);
            CapsuleCollider *capsuleCollider = _registry.try_get<CapsuleCollider>(_entityHandle);
            MeshCollider *meshCollider = _registry.try_get<MeshCollider>(_entityHandle);
            ConvexMeshCollider *convexMeshCollider = _registry.try_get<ConvexMeshCollider>(_entityHandle);

            if (boxCollider != nullptr && !boxCollider->active)
                boxCollider = nullptr;
            if (sphereCollider != nullptr && !sphereCollider->active)
                sphereCollider = nullptr;
            if (capsuleCollider != nullptr && !capsuleCollider->active)
                capsuleCollider = nullptr;
            if (meshCollider != nullptr && !meshCollider->active)
                meshCollider = nullptr;
            if (convexMeshCollider != nullptr && !convexMeshCollider->active)
                convexMeshCollider = nullptr;

            if (boxCollider == nullptr && sphereCollider == nullptr && capsuleCollider == nullptr && meshCollider == nullptr && convexMeshCollider == nullptr)
            {
                RemoveBody(_entityHandle);
                return false;
            }

            const size_t settingsHash = BuildSettingsHash(
                _registry,
                _entityHandle,
                *transform,
                *rigidbody,
                boxCollider,
                sphereCollider,
                capsuleCollider,
                meshCollider,
                convexMeshCollider);
            auto bodyIt = bodies.find(_entityHandle);
            const bool needsRecreate = (bodyIt == bodies.end()) || (bodyIt->second.settingsHash != settingsHash);

            if (!needsRecreate)
                return true;

            if (bodyIt != bodies.end())
                RemoveBodyFromWorld(bodyIt->second.bodyID);

            JPH::RefConst<JPH::Shape> shape = BuildShape(
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
                return false;
            }

            const JPH::EMotionType motionType = ToMotionType(rigidbody->motionType);
            JPH::BodyCreationSettings bodySettings(
                shape.GetPtr(),
                ToJoltPosition(transform->GetGlobalPosition()),
                ToJoltRotation(transform->GetGlobalRotation()),
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
                Debug::Log("JoltPhysics3DSystem: failed to create body for entity '%s'.", entity->name.c_str());
                RemoveBody(_entityHandle);
                return false;
            }

            BodyRuntimeData runtimeData;
            runtimeData.bodyID = bodyID;
            runtimeData.settingsHash = settingsHash;
            runtimeData.syncedLocalPosition = transform->position;
            runtimeData.syncedLocalRotation = transform->rotation;
            runtimeData.hasSyncedTransform = true;
            runtimeData.previousWorldPosition = transform->GetGlobalPosition();
            runtimeData.previousWorldRotation = transform->GetGlobalRotation();
            runtimeData.currentWorldPosition = runtimeData.previousWorldPosition;
            runtimeData.currentWorldRotation = runtimeData.previousWorldRotation;
            runtimeData.hasPhysicsPose = true;
            bodies[_entityHandle] = runtimeData;
            return true;
        }

        std::vector<entt::entity> SyncBodiesBeforeStep(entt::registry &_registry, float _physicsDeltaTime)
        {
            std::vector<entt::entity> activeBodies = {};
            std::unordered_set<entt::entity> expected = {};

            auto rigidbodyView = _registry.view<Rigidbody, Transform>();
            for (const entt::entity entityHandle : rigidbodyView)
            {
                if (!EnsureBodyForEntity(_registry, entityHandle))
                    continue;

                expected.insert(entityHandle);
                activeBodies.push_back(entityHandle);
            }

            std::vector<entt::entity> staleEntities = {};
            staleEntities.reserve(bodies.size());
            for (const auto &entry : bodies)
            {
                const entt::entity entityHandle = entry.first;
                if (!_registry.valid(entityHandle) || expected.find(entityHandle) == expected.end())
                    staleEntities.push_back(entityHandle);
            }

            for (const entt::entity entityHandle : staleEntities)
                RemoveBody(entityHandle);

            for (const entt::entity entityHandle : activeBodies)
            {
                auto bodyIt = bodies.find(entityHandle);
                Transform *transform = _registry.try_get<Transform>(entityHandle);
                Rigidbody *rigidbody = _registry.try_get<Rigidbody>(entityHandle);
                if (bodyIt == bodies.end() || transform == nullptr || rigidbody == nullptr)
                    continue;

                BodyRuntimeData &runtimeData = bodyIt->second;
                const JPH::EMotionType motionType = ToMotionType(rigidbody->motionType);

                const bool transformEdited = !runtimeData.hasSyncedTransform
                    || !NearlyEqual(transform->position, runtimeData.syncedLocalPosition)
                    || !NearlyEqual(transform->rotation, runtimeData.syncedLocalRotation);

                // Kinematic bodies are advanced by MoveKinematic. Reapplying the
                // interpolated Transform every pre-step resets that motion before
                // the next command can accumulate. Only push a kinematic pose
                // when game/editor code explicitly changed its Transform.
                if (motionType == JPH::EMotionType::Static || transformEdited)
                {
                    const JPH::EActivation activation = (motionType == JPH::EMotionType::Static)
                        ? JPH::EActivation::DontActivate
                        : JPH::EActivation::Activate;
                    bodyInterface->SetPositionAndRotation(
                        runtimeData.bodyID,
                        ToJoltPosition(transform->GetGlobalPosition()),
                        ToJoltRotation(transform->GetGlobalRotation()),
                        activation);

                    runtimeData.syncedLocalPosition = transform->position;
                    runtimeData.syncedLocalRotation = transform->rotation;
                    runtimeData.hasSyncedTransform = true;
                    runtimeData.previousWorldPosition = transform->GetGlobalPosition();
                    runtimeData.previousWorldRotation = transform->GetGlobalRotation();
                    runtimeData.currentWorldPosition = runtimeData.previousWorldPosition;
                    runtimeData.currentWorldRotation = runtimeData.previousWorldRotation;
                    runtimeData.hasPhysicsPose = true;
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

            return activeBodies;
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
                    if (selfEntity == nullptr || otherEntity == nullptr || !selfEntity->active || !otherEntity->active)
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

            std::vector<entt::entity> activeBodies = SyncBodiesBeforeStep(_registry, physicsDeltaTime);

            for (int step = 0; step < stepCount; ++step)
            {
                physicsSystem->Update(kFixedTimeStep, kCollisionSteps, tempAllocator.get(), jobSystem.get());
                accumulator -= kFixedTimeStep;
                CaptureBodyPoses(_registry, activeBodies);
            }

            SyncBodiesAfterStep(
                _registry,
                activeBodies,
                accumulator / kFixedTimeStep);
            ApplyContactFrameData(_registry);
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
                const entt::entity entityHandle = ToEntityHandle(body.GetUserData());
                Entity *entity = ResolveEntity(_registry, entityHandle);
                if (entity == nullptr)
                    continue;

                const Rigidbody *rigidbody = _registry.try_get<Rigidbody>(entityHandle);
                if (rigidbody == nullptr || (rigidbody->layer & _mask) == 0u)
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
