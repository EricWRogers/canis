#pragma once

#include <Canis/Scene.hpp>
#include <Canis/System.hpp>

namespace Canis
{
    class JoltPhysics3DSystem : public System
    {
    public:
        JoltPhysics3DSystem();
        ~JoltPhysics3DSystem();

        void Create() override;
        void Ready() override;
        void Update(entt::registry &_registry, float _deltaTime) override;
        void OnDestroy() override;
        bool Raycast(const Vector3 &_origin, const Vector3 &_direction, RaycastHit &_hit, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max()) const;
        bool Raycast(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max()) const;
        std::vector<RaycastHit> RaycastAll(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max()) const;

    private:
        struct Impl;
        Impl *m_impl = nullptr;
    };
} // namespace Canis
