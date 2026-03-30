#pragma once
#include <Canis/System.hpp>

namespace Canis
{
    class SpriteAnimationSystem : public System
    {
    public:
        SpriteAnimationSystem() : System() { m_name = type_name<SpriteAnimationSystem>(); }

        void Create() override {}

        void Ready() override;

        void Update(entt::registry &_registry, float _deltaTime) override;
    };
} // end of Canis namespace
