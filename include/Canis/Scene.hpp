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
            friend class SceneManager;

            std::vector<System*> m_updateSystems = {};
            std::vector<System*> m_renderSystems = {};
            
            high_resolution_clock::time_point m_drawStart;
            high_resolution_clock::time_point m_drawEnd;
            float m_drawTime;
        public:
            //Scene() {}
            Scene(std::string _name);
            Scene(std::string _name, std::string _path);
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

            void SetTimeScale(double _timeScale) { timeScale = _timeScale; }

            Entity CreateEntity();
            Entity CreateEntity(const std::string &_tag);
            Entity FindEntityWithTag(const std::string &_tag);

            std::vector<Canis::Entity> Instantiate(const std::string &_path);
            std::vector<Canis::Entity> Instantiate(const std::string &_path, glm::vec3 _position);

            template<typename T>
            T* GetSystem() {
                T* castedSystem = nullptr;

                for(System* s : systems)
                    if ((castedSystem = dynamic_cast<T*>(s)) != nullptr)
                        return castedSystem;

                return nullptr;
            }

            template<typename T>
            void CreateSystem() {
                System* s = new T();
                
                m_updateSystems.push_back(s);
                ReadySystem(s);
            }

            template<typename T>
            void CreateRenderSystem() {
                System* s = new T();

                m_renderSystems.push_back(s);
                ReadySystem(s);
            }

            std::string name = "";
            std::string path = "";

            Window *window;
            InputManager *inputManager;
            unsigned int *sceneManager;
            Time *time;
            Camera *camera;
            unsigned int seed;

            std::vector<System*> systems = {};

            entt::registry entityRegistry;

            double deltaTime = 1.0;
            double unscaledDeltaTime = 1.0;
            double timeScale = 1.0;
    };
} // end of Canis namespace
