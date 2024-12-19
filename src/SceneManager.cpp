#include <Canis/SceneManager.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/Canis.hpp>

#include <Canis/ECS/Components/Color.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>
#include <Canis/ECS/Components/UISliderKnobComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>

#include <Canis/External/OpenGl.hpp>

#include <SDL.h>
#include <SDL_keyboard.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <glm/gtc/type_ptr.hpp>

namespace Canis
{
    void SceneManager::FindEntityEditor(Entity &_entity, UUID &_uuid)
    {
        for (auto euuid : entityAndUUIDToConnect)
        {
            if (_uuid == euuid.uuid)
            {
                _entity.entityHandle = euuid.entity->entityHandle;
                return;
            }
        }

        _uuid = 0lu;
    }

    void SceneManager::SetUpIMGUI(Window *_window)
    {
        m_editor.Init(_window);
    }

    SceneManager::SceneManager() {}

    SceneManager::~SceneManager()
    {
        if (scene != nullptr)
        {
            scene->UnLoad();
            {
                // Clean up ScriptableEntity pointer
                scene->entityRegistry.view<ScriptComponent>().each([](auto entity, auto &scriptComponent)
                                                                   {
                    if (scriptComponent.Instance)
                    {
                        scriptComponent.Instance->OnDestroy();
                        delete scriptComponent.Instance;
                    } });
            }

            // swap maps
            message.swap(nextMessage);
            nextMessage.clear();

            scene->entityRegistry = entt::registry();
        }

        for (int i = 0; i < m_scenes.size(); i++)
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
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].preloaded == false)
            {
                m_scenes[i].scene->window = window;
                m_scenes[i].scene->inputManager = inputManager;
                m_scenes[i].scene->sceneManager = (unsigned int *)this;
                m_scenes[i].scene->time = time;
                m_scenes[i].scene->camera = camera;
                m_scenes[i].scene->seed = seed;

                if (m_scenes[i].scene->path != "")
                {
                    YAML::Node root = YAML::LoadFile(m_scenes[i].scene->path);

                    m_scenes[i].scene->name = root["Scene"].as<std::string>();

                    // serialize systems
                    if (YAML::Node systems = root["Systems"])
                    {
                        for (int s = 0; s < systems.size(); s++)
                        {
                            std::string name = systems[s].as<std::string>();
                            for (int d = 0; d < decodeSystem.size(); d++)
                            {
                                if (decodeSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }

                    // serialize render systems
                    if (YAML::Node renderSystems = root["RenderSystems"])
                    {
                        for (int r = 0; r < renderSystems.size(); r++)
                        {
                            std::string name = renderSystems[r].as<std::string>();
                            for (int d = 0; d < decodeRenderSystem.size(); d++)
                            {
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
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].preloaded == false && m_scenes[i].scene->name == _sceneName)
            {
                m_scenes[i].scene->window = window;
                m_scenes[i].scene->inputManager = inputManager;
                m_scenes[i].scene->sceneManager = (unsigned int *)this;
                m_scenes[i].scene->time = time;
                m_scenes[i].scene->camera = camera;
                m_scenes[i].scene->seed = seed;

                if (m_scenes[i].scene->path != "")
                {
                    YAML::Node root = YAML::LoadFile(m_scenes[i].scene->path);

                    m_scenes[i].scene->name = root["Scene"].as<std::string>();

                    // serialize systems
                    if (YAML::Node systems = root["Systems"])
                    {
                        for (int s = 0; s < systems.size(); s++)
                        {
                            std::string name = systems[s].as<std::string>();
                            for (int d = 0; d < decodeSystem.size(); d++)
                            {
                                if (decodeSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }

                    // serialize render systems
                    if (YAML::Node renderSystems = root["RenderSystems"])
                    {
                        for (int r = 0; r < renderSystems.size(); r++)
                        {
                            std::string name = renderSystems[r].as<std::string>();
                            for (int d = 0; d < decodeRenderSystem.size(); d++)
                            {
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
        static bool notInit = true;

        if (notInit)
        {
            notInit = false;

            SetUpIMGUI(window);
        }

        if (m_scenes.size() < _index)
            FatalError("Failed to load scene at index " + std::to_string(_index));

        patientLoadIndex = -1;

        if (scene != nullptr)
        {

            scene->UnLoad();
            {
                // Clean up ScriptableEntity pointer
                scene->entityRegistry.view<ScriptComponent>().each([this](auto entity, auto &scriptComponent) {
                    Entity(entity, scene).RemoveScript();
                });
            }

            // swap maps
            message.swap(nextMessage);
            nextMessage.clear();

            scene->entityRegistry = entt::registry();
        }

        scene = m_scenes[_index].scene;
        m_sceneIndex = _index;

        // normally only runs in editor
        // when a splash screen is launched first
        if (m_scenes[_index].preloaded == false)
        {
            PreLoad(m_scenes[_index].scene->name);
        }

        entityAndUUIDToConnect.clear();
        hierarchyElements.clear();

        scene->Load();

        // in case someone calls load in load
        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
        }

        if (scene->path != "")
        {
            YAML::Node root = YAML::LoadFile(scene->path);

            window->SetClearColor(
                root["ClearColor"].as<glm::vec4>(glm::vec4(0.05f, 0.05f, 0.05f, 1.0f)));

            window->ClearColor();

            auto entities = root["Entities"];

            if (entities)
            {
                for (auto e : entities)
                {
                    Canis::Entity entity = scene->CreateEntity();

                    entity.AddComponent<IDComponent>(UUID(e["Entity"].as<uint64_t>(0)));

                    HierarchyElementInfo hei;
                    hei.name = e["Name"].as<std::string>("");
                    hei.entity.entityHandle = entity.entityHandle;
                    hei.entity.scene = scene;

                    hierarchyElements.push_back(hei);
 
                    for (int d = 0; d < decodeEntity.size(); d++)
                        decodeEntity[d](e, entity, this);

                    if (auto scriptComponent = e["Canis::ScriptComponent"])
                        for (int d = 0; d < decodeScriptableEntity.size(); d++)
                            if (decodeScriptableEntity[d](scriptComponent.as<std::string>(), entity))
                                continue;

                    if (auto prefab = e["Prefab"])
                    {
                        scene->Instantiate(prefab.as<std::string>());
                    }
                }

                for (auto euuid : entityAndUUIDToConnect)
                {
                    euuid.entity->scene = scene;

                    auto view = scene->entityRegistry.view<IDComponent>();

                    for (auto [entity, id] : view.each())
                    {
                        if (id.ID == euuid.uuid)
                        {
                            euuid.entity->entityHandle = entity;
                            break;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < scene->systems.size(); i++)
        {
            if (!scene->systems[i]->IsCreated())
            {
                scene->systems[i]->Create();
                scene->systems[i]->m_isCreated = true;
            }
        }

        for (int i = 0; i < scene->systems.size(); i++)
        {
            scene->systems[i]->Ready();
        }
    }

    void SceneManager::ForceLoad(std::string _name)
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].scene->name == _name)
            {
                Load(i);
                return;
            }
        }

        FatalError("Scene not found with name " + _name);
    }

    void SceneManager::Load(std::string _name)
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].scene->name == _name)
            {
                patientLoadIndex = i;
                return;
            }
        }

        Warning("SceneManager: " + _name + " NOT FOUND");
    }

    void SceneManager::Save()
    {
        Log("Save Scene");
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << scene->name;

        out << YAML::Key << "ClearColor" << YAML::Value << window->GetScreenColor();

        // Serialize System
        out << YAML::Key << "Systems";
        out << YAML::BeginSeq;
        for (System *s : scene->m_updateSystems)
        {
            out << YAML::Value << s->GetName();
        }
        out << YAML::EndSeq;

        // Serialize Render System
        out << YAML::Key << "RenderSystems";
        out << YAML::BeginSeq;
        for (System *s : scene->m_renderSystems)
        {
            out << YAML::Value << s->GetName();
        }
        out << YAML::EndSeq;

        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

        for (HierarchyElementInfo info : hierarchyElements)
        {
            Entity entity = info.entity;
            
			if (!entity)
				return;

            if (!entity.HasComponent<IDComponent>())
                return;
            
            out << YAML::BeginMap;

            out << YAML::Key << "Entity" << YAML::Key << std::to_string(entity.GetUUID());

            out << YAML::Key << "Name" << YAML::Key << info.name;

            for (int i = 0; i < encodeEntity.size(); i++)
                encodeEntity[i](out, entity);
            
            if (entity.HasComponent<ScriptComponent>())
                for (int i = 0; i < encodeScriptableEntity.size(); i++)
                    if (encodeScriptableEntity[i](out, entity.GetComponent<ScriptComponent>().Instance))
                        break;
            
            out << YAML::EndMap;
        }

        out << YAML::EndSeq;
        out << YAML::EndMap;

        if (scene->path.size() > 0)
        {
            std::ofstream fout(scene->path);
            fout << out.c_str();
        }
        else
        {
            std::ofstream fout(scene->name);
            fout << out.c_str();
        }
    }

    bool SceneManager::IsSplashScene(std::string _sceneName)
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].scene->name == _sceneName && m_scenes[i].splashScreen == true)
            {
                return true;
            }
        }

        return false;
    }

    void SceneManager::HotReload()
    {
        if (scene == nullptr)
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

        for (auto [_entity, _scriptComponent] : view.each())
        {
            if (!_scriptComponent.Instance)
            {
                _scriptComponent.Instance = _scriptComponent.InstantiateScript();
                _scriptComponent.Instance->entity = Entity{_entity, this->scene};

                if (m_editor.GetMode() == EditorMode::PLAY || GetProjectConfig().editor == false)
                {
                    _scriptComponent.Instance->OnCreate();
                }
            }
        }

        if (m_editor.GetMode() == EditorMode::PLAY || GetProjectConfig().editor == false)
        {

            scene->Update();

            for (auto [_entity, _scriptComponent] : view.each())
            {
                if (!_scriptComponent.Instance->isOnReadyCalled)
                {
                    _scriptComponent.Instance->isOnReadyCalled = true;
                    _scriptComponent.Instance->OnReady();
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

        if (m_editor.GetMode() != EditorMode::PLAY && GetProjectConfig().editor == true)
            return;

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

        m_editor.Draw(scene, window, time);

        glFinish(); // make sure all queued command are finished

        m_drawEnd = high_resolution_clock::now();
        drawTime = std::chrono::duration_cast<std::chrono::nanoseconds>(m_drawEnd - m_drawStart).count() / 1000000000.0f;
    }

    void SceneManager::InputUpdate()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");
        
        if (m_editor.GetMode() != EditorMode::PLAY && GetProjectConfig().editor == true)
            return;

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

        scene->unscaledDeltaTime = _deltaTime;
        scene->deltaTime = _deltaTime * scene->timeScale;
    }
} // end of Canis namespace
