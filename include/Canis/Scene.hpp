#pragma once
#include <string>

#include <Canis/Time.hpp>
#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Scene {
        public:
            //Scene() {}
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

            void ReadySystem(System *_system);

            std::string name;

            Window *window;
            InputManager *inputManager;
            unsigned int *sceneManager;
            Time *time;
            AssetManager *assetManager;
            Camera *camera;

            entt::registry entityRegistry;

            double deltaTime;

            bool preLoaded = false;
    };
} // end of Canis namespace