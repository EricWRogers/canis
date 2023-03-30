#pragma once
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class SpriteAnimationSystem : public System
    {
    private:
    public:
        SpriteAnimationSystem() : System() {}

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeSpriteAnimationSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace