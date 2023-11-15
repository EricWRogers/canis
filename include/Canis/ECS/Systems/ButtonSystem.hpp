#pragma once
#include <Canis/ECS/Systems/System.hpp>
#include <Canis/External/entt.hpp>

namespace Canis
{
    class Scene;

    struct ButtonAndDepth {
        entt::entity entity;
        float depth;
    };

    class ButtonSystem : public System
    {
    private:
        ButtonAndDepth* m_buttons = nullptr;
    public:
        ButtonSystem() : System() {}
        ~ButtonSystem();

        void Create();

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeButtonSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace