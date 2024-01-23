#pragma once
#include <Canis/ECS/Systems/System.hpp>
#include <Canis/External/entt.hpp>
#include <functional>

namespace Canis
{
    class Scene;

    struct ButtonAndDepth {
        entt::entity entity;
        float depth;
    };

    struct ButtonListener {
        std::string name;
        void* data;
        std::function<void(void* _data)> func;
    };

    class ButtonSystem : public System
    {
    private:
        ButtonAndDepth* m_buttons = nullptr;
        ButtonListener* m_buttonListeners = nullptr;
    public:
        ButtonSystem() : System() {}
        ~ButtonSystem();

        void Create();

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);

        ButtonListener* AddButtonListener(
            std::string _name,
            void* _data,
            std::function<void(void* _data)> func
        );

        void RemoveButtonListener(ButtonListener* _listener);
    };

    extern bool DecodeButtonSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace