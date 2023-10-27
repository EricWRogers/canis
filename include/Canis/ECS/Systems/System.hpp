#pragma once

#include <Canis/External/entt.hpp>

namespace Canis
{
    class Scene;
    class Entity;
    class Time;
    class Camera;
    class Window;
    class InputManager;

    class System
    {
    private:
    public:
        bool m_isCreated = false;

        System(){};

        virtual void Create() {}
        virtual void Ready() {}
        virtual void Update(entt::registry &_registry, float _deltaTime) {}
        virtual void Draw(entt::registry &_registry, float _deltaTime) {}

        bool IsCreated() { return m_isCreated; }

        Scene &GetScene() { return *scene; }
        Window &GetWindow() { return *window; }
        InputManager &GetInputManager() { return *inputManager; }

        Scene *scene = nullptr;
        Window *window = nullptr;
        InputManager *inputManager = nullptr;
        Time *time = nullptr;
        Camera *camera = nullptr;
    };
} // end of Canis namespace
