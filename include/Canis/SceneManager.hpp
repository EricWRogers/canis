#pragma once
#include <vector>

#include <Canis/Time.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/AssetManager.hpp>

#include <Canis/ECS/Components/ScriptComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>

namespace Canis
{
    class SceneManager {
        private:
            int index = -1;
            int patientLoadIndex = -1;
            Scene *scene = nullptr;
            std::vector<Scene *> scenes;

        public:
            SceneManager();
            ~SceneManager();

            int Add(Scene *s);
            void PreLoad(Window *_window,
                            InputManager *_inputManager,
                            Time *_time,
                            Camera *_camera,
                            AssetManager *_assetManager);

            void Load(int _index);
            void Load(std::string _name);
            void PatientLoad(std::string _name); // use this function when looping threw entities or in a update loop

            void Update();
            void LateUpdate();
            void Draw();
            void InputUpdate();

            void UnLoad();

            void SetDeltaTime(double _deltaTime);

            Window *window;
            InputManager *inputManager;
            Time *time;
            AssetManager *assetManager;
            Camera *camera;
    };
} // end of Canis namespace