#pragma once

#include <Canis/Entity.hpp>

namespace Canis
{
class ScriptableEntity
{    
public:
    virtual ~ScriptableEntity() {}
    Entity m_Entity;

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
    AssetManager& GetAssetManager() { return *m_Entity.scene->assetManager; }
    Window& GetWindow() { return *m_Entity.scene->window; }
    InputManager& GetInputManager() { return *m_Entity.scene->inputManager; }
};
} // end of Canis namespace