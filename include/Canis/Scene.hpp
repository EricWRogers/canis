#pragma once
#include <string>

#include <Canis/Time.hpp>
#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/ECS/Systems/System.hpp>
#include <Canis/ECS/Components/TagComponent.hpp>

namespace Canis
{
    class Entity;

    class Scene {
        private:
            friend class Entity;
        
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

            Entity CreateEntity();
            Entity CreateEntity(const std::string &_tag);
            System* GetSystem(std::string _name);

            template<typename T>
            T* GetSystem() {
                T* castedSystem = nullptr;

                for(System* s : systems)
                    if ((castedSystem = dynamic_cast<T*>(s)) != nullptr)
                        return castedSystem;

                return nullptr;
            }

            std::string name;

            Window *window;
            InputManager *inputManager;
            unsigned int *sceneManager;
            Time *time;
            AssetManager *assetManager;
            Camera *camera;
            unsigned int seed;

            std::vector<System*> systems = {};

            entt::registry entityRegistry;

            double deltaTime;

            bool preLoaded = false;
    };
} // end of Canis namespace