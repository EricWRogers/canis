#pragma once

#include <Canis/System.hpp>

namespace Canis
{
    class AnimatorSystem : public System
    {
    public:
        AnimatorSystem() : System() { m_name = type_name<AnimatorSystem>(); }

        void Create() override {}
        void Ready() override {}
        void Update(entt::registry &_registry, float _deltaTime) override;
    };
}
