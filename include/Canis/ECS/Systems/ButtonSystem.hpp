#pragma once
#include <Canis/ECS/Systems/System.hpp>
#include <Canis/External/entt.hpp>
#include <Canis/Entity.hpp>
#include <functional>

namespace Canis
{
    class Scene;

    struct ButtonAndDepth {
        Entity entity;
        float depth;
    };

    struct ButtonListener {
        std::string name = "";
        void* data = nullptr;
        std::function<void(Entity _entity, void* _data)> func = nullptr;
    };

    class ButtonSystem : public System
    {
    private:
        InputDevice m_lastInputDevice = InputDevice::MOUSE;
        ButtonAndDepth* m_buttons = nullptr;
        ButtonListener* m_buttonListeners = nullptr;
    public:
        ButtonSystem() : System() { m_name = type_name<ButtonSystem>(); }
        ~ButtonSystem();

        void Create();

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);

        ButtonListener* AddButtonListener(
            std::string _name,
            void* _data,
            std::function<void(Entity _entity, void* _data)> func
        );

        void RemoveButtonListener(ButtonListener* _listener);
    };
} // end of Canis namespace