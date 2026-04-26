#pragma once

#include <Canis/System.hpp>

namespace Canis
{
    class AnimationPlayerSystem : public System
    {
    public:
        AnimationPlayerSystem() : System() { m_name = type_name<AnimationPlayerSystem>(); }

        void Create() override {}
        void Ready() override {}
        void Update(entt::registry &_registry, float _deltaTime) override;
    };
}
