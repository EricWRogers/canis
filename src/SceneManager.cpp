#include <Canis/SceneManager.hpp>
#include <Canis/Entity.hpp>

#include <Canis/ECS/Components/TextComponent.hpp>

#include <SDL_keyboard.h>

namespace Canis
{
    SceneManager::SceneManager(){}

    SceneManager::~SceneManager()
    {
        for(int i = 0; i < m_scenes.size(); i++)
        {
            delete m_scenes[i].scene;
        }

        m_scenes.clear();
    }

    int SceneManager::Add(Scene *s)
    {
        SceneData sceneData;
        sceneData.scene = s;
        m_scenes.push_back(sceneData);

        return m_scenes.size() - 1;
    }

    int SceneManager::AddSplashScene(Scene *s)
    {
        SceneData sceneData;
        sceneData.scene = s;
        sceneData.splashScreen = true;
        m_scenes.push_back(sceneData);

        return m_scenes.size() - 1;
    }

    void SceneManager::PreLoadAll()
    {
        for(int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].preloaded == false)
            {
                m_scenes[i].scene->window = window;
                m_scenes[i].scene->inputManager = inputManager;
                m_scenes[i].scene->sceneManager = (unsigned int *)this;
                m_scenes[i].scene->time = time;
                m_scenes[i].scene->camera = camera;
                m_scenes[i].scene->seed = seed;

                if (m_scenes[i].scene->path != "") {
                    YAML::Node root = YAML::LoadFile(m_scenes[i].scene->path);

                    m_scenes[i].scene->name = root["Scene"].as<std::string>();

                    // serialize systems
                    if(YAML::Node systems = root["Systems"]) {
                        for(int s = 0;  s < systems.size(); s++) {
                            std::string name = systems[s].as<std::string>();
                            for(int d = 0;  d < decodeSystem.size(); d++) {
                                if (decodeSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }

                    // serialize render systems
                    if(YAML::Node renderSystems = root["RenderSystems"]) {
                        for(int r = 0;  r < renderSystems.size(); r++) {
                            std::string name = renderSystems[r].as<std::string>();
                            for(int d = 0;  d < decodeRenderSystem.size(); d++) {
                                if (decodeRenderSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }
                }
                
                m_scenes[i].scene->PreLoad();
                m_scenes[i].preloaded = true;
            }
        }
    }

    void SceneManager::PreLoad(std::string _sceneName)
    {
        for(int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].preloaded == false && m_scenes[i].scene->name == _sceneName)
            {
                m_scenes[i].scene->window = window;
                m_scenes[i].scene->inputManager = inputManager;
                m_scenes[i].scene->sceneManager = (unsigned int *)this;
                m_scenes[i].scene->time = time;
                m_scenes[i].scene->camera = camera;
                m_scenes[i].scene->seed = seed;

                if (m_scenes[i].scene->path != "") {
                    YAML::Node root = YAML::LoadFile(m_scenes[i].scene->path);

                    m_scenes[i].scene->name = root["Scene"].as<std::string>();

                    // serialize systems
                    if(YAML::Node systems = root["Systems"]) {
                        for(int s = 0;  s < systems.size(); s++) {
                            std::string name = systems[s].as<std::string>();
                            for(int d = 0;  d < decodeSystem.size(); d++) {
                                if (decodeSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }

                    // serialize render systems
                    if(YAML::Node renderSystems = root["RenderSystems"]) {
                        for(int r = 0;  r < renderSystems.size(); r++) {
                            std::string name = renderSystems[r].as<std::string>();
                            for(int d = 0;  d < decodeRenderSystem.size(); d++) {
                                if (decodeRenderSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }
                }
                
                m_scenes[i].scene->PreLoad();
                m_scenes[i].preloaded = true;
            }
        }
    }

    void SceneManager::Load(int _index)
    {
        if (m_scenes.size() < _index)
            FatalError("Failed to load scene at index " + std::to_string(_index));

        patientLoadIndex = -1;
        
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

            // swap maps
            message.swap(nextMessage);
            nextMessage.clear();
            
            scene->entityRegistry = entt::registry();
        }

        scene = m_scenes[_index].scene;
        m_sceneIndex = _index;

        if (!scene->IsPreLoaded())
        {
            scene->window = window;
            scene->inputManager = inputManager;
            scene->time = time;
            scene->camera = camera;

            scene->PreLoad();
        }

        scene->Load();

        // in case someone calls load in load
        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
        }

        if(scene->path != "")
        {
            YAML::Node root = YAML::LoadFile(scene->path);

            auto entities = root["Entities"];

            if(entities)
            {
                for(auto e : entities)
                {
                    Canis::Entity entity = scene->CreateEntity();

                    for(int d = 0;  d < decodeEntity.size(); d++) {
                        decodeEntity[d](e, entity, this);
                    }

                    if (auto scriptComponent = e["Canis::ScriptComponent"])
                    {
                        for (int d = 0; d < decodeScriptableEntity.size(); d++)
                            if (decodeScriptableEntity[d](scriptComponent.as<std::string>(), entity))
                                continue;
                    }
                }
            }
        }

        for(int i = 0; i < scene->systems.size(); i++)
        {
            if(!scene->systems[i]->IsCreated())
            {
                scene->systems[i]->Create();
                scene->systems[i]->m_isCreated = true;
            }
        }

        for(int i = 0; i < scene->systems.size(); i++)
        {
            scene->systems[i]->Ready();
        }
    }

    void SceneManager::ForceLoad(std::string _name)
    {
        for(int i = 0; i < m_scenes.size(); i++)
        {
            if(m_scenes[i].scene->name == _name)
            {
                Load(i);
                return;
            }
        }

        FatalError("Scene not found with name " + _name);
    }

    void SceneManager::Load(std::string _name)
    {
        for(int i = 0; i < m_scenes.size(); i++)
        {
            if(m_scenes[i].scene->name == _name)
            {
                patientLoadIndex = i;
                return;
            }
        }

        Warning("SceneManager: " + _name + " NOT FOUND");
    }

    bool SceneManager::IsSplashScene(std::string _sceneName)
    {
        for(int i = 0; i < m_scenes.size(); i++)
        {
            if(m_scenes[i].scene->name == _sceneName && m_scenes[i].splashScreen == true)
            {
                return true;
            }
        }

        return false;
    }

    void SceneManager::HotReload()
    {
        if(scene == nullptr)
        Load(scene->name);
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
        
        auto view = scene->entityRegistry.view<Canis::ScriptComponent>();

        for(auto [_entity, _scriptComponent] : view.each())
        {
            if (!_scriptComponent.Instance) {
                _scriptComponent.Instance = _scriptComponent.InstantiateScript();
                _scriptComponent.Instance->entity = Entity { _entity,  this->scene };
                _scriptComponent.Instance->OnCreate();
            }
        }
        
        scene->Update();

        for(auto [_entity, _scriptComponent] : view.each())
        {
            if (!_scriptComponent.Instance->isOnReadyCalled) {
                _scriptComponent.Instance->isOnReadyCalled = true;
                _scriptComponent.Instance->OnReady();
            }

            _scriptComponent.Instance->OnUpdate(this->scene->deltaTime);
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

        if (m_scenes[m_sceneIndex].splashScreenHasCalledLoadAll == false && m_scenes[m_sceneIndex].splashScreen)
        {
            PreLoadAll();
            m_scenes[m_sceneIndex].splashScreenHasCalledLoadAll = true;
        }
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

        glFinish(); // make sure all queued command are finished

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

        if (inputManager->JustPressedKey(SDLK_F12))
        {
            window->ToggleFullScreen();
        }
    }

    void SceneManager::SetDeltaTime(double _deltaTime)
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        scene->deltaTime       = _deltaTime * scene->timeScale;
        scene->unscaledDeltaTime = _deltaTime;
    }
} // end of Canis namespace
