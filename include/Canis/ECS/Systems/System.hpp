#pragma once
#include <Canis/Time.hpp>
#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/AssetManager.hpp>


namespace Canis
{
class System
{
private:
public:
    Window *window = nullptr;
    InputManager *inputManager = nullptr;
    Time *time = nullptr;
    AssetManager *assetManager = nullptr;
    Camera *camera = nullptr;
};
} // end of Canis namespace