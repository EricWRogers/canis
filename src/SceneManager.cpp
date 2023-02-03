#include <Canis/SceneManager.hpp>
#include <Canis/Entity.hpp>

#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/Camera2DComponent.hpp>
#include <Canis/ECS/Components/CircleColliderComponent.hpp>

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
        AssetManager *_assetManager,
        unsigned int _seed
    )
    {
        window = _window;
        inputManager = _inputManager;
        time = _time;
        camera = _camera;
        assetManager = _assetManager;
        seed = _seed;

        for(int i = 0; i < scenes.size(); i++)
        {
            scenes[i]->window = _window;
            scenes[i]->inputManager = _inputManager;
            scenes[i]->sceneManager = (unsigned int *)this;
            scenes[i]->time = _time;
            scenes[i]->camera = _camera;
            scenes[i]->assetManager = _assetManager;
            scenes[i]->seed = _seed;

            if (scenes[i]->path != "") {
                YAML::Node root = YAML::LoadFile(scenes[i]->path);

                scenes[i]->name = root["Scene"].as<std::string>();

                // serialize systems
                if(YAML::Node systems = root["Systems"]) {
                    for(int s = 0;  s < systems.size(); s++) {
                        for(int d = 0;  d < decodeSystem.size(); d++) {
                            if (decodeSystem[d](systems, s, scenes[i]))
                                continue;
                        }
                    }
                }

                // serialize render systems
                if(YAML::Node renderSystems = root["RenderSystems"]) {
                    for(int r = 0;  r < renderSystems.size(); r++) {
                        for(int d = 0;  d < decodeRenderSystem.size(); d++) {
                            if (decodeRenderSystem[d](renderSystems, r, scenes[i]))
                                continue;
                        }
                    }
                }
            }
            
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


            /*scene->entityRegistry.each([&](auto entityID) {
                Canis::Log("HI");
                scene->entityRegistry.destroy(entityID);
            });*/

            scene->entityRegistry = entt::registry();
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


        auto view = scene->entityRegistry.view<Canis::ScriptComponent>();

        for(auto [_entity, _scriptComponent] : view.each())
        {
            if (!_scriptComponent.Instance) {
                _scriptComponent.Instance = _scriptComponent.InstantiateScript();
                _scriptComponent.Instance->m_Entity = Entity { _entity,  this->scene };
                _scriptComponent.Instance->OnCreate();
            }
        }

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