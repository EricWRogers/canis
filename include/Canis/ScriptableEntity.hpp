#pragma once

#include <Canis/Entity.hpp>
#include <Canis/AssetManager.hpp>

namespace Canis
{
class SceneManager;

class ScriptableEntity
{    
public:
    virtual ~ScriptableEntity() {}
    Entity entity;
    bool isOnReadyCalled = false;

    virtual void OnCreate() {}
    virtual void OnReady() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(float _dt) {}

    template<typename T>
    T& GetComponent()
    {
        return entity.GetComponent<T>();
    }

    Entity CreateEntity() { return entity.scene->CreateEntity(); }
    Entity CreateEntity(const std::string &_tag) { return entity.scene->CreateEntity(_tag); }
    Window& GetWindow() { return *entity.scene->window; }
    InputManager& GetInputManager() { return *entity.scene->inputManager; }
    Camera& GetCamera() { return *entity.scene->camera; }
    Scene& GetScene() { return *entity.scene; }
    SceneManager& GetSceneManager() { return *(SceneManager*)(entity.scene->sceneManager); }

    template<typename T>
    T* GetSystem() { return entity.scene->GetSystem<T>(); }
};
} // end of Canis namespace