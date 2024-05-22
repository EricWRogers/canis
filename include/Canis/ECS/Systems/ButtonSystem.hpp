#pragma once
#include <Canis/ECS/Systems/System.hpp>
#include <Canis/External/entt.hpp>
#include <Canis/Entity.hpp>
#include <functional>

namespace Canis
{
    class Scene;
    class ButtonSystem;

    struct ButtonAndDepth {
        Entity entity;
        float depth;
    };

    struct ButtonListener {
        std::string name = "";
        void* data = nullptr;
        std::function<void(Entity _entity, void* _data)> func = nullptr;

        int _id = 0;
        ButtonSystem* _system = nullptr;

        ~ButtonListener();
    };

    class ButtonSystem : public System
    {
    private:
        InputDevice m_lastInputDevice = InputDevice::MOUSE;
        ButtonAndDepth* m_buttons = nullptr;
        std::vector<ButtonListener*> m_buttonListeners = {};
        int nextId = 0;
    public:
        ButtonSystem() : System() { m_name = type_name<ButtonSystem>(); }
        ~ButtonSystem();

        void Create();

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);

        ButtonListener& AddButtonListener(
            std::string _name,
            void* _data,
            std::function<void(Entity _entity, void* _data)> _func
        );

        void RemoveButtonListener(int _id);
    };
} // end of Canis namespace