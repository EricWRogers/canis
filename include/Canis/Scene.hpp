#pragma once
#include <string>

#include <Canis/Time.hpp>
#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>

namespace Canis
{
    class Scene {
        public:
            Scene(std::string _name);
            ~Scene();

            virtual void PreLoad();
            virtual void Load();
            virtual void UnLoad();
            virtual void Update();
            virtual void LateUpdate();
            virtual void Draw();
            virtual void InputUpdate();

            bool IsPreLoaded();

            std::string name;

            Window *window;
            InputManager *inputManager;
            unsigned int *sceneManager;
            Time *time;

            Camera *camera;

            double deltaTime;

            bool preLoaded = false;
    };
} // end of Canis namespace