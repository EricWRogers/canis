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

                    auto camera2dComponent = e["Canis::Camera2DComponent"];
                    if (camera2dComponent)
                    {
                        auto& c2dc = entity.AddComponent<Canis::Camera2DComponent>();
                        c2dc.position = camera2dComponent["position"].as<glm::vec2>();
                        c2dc.scale = camera2dComponent["scale"].as<float>();
                    }

                    auto rectTransform = e["Canis::RectTransformComponent"];
                    if (rectTransform)
                    {
                        auto& rt = entity.AddComponent<Canis::RectTransformComponent>();
                        rt.active = rectTransform["active"].as<bool>();
                        rt.anchor = (Canis::RectAnchor)rectTransform["anchor"].as<int>();
                        rt.position = rectTransform["position"].as<glm::vec2>();
                        rt.size = rectTransform["size"].as<glm::vec2>();
                        rt.originOffset = rectTransform["originOffset"].as<glm::vec2>();
                        rt.rotation = rectTransform["rotation"].as<float>();
                        rt.scale = rectTransform["scale"].as<float>();
                        rt.depth = rectTransform["depth"].as<float>();
                    }

                    auto colorComponent = e["Canis::ColorComponent"];
                    if (colorComponent)
                    {
                        auto& cc = entity.AddComponent<Canis::ColorComponent>();
                        cc.color = colorComponent["color"].as<glm::vec4>();
                    }

                    auto textComponent = e["Canis::TextComponent"];
                    if (textComponent)
                    {
                        auto& tc = entity.AddComponent<Canis::TextComponent>();
                        auto asset = textComponent["assetId"];
                        if (asset)
                        {
                            tc.assetId = assetManager->LoadText(
                                asset["path"].as<std::string>(),
                                asset["size"].as<unsigned int>()
                            );
                        }
                        tc.text = new std::string;
                        (*tc.text) = textComponent["text"].as<std::string>();
                        tc.align = textComponent["align"].as<unsigned int>();
                    }

                    auto sprite2DComponent = e["Canis::Sprite2DComponent"];
                    if (sprite2DComponent)
                    {
                        auto& s2dc = entity.AddComponent<Canis::Sprite2DComponent>();
                        s2dc.uv = sprite2DComponent["uv"].as<glm::vec4>();
                        s2dc.texture = assetManager->Get<Canis::TextureAsset>(
                            assetManager->LoadTexture(
                                sprite2DComponent["texture"].as<std::string>()
                            )
                        )->GetTexture();
                    }

                    auto scriptComponent = e["Canis::ScriptComponent"];
                    if (scriptComponent)
                    {
                        for(int d = 0;  d < decodeScriptableEntity.size(); d++)
                            if (decodeScriptableEntity[d](scriptComponent.as<std::string>(),entity))
                                continue;
                    }

                    auto circleColliderComponent = e["Canis::CircleColliderComponent"];
                    if (circleColliderComponent)
                    {
                        auto& ccc = entity.AddComponent<Canis::CircleColliderComponent>();
                        ccc.center = circleColliderComponent["center"].as<glm::vec2>();
                        ccc.radius = circleColliderComponent["radius"].as<float>();
                        ccc.layer = (Canis::BIT)circleColliderComponent["layer"].as<unsigned int>();
                        ccc.mask = (Canis::BIT)circleColliderComponent["mask"].as<unsigned int>();
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