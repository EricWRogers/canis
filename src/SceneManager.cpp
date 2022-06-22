#include <Canis/SceneManager.hpp>

namespace Canis
{
    SceneManager::SceneManager(){}

    SceneManager::~SceneManager()
    {
        for(int i = 0; i < scenes.size(); i++)
        {
            delete scenes[i];
        }

        scenes.clear();
    }

    int SceneManager::Add(Scene *s)
    {
        scenes.push_back(s);

        return scenes.size() - 1;
    }

    void SceneManager::PreLoad(
        Window *_window,
        InputManager *_inputManager,
        Time *_time,
        Camera *_camera
    )
    {
        Canis::Log("Q SceneManager PreLoad Start");

        window = _window;
        inputManager = _inputManager;
        time = _time;
        camera = _camera;

        for(int i = 0; i < scenes.size(); i++)
        {
            Canis::Log("Q SceneManager PreLoad " + std::to_string(i));
            scenes[i]->window = _window;
            scenes[i]->inputManager = _inputManager;
            scenes[i]->time = _time;
            scenes[i]->camera = _camera;
            
            scenes[i]->PreLoad();
        }
        Canis::Log("Q SceneManager PreLoad End");
    }

    void SceneManager::Load(int _index)
    {
        if (scenes.size() < _index)
            FatalError("Failed to load scene at index " + std::to_string(_index));
        
        if (scene != nullptr)
        {
            scene->UnLoad();
        }

        scene = scenes[_index];

        if (!scene->IsPreLoaded())
        {
            scene->window = window;
            scene->inputManager = inputManager;
            scene->time = time;
            scene->camera = camera;

            scene->PreLoad();
        }

        scene->Load();
    }

    void SceneManager::Load(std::string _name)
    {
        for(int i = 0; i < scenes.size(); i++)
        {
            if(scenes[i]->name == _name)
            {
                Load(i);
                return;
            }
        }

        FatalError("Scene not found with name " + _name);
    }

    void SceneManager::Update()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        scene->Update();
    }

    void SceneManager::LateUpdate()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");

        scene->LateUpdate();
    }

    void SceneManager::Draw()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        scene->Draw();
    }

    void SceneManager::InputUpdate()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        scene->InputUpdate();
    }

    void SceneManager::SetDeltaTime(double _deltaTime)
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        scene->deltaTime = _deltaTime;
    }
} // end of Canis namespace