#pragma once

#include <Canis/Entity.hpp>
#include <Canis/AssetManager.hpp>

namespace Canis
{

class ScriptableEntity
{    
public:
    virtual ~ScriptableEntity() {}
    Entity m_Entity;
    bool isOnReadyCalled = false;

    virtual void OnCreate() {}
    virtual void OnReady() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(float _dt) {}

    template<typename T>
    T& GetComponent()
    {
        return m_Entity.GetComponent<T>();
    }

    Entity CreateEntity() { return m_Entity.scene->CreateEntity(); }
    Entity CreateEntity(const std::string &_tag) { return m_Entity.scene->CreateEntity(_tag); }
    Window& GetWindow() { return *m_Entity.scene->window; }
    InputManager& GetInputManager() { return *m_Entity.scene->inputManager; }
    Camera& GetCamera() { return *m_Entity.scene->camera; }
    Scene& GetScene() { return *m_Entity.scene; }

    template<typename T>
    T* GetSystem() { return m_Entity.scene->GetSystem<T>(); }
};
} // end of Canis namespace