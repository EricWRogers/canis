#pragma once
#include <vector>

#include <Canis/Time.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>

namespace Canis
{
    class SceneManager {
        private:
            int index = -1;
            Scene *scene = nullptr;
            std::vector<Scene *> scenes;

        public:
            SceneManager();
            ~SceneManager();

            int Add(Scene *s);
            void PreLoad(Window *_window,
                    InputManager *_inputManager, Time *_time, Camera *_camera);

            void Load(int _index);
            void Load(std::string _name);

            void Update();
            void LateUpdate();
            void Draw();
            void InputUpdate();

            void UnLoad();

            void SetDeltaTime(double _deltaTime);

            Window *window;
            InputManager *inputManager;
            Time *time;

            Camera *camera;
    };
} // end of Canis namespace