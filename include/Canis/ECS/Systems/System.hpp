#pragma once
#include <Canis/Time.hpp>
#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/External/entt.hpp>

namespace Canis
{
class Scene;

class System
{
private:
    
public:
    bool m_isCreated = false;
    
    System() {};

    virtual void Create() {}
    virtual void Ready() {}
    virtual void Update(entt::registry &_registry, float _deltaTime) {}
    virtual void Draw(entt::registry &_registry, float _deltaTime) {}

    bool IsCreated() { return m_isCreated; }

    Scene *scene = nullptr;
    Window *window = nullptr;
    InputManager *inputManager = nullptr;
    Time *time = nullptr;
    AssetManager *assetManager = nullptr;
    Camera *camera = nullptr;

    //virtual void Init() {}
    //virtual void UpdateComponent() {}
};
} // end of Canis namespace