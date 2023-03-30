#pragma once
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Scene;

    class ButtonSystem : public System
    {
    private:
    public:
        ButtonSystem() : System() {}

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeButtonSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace