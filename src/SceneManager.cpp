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
        Camera *_camera,
        AssetManager *_assetManager
    )
    {
        window = _window;
        inputManager = _inputManager;
        time = _time;
        camera = _camera;
        assetManager = _assetManager;

        for(int i = 0; i < scenes.size(); i++)
        {
            scenes[i]->window = _window;
            scenes[i]->inputManager = _inputManager;
            scenes[i]->sceneManager = (unsigned int *)this;
            scenes[i]->time = _time;
            scenes[i]->camera = _camera;
            scenes[i]->assetManager = _assetManager;
            
            scenes[i]->PreLoad();
        }
    }

    void SceneManager::Load(int _index)
    {
        if (scenes.size() < _index)
            FatalError("Failed to load scene at index " + std::to_string(_index));
        
        if (scene != nullptr)
        {
            scene->UnLoad();
            {
                scene->entityRegistry.view<ScriptComponent>().each([](auto entity, auto& scriptComponent)
                {
                    if (scriptComponent.Instance)
                    {
                        scriptComponent.Instance->OnDestroy();
                        delete scriptComponent.Instance;
                    }
                });
            }
            scene->entityRegistry.clear();
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

        // update scripts
        {
            scene->entityRegistry.view<Canis::ScriptComponent>().each([=, this](auto entity, auto& scriptComponent)
            {
                if (!scriptComponent.Instance) {
                    scriptComponent.Instance = scriptComponent.InstantiateScript();
                    scriptComponent.Instance->m_Entity = Entity { entity,  this->scene };
                    scriptComponent.Instance->OnCreate();
                }

                scriptComponent.Instance->OnUpdate(this->scene->deltaTime);
            });
        }
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