#pragma once
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Scene;

    class DamageText3DSystem : public System
    {
    private:
    public:
        DamageText3DSystem() : System() {}

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeDamageText3DSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace