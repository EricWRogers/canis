#pragma once
#include <string>

#include <Canis/External/GetNameOfType.hpp>
#include <Canis/External/entt.hpp>

namespace Canis
{
    class Scene;
    class Entity;
    //class Time;
    //class Camera;
    class Window;
    class InputManager;

    class System
    {
    protected:
        std::string m_name = "ChangeMe";
    public:
        bool m_isCreated = false;

        System(){};

        virtual void Create() {}
        virtual void Ready() {}
        virtual void Update(entt::registry &_registry, float _deltaTime) {}
        virtual void OnDestroy() {}
        virtual bool UpdateWhenPaused() const { return false; }

        bool IsCreated() { return m_isCreated; }
        std::string GetName() { return m_name; }

        Scene &GetScene() { return *scene; }
        Window &GetWindow() { return *window; }
        //InputManager &GetInputManager() { return *inputManager; }

        Scene *scene = nullptr;
        Window *window = nullptr;
        InputManager *inputManager = nullptr;
        //Time *time = nullptr;
        //Camera *camera = nullptr;
    };
} // end of Canis namespace
