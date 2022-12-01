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
                // Clean up ScriptableEntity pointer
                scene->entityRegistry.view<ScriptComponent>().each([](auto entity, auto& scriptComponent)
                {
                    if (scriptComponent.Instance)
                    {
                        scriptComponent.Instance->OnDestroy();
                    }
                });

                // Clean up TextComponent pointer
                scene->entityRegistry.view<TextComponent>().each([](auto entity, auto& textComponent)
                {
                    if (textComponent.text != nullptr)
                    {
                        delete textComponent.text;
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

    void SceneManager::ForceLoad(std::string _name)
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

    void SceneManager::Load(std::string _name)
    {
        for(int i = 0; i < scenes.size(); i++)
        {
            if(scenes[i]->name == _name)
            {
                patientLoadIndex = i;
                return;
            }
        }
    }

    void SceneManager::Update()
    {
        m_updateStart = high_resolution_clock::now();
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
            patientLoadIndex = -1;
        }
        
        scene->Update();

        // update scripts
        {
            auto view = scene->entityRegistry.view<Canis::ScriptComponent>();

            for(auto [_entity, _scriptComponent] : view.each())
            {
                if (!_scriptComponent.Instance) {
                    _scriptComponent.Instance = _scriptComponent.InstantiateScript();
                    _scriptComponent.Instance->m_Entity = Entity { _entity,  this->scene };
                    _scriptComponent.Instance->OnCreate();
                }

                _scriptComponent.Instance->OnUpdate(this->scene->deltaTime);
            }
        }

        m_updateEnd = high_resolution_clock::now();
        updateTime = std::chrono::duration_cast<std::chrono::nanoseconds>(m_updateEnd - m_updateStart).count() / 1000000000.0f;
    }

    void SceneManager::LateUpdate()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
            patientLoadIndex = -1;
        }

        scene->LateUpdate();
    }

    void SceneManager::Draw()
    {
        m_drawStart = high_resolution_clock::now();

        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
            patientLoadIndex = -1;
        }
        
        scene->Draw();

        m_drawEnd = high_resolution_clock::now();
        drawTime = std::chrono::duration_cast<std::chrono::nanoseconds>(m_drawEnd - m_drawStart).count() / 1000000000.0f;
    }

    void SceneManager::InputUpdate()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
            patientLoadIndex = -1;
        }
        
        scene->InputUpdate();
    }

    void SceneManager::SetDeltaTime(double _deltaTime)
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        scene->deltaTime = _deltaTime;
    }
} // end of Canis namespace