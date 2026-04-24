#include <Canis/Scene.hpp>
#include <Canis/App.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Entity.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/System.hpp>
#include <Canis/Time.hpp>
#include <Canis/Window.hpp>
#include <Canis/ECS/Systems/SpriteAnimationSystem.hpp>
#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>
#include <Canis/ECS/Systems/UIInteractionSystem.hpp>
#include <Canis/ECS/Systems/ModelAnimation3DSystem.hpp>
#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <Canis/ECS/Systems/JoltPhysics3DSystem.hpp>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <SDL3/SDL_timer.h>

namespace Canis
{
    namespace
    {
        UUID ResolveEnvironmentAssetUUID(const YAML::Node &node)
        {
            if (!node)
                return UUID(0);

            if (node.IsMap())
            {
                if (YAML::Node uuidNode = node["uuid"])
                {
                    const UUID uuid = uuidNode.as<uint64_t>(0);
                    if ((uint64_t)uuid != 0)
                        return uuid;
                }

                if (YAML::Node pathNode = node["path"])
                {
                    const std::string path = pathNode.as<std::string>("");
                    if (!path.empty())
                    {
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                            return meta->uuid;
                    }
                }

                return UUID(0);
            }

            if (node.IsScalar())
            {
                const std::string rawValue = node.as<std::string>("");
                if (rawValue.empty())
                    return UUID(0);

                const bool isNumeric = std::all_of(rawValue.begin(), rawValue.end(), [](unsigned char c)
                    { return std::isdigit(c) != 0; });
                if (isNumeric)
                    return (UUID)std::stoull(rawValue);

                if (MetaFileAsset *meta = AssetManager::GetMetaFile(rawValue))
                    return meta->uuid;
            }

            return UUID(0);
        }

        bool HasHierarchyParent(Entity *_entity)
        {
            if (_entity == nullptr)
                return false;

            if (_entity->HasComponent<RectTransform>() && _entity->GetComponent<RectTransform>().parent != nullptr)
                return true;

            if (_entity->HasComponent<Transform>() && _entity->GetComponent<Transform>().parent != nullptr)
                return true;

            return false;
        }
    }

    void Scene::Init(App *_app, Window *_window, InputManager *_inputManger)
    {
        app = _app;
        m_window = _window;
        m_inputManager = _inputManger;
        m_paused = false;
        ClearLastRenderCamera();
        ClearEditorCameraOverrides();

        // TODO resizing breaks components
        // I think this is fixed but needs more testing
        m_entities.reserve(1000);
    }

    void Scene::SetEditorCamera3DOverride(const Matrix4 &_view, const Matrix4 &_projection)
    {
        m_editorCamera3DOverrideEnabled = true;
        m_editorCamera3DView = _view;
        m_editorCamera3DProjection = _projection;
    }

    void Scene::SetEditorCamera2DOverride(const Matrix4 &_cameraMatrix, const Vector2 &_position)
    {
        m_editorCamera2DOverrideEnabled = true;
        m_editorCamera2DMatrix = _cameraMatrix;
        m_editorCamera2DPosition = _position;
    }

    void Scene::ClearEditorCameraOverrides()
    {
        m_editorCamera3DOverrideEnabled = false;
        m_editorCamera2DOverrideEnabled = false;
    }

    void Scene::SetLastRenderCamera(const Matrix4 &_view, const Matrix4 &_projection, const Vector3 &_cameraPosition, float _nearClip, float _farClip)
    {
        m_lastRenderCameraValid = true;
        m_lastRenderView = _view;
        m_lastRenderProjection = _projection;
        m_lastRenderCameraPosition = _cameraPosition;
        m_lastRenderCameraNearClip = _nearClip;
        m_lastRenderCameraFarClip = _farClip;
    }

    void Scene::ClearLastRenderCamera()
    {
        m_lastRenderCameraValid = false;
        m_lastRenderView = Matrix4(1.0f);
        m_lastRenderProjection = Matrix4(1.0f);
        m_lastRenderCameraPosition = Vector3(0.0f);
        m_lastRenderCameraNearClip = 0.1f;
        m_lastRenderCameraFarClip = 100.0f;
    }

    bool Scene::Raycast(const Vector3 &_origin, const Vector3 &_direction, RaycastHit &_hit, float _maxDistance, u32 _mask)
    {
        if (JoltPhysics3DSystem *physicsSystem = GetSystem<JoltPhysics3DSystem>())
            return physicsSystem->Raycast(_origin, _direction, _hit, _maxDistance, _mask);

        _hit = RaycastHit{};
        return false;
    }

    bool Scene::Raycast(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance, u32 _mask)
    {
        RaycastHit hit = {};
        return Raycast(_origin, _direction, hit, _maxDistance, _mask);
    }

    std::vector<RaycastHit> Scene::RaycastAll(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance, u32 _mask)
    {
        if (JoltPhysics3DSystem *physicsSystem = GetSystem<JoltPhysics3DSystem>())
            return physicsSystem->RaycastAll(_origin, _direction, _maxDistance, _mask);

        return {};
    }

    void Scene::Update(float _deltaTime)
    {
        m_isUpdating = true;

        for (System* system : m_updateSystems)
        {
            if (m_paused && !system->UpdateWhenPaused())
                continue;

            const Uint64 start = SDL_GetTicksNS();
            system->Update(m_registry, _deltaTime);
            const float elapsedMs = static_cast<float>(SDL_GetTicksNS() - start) / 1000000.0f;
            if (SystemTiming* timing = GetSystemTiming(system))
                timing->updateMs = elapsedMs;
        }
        // Run Ready only on entities created since the last frame.
        for (size_t i = 0; i < m_entitiesToReady.size(); ++i)
        {
            const int id = m_entitiesToReady[i];
            if (id < 0 || id >= (int)m_entities.size())
                continue;

            Entity* e = m_entities[id];
            if (e == nullptr || !e->active)
                continue;

            auto& scripts = e->m_scriptComponents;
            for (size_t j = 0; j < scripts.size() && e->active; ++j)
            {
                ScriptableEntity* se = scripts[j];
                if (!se || se->m_onReadyCalled)
                    continue;

                se->Ready();
                se->m_onReadyCalled = true;
            }
        }
        m_entitiesToReady.clear();

        for (size_t i = 0; i < m_entities.size(); ++i)
        {
            Entity* e = m_entities[i];
            if (e == nullptr || !e->active)
                continue;

            auto& scripts = e->m_scriptComponents;
            for (size_t j = 0; j < scripts.size() && e->active; ++j)
            {
                ScriptableEntity* se = scripts[j];
                if (se && se->m_onReadyCalled)
                {
                    if (m_paused && !se->UpdateWhenPaused())
                        continue;

                    se->Update(_deltaTime);
                }
            }
        }

        m_isUpdating = false;
        for (int id : m_entitiesToDestroy)
        {
            DestroyNow(id);
        }
        m_entitiesToDestroy.clear();
    }

    void Scene::Render(float _deltaTime)
    {
        //Canis::Debug::Log("Render Update %i", m_renderSystems.size());
        for (System* renderer : m_renderSystems)
        {
            const Uint64 start = SDL_GetTicksNS();
            renderer->Update(m_registry, _deltaTime);
            const float elapsedMs = static_cast<float>(SDL_GetTicksNS() - start) / 1000000.0f;
            if (SystemTiming* timing = GetSystemTiming(renderer))
                timing->renderMs = elapsedMs;
        }
    }

    void Scene::Unload()
    {
        for (Entity* e : m_entities)
        {
            if (e == nullptr)
                continue;
            e->RemoveAllScripts();
            delete e;
        }

        m_entities.clear();
        m_registry.clear();
        // Fully release EnTT storage pools now, while any game-code component
        // types are still loaded. Otherwise pool destruction can be deferred
        // until Scene itself dies after the game shared library unloads.
        entt::registry emptyRegistry = {};
        m_registry.swap(emptyRegistry);
        m_paused = false;
        m_entitiesToReady.clear();
        m_entitiesToDestroy.clear();
        m_targetUUIDNewUUID.clear();
        m_entityConnectInfo.clear();
        m_isUpdating = false;
        m_isLoadingEntityNodes = false;
        m_environmentSkyboxUUID = UUID(0);
        m_environmentPostProcessUUID = UUID(0);
        ClearLastRenderCamera();
        ClearEditorCameraOverrides();

        for (System* system : m_systems)
        {
            system->OnDestroy();
        }

        for (System* system : m_systems)
        {
            delete system;
        }

        m_systems.clear();
        m_updateSystems.clear();
        m_renderSystems.clear();
        m_systemTimings.clear();
    }

    void Scene::QuitGame()
    {
        SetPaused(false);
        Time::SetTimeScale(1.0f);

#if CANIS_EDITOR
        if (app != nullptr)
        {
            Editor& editor = app->GetEditor();
            const EditorMode mode = editor.GetMode();
            if (mode == EditorMode::PLAY || mode == EditorMode::PAUSE)
            {
                editor.RequestStopPlayMode();
                return;
            }
        }
#endif

        GetWindow().RequestClose();
    }

    void Scene::Load(std::string _path)
    {
        YAML::Node root;
        try
        {
            m_path = _path;
            root = YAML::LoadFile(m_path);
        }
        catch (const YAML::BadFile&)
        {
            Debug::FatalError("Scene not found: %s", m_path.c_str());
            return;
        }
        catch (const YAML::Exception &_exception)
        {
            Debug::FatalError("Failed to load scene '%s': %s", m_path.c_str(), _exception.what());
            return;
        }

        if (!root)
        {
            Debug::FatalError("Scene not found: %s", m_path.c_str());
        }
        
        LoadSceneNode(root);
    }

    void Scene::LoadSceneNode(YAML::Node &_root)
    {
        CreateRenderSystem<Canis::MeshRenderer3DSystem>();
        CreateRenderSystem<Canis::SpriteRenderer2DSystem>();
        CreateSystem<Canis::UIInteractionSystem>();
        CreateSystem<Canis::ModelAnimation3DSystem>();
        CreateSystem<Canis::SpriteAnimationSystem>();
        CreateSystem<Canis::JoltPhysics3DSystem>();

        if (app != nullptr)
        {
            for (const SystemConf& conf : app->GetSystemRegistry())
            {
                if (!conf.autoCreate || conf.Construct == nullptr)
                    continue;

                if (conf.pipeline == SystemPipeline::Render)
                    CreateRenderSystem(conf.Construct());
                else
                    CreateSystem(conf.Construct());
            }
        }

        m_environmentAmbientLight = Color(0.24f, 0.26f, 0.32f, 1.0f);
        m_environmentAmbientLightIntensity = 1.0f;
        m_environmentSkyboxUUID = UUID(0);
        m_environmentPostProcessUUID = UUID(0);
        m_paused = false;
        
        for (System* system : m_systems)
        {
            system->Create();
        }

        for (System* system : m_systems)
        {
            system->Ready();
        }

        auto environment = _root["Environment"]; 

        if (environment)
        {
            m_window->SetClearColor(
                environment["ClearColor"].as<Vector4>(Vector4(0.05f, 0.05f, 0.05f, 1.0f))
            );
            m_environmentAmbientLight = environment["AmbientLight"].as<Vector4>(Vector4(0.24f, 0.26f, 0.32f, 1.0f));
            m_environmentAmbientLightIntensity = std::max(environment["AmbientLightIntensity"].as<float>(1.0f), 0.0f);

            if (YAML::Node skyboxNode = environment["SkyboxAsset"])
            {
                m_environmentSkyboxUUID = ResolveEnvironmentAssetUUID(skyboxNode);
            }

            if (YAML::Node postProcessNode = environment["PostProcessAsset"])
            {
                m_environmentPostProcessUUID = ResolveEnvironmentAssetUUID(postProcessNode);
            }
        }

        auto entities = _root["Entities"];

        LoadEntityNodes(entities);
    }

    std::vector<Entity*> Scene::LoadEntityNodes(YAML::Node &_entities, bool _copyUUID)
    {
        m_targetUUIDNewUUID.clear();
        m_entityConnectInfo.clear();
        m_isLoadingEntityNodes = true;
        std::vector<Canis::Entity*> newEntitys = {};

        if (_entities)
        {
            for (auto e : _entities)
            {
                newEntitys.push_back(&DecodeEntity(e, _copyUUID));
            }

            for (auto eci : m_entityConnectInfo)
            {
                Canis::UUID uuid = eci.targetUUID;

                if (m_targetUUIDNewUUID.contains(uuid))
                    uuid = m_targetUUIDNewUUID[uuid];
                
                (*eci.variable) = GetEntityWithUUID(uuid);
            }

            // Keep bidirectional hierarchy links in sync after pointer remapping.
            for (Canis::Entity* entity : newEntitys)
            {
                if (entity == nullptr)
                    continue;

                if (entity->HasComponent<RectTransform>())
                {
                    RectTransform& transform = entity->GetComponent<RectTransform>();
                    if (transform.parent != nullptr)
                    {
                        if (transform.parent->HasComponent<RectTransform>())
                        {
                            RectTransform& parentTransform = transform.parent->GetComponent<RectTransform>();
                            auto& siblings = parentTransform.children;
                            if (std::find(siblings.begin(), siblings.end(), entity) == siblings.end())
                                siblings.push_back(entity);
                        }
                        else
                        {
                            transform.parent = nullptr;
                        }
                    }

                    auto& rectChildren = transform.children;
                    rectChildren.erase(std::remove_if(rectChildren.begin(), rectChildren.end(),
                        [entity](Canis::Entity* child) -> bool
                        {
                            if (child == nullptr || !child->HasComponent<RectTransform>())
                                return true;

                            RectTransform& childTransform = child->GetComponent<RectTransform>();
                            childTransform.parent = entity;
                            return false;
                        }), rectChildren.end());
                }

                if (entity->HasComponent<Transform>())
                {
                    Transform& transform = entity->GetComponent<Transform>();
                    if (transform.parent != nullptr)
                    {
                        if (transform.parent->HasComponent<Transform>())
                        {
                            Transform& parentTransform = transform.parent->GetComponent<Transform>();
                            auto& siblings = parentTransform.children;
                            if (std::find(siblings.begin(), siblings.end(), entity) == siblings.end())
                                siblings.push_back(entity);
                        }
                        else
                        {
                            transform.parent = nullptr;
                        }
                    }

                    auto& transformChildren = transform.children;
                    transformChildren.erase(std::remove_if(transformChildren.begin(), transformChildren.end(),
                        [entity](Canis::Entity* child) -> bool
                        {
                            if (child == nullptr || !child->HasComponent<Transform>())
                                return true;

                            Transform& childTransform = child->GetComponent<Transform>();
                            childTransform.parent = entity;
                            return false;
                        }), transformChildren.end());
                }
            }

            for (auto e : newEntitys)
            {
                for (ScriptableEntity* se : e->m_scriptComponents)
                {
                    if (se)
                    {
                        se->Create();
                    }
                }
            }
        }

        m_isLoadingEntityNodes = false;

        if (!m_entitiesToDestroy.empty())
        {
            std::vector<int> pendingDestroyIds = m_entitiesToDestroy;
            m_entitiesToDestroy.clear();

            std::sort(pendingDestroyIds.begin(), pendingDestroyIds.end());
            pendingDestroyIds.erase(std::unique(pendingDestroyIds.begin(), pendingDestroyIds.end()), pendingDestroyIds.end());

            for (const int id : pendingDestroyIds)
                DestroyNow(id);
        }

        return newEntitys;
    }

    Canis::Entity& Scene::DecodeEntity(YAML::Node _node, bool _copyUUID)
    {
        Entity& entity = *CreateEntity();
        
        if (_copyUUID) {
            entity.uuid = _node["Entity"].as<Canis::UUID>(0);
        } else {
            m_targetUUIDNewUUID[_node["Entity"].as<Canis::UUID>(0)] = entity.uuid;
        }
        
        entity.name = _node["Name"].as<std::string>("");
        entity.tag = _node["Tag"].as<std::string>("");

        if (app != nullptr)
        {
            std::vector<ScriptConf>& scriptRegistry = app->GetScriptRegistry();

            for (int i = 0; i < scriptRegistry.size(); i++)
            {
                if (scriptRegistry[i].Decode)
                {
                    scriptRegistry[i].Decode(_node, entity, false);
                }
            }
        }

        return entity;
    }

    void Scene::GetEntityAfterLoad(Canis::UUID _uuid, Canis::Entity* &_variable)
    {
        if (_uuid == 0)
        {
            _variable = nullptr;
            return;
        }

        if (!m_isLoadingEntityNodes)
        {
            _variable = GetEntityWithUUID(_uuid);
            return;
        }

        EntityConnectInfo eci;
        eci.targetUUID = _uuid;
        eci.variable = &_variable;

        m_entityConnectInfo.push_back(eci);
    }

    std::vector<Entity*> Scene::Instantiate(const SceneAssetHandle &_sceneAssetHandle)
    {
        std::vector<Entity*> rootEntities = {};
        const std::string scenePath = AssetManager::ResolvePath(_sceneAssetHandle);

        if (scenePath.empty())
        {
            Debug::Warning(
                "Scene::Instantiate could not resolve scene handle (uuid=%llu, path='%s').",
                static_cast<unsigned long long>(_sceneAssetHandle.uuid),
                _sceneAssetHandle.path.c_str());
            return rootEntities;
        }

        if (!std::filesystem::exists(scenePath))
        {
            Debug::Warning("Scene::Instantiate could not find scene '%s'.", scenePath.c_str());
            return rootEntities;
        }

        try
        {
            YAML::Node root = YAML::LoadFile(scenePath);
            YAML::Node entities = root["Entities"];
            std::vector<Entity*> newEntities = LoadEntityNodes(entities, false);

            for (Entity *entity : newEntities)
            {
                if (entity != nullptr && !HasHierarchyParent(entity))
                    rootEntities.push_back(entity);
            }
        }
        catch (const YAML::Exception &_exception)
        {
            Debug::Warning(
                "Scene::Instantiate failed to load '%s': %s",
                scenePath.c_str(),
                _exception.what());
        }

        return rootEntities;
    }

    void Scene::ForceReady(Entity& _entity)
    {
        auto& scripts = _entity.m_scriptComponents;
        for (ScriptableEntity* script : scripts)
        {
            if (script == nullptr || script->m_onReadyCalled)
                continue;

            script->Ready();
            script->m_onReadyCalled = true;
        }
    }

    void Scene::Save()
    {
        Debug::Log("Save Scene");
        
        YAML::Emitter out;

        out << EncodeScene();

        if (m_path.size() > 0)
        {
            std::ofstream fout(m_path);
            fout << out.c_str();
        }
        else
        {
            std::ofstream fout(m_name);
            fout << out.c_str();
        }
    }

    YAML::Node Scene::EncodeScene()
    {
        YAML::Node node;

        YAML::Node environment;
        environment["ClearColor"] = m_window->GetClearColor();
        environment["AmbientLight"] = m_environmentAmbientLight;
        environment["AmbientLightIntensity"] = m_environmentAmbientLightIntensity;
        if ((uint64_t)m_environmentSkyboxUUID != 0)
        {
            YAML::Node skyboxAsset(YAML::NodeType::Map);
            skyboxAsset["uuid"] = (uint64_t)m_environmentSkyboxUUID;
            environment["SkyboxAsset"] = skyboxAsset;
        }
        if ((uint64_t)m_environmentPostProcessUUID != 0)
        {
            YAML::Node postProcessAsset(YAML::NodeType::Map);
            postProcessAsset["uuid"] = (uint64_t)m_environmentPostProcessUUID;
            environment["PostProcessAsset"] = postProcessAsset;
        }
        node["Environment"] = environment;

        YAML::Node entities = YAML::Node(YAML::NodeType::Sequence);

        for(Entity* entity : m_entities)
        {
            if (!entity)
                continue;

            entities.push_back(EncodeEntity(*entity));
        }

        node["Entities"] = entities;

        return node;
    }

    YAML::Node Scene::EncodeEntity(Entity &_entity)
    {
        YAML::Node node;
        node["Entity"] = _entity.uuid;
        node["Name"] = _entity.name;
        node["Tag"] = _entity.tag;

        std::vector<ScriptConf>& scriptRegistry = app->GetScriptRegistry();

        for (int i = 0; i < scriptRegistry.size(); i++)
            if (scriptRegistry[i].Encode)
                scriptRegistry[i].Encode(node, _entity);
        
        return node;
    }

    Entity* Scene::CreateEntity(std::string _name, std::string _tag)
    {
        Entity* entity = new Entity(*this);
        entity->id = m_entities.size();
        entity->name = _name;
        entity->tag = _tag;
        entity->m_entityHandle = m_registry.create();

        // TODO : handle better
        for (int i = 0; i < m_entities.size(); i++)
        {
            if (m_entities[i] == nullptr)
            {
                if (i == 0)
                    Debug::Log("The Camera Just Died");
                m_entities[i] = entity;
                entity->id = i;
                QueueEntityForReady(entity->id);
                return entity;
            }
        }
        
        m_entities.push_back(entity);
        QueueEntityForReady(entity->id);
        return entity;
    }

    Entity* Scene::GetEntity(int _id)
    {
        if (_id > -1 && _id < m_entities.size())
            return m_entities[_id];
        
        // TODO : Handle Error
        return nullptr; 
    }

    Entity* Scene::GetEntityWithUUID(Canis::UUID _uuid)
    {
        for (int i = 0; i < m_entities.size(); i++)
            if (m_entities[i] != nullptr)
                if (m_entities[i]->uuid == _uuid)
                    return m_entities[i];

        
        // TODO : Handle Error
        return nullptr; 
    }

    UUID Scene::GetLiveEntityUUID(const Entity* _entity) const
    {
        if (_entity == nullptr)
            return UUID(0);

        for (const Entity* liveEntity : m_entities)
        {
            if (liveEntity == _entity)
                return liveEntity->uuid;
        }

        return UUID(0);
    }

    Entity* Scene::FindEntityWithName(std::string _name)
    {
        for (Entity* entity : m_entities)
        {
            if (entity == nullptr)
                continue;
            
            if (entity->name == _name)
                return entity;
        }

        return nullptr;
    }

    Entity* Scene::GetEntityWithTag(std::string _tag)
    {
        for (Entity* entity : m_entities)
        {
            if (entity == nullptr)
                continue;
            
            if (entity->tag == _tag)
                return entity;
        }

        return nullptr;
    }

    std::vector<Entity*> Scene::GetEntitiesWithTag(std::string _tag)
    {
        std::vector<Entity*> entities = {};

        for (Entity* entity : m_entities)
        {
            if (entity == nullptr)
                continue;
            
            if (entity->tag == _tag)
                entities.push_back(entity);
        }

        return entities;
    }

    void Scene::Destroy(int _id)
    {
        if (_id < 0 || m_entities.size() <= _id)
        {
            Debug::Log("size %d, id %d", m_entities.size(), _id);
            return;
        }

        if (m_entities[_id] == nullptr)
        {
            Debug::Log("Why are you NULL");
            return;
        }

        if (m_isUpdating || m_isLoadingEntityNodes)
        {
            if (m_entities[_id]->active)
            {
                m_entities[_id]->active = false;
                m_entitiesToDestroy.push_back(_id);
            }
            return;
        }

        DestroyNow(_id);
    }

    void Scene::DestroyNow(int _id)
    {
        if (_id < 0 || m_entities.size() <= _id)
            return;

        Entity* entity = m_entities[_id];
        if (entity == nullptr)
            return;

        // Capture child ids before component teardown mutates hierarchy links.
        std::vector<int> childIdsToDestroy = {};
        auto queueChildForDestroy = [&](Entity* _child)
        {
            if (_child == nullptr || _child == entity)
                return;

            const int childId = _child->id;
            if (childId < 0 || childId >= static_cast<int>(m_entities.size()))
                return;

            if (m_entities[childId] != _child)
                return;

            if (std::find(childIdsToDestroy.begin(), childIdsToDestroy.end(), childId) == childIdsToDestroy.end())
                childIdsToDestroy.push_back(childId);
        };

        if (entity->HasComponent<RectTransform>())
        {
            RectTransform& rectTransform = entity->GetComponent<RectTransform>();
            if (rectTransform.parent != nullptr)
            {
                if (rectTransform.parent->HasComponent<RectTransform>())
                {
                    RectTransform& parentTransform = rectTransform.parent->GetComponent<RectTransform>();
                    auto& siblings = parentTransform.children;
                    siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
                }
                rectTransform.parent = nullptr;
            }

            for (Entity* child : rectTransform.children)
                queueChildForDestroy(child);
        }

        if (entity->HasComponent<Transform>())
        {
            Transform& transform3D = entity->GetComponent<Transform>();
            if (transform3D.parent != nullptr)
            {
                if (transform3D.parent->HasComponent<Transform>())
                {
                    Transform& parentTransform = transform3D.parent->GetComponent<Transform>();
                    auto& siblings = parentTransform.children;
                    siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
                }
                transform3D.parent = nullptr;
            }

            for (Entity* child : transform3D.children)
                queueChildForDestroy(child);
        }

        for (int childId : childIdsToDestroy)
            DestroyNow(childId);

        // Clear slot first to prevent recursive self-destroy during callbacks.
        m_entities[_id] = nullptr;

        entity->RemoveAllScripts();
        if (entity->m_entityHandle != entt::null && m_registry.valid(entity->m_entityHandle))
            m_registry.destroy(entity->m_entityHandle);
        delete entity;
    }

    void Scene::Destroy(Entity& _entity)
    {
        Destroy(_entity.id);
    }

    void Scene::QueueEntityForReady(int _id)
    {
        if (_id < 0 || _id >= (int)m_entities.size())
            return;

        m_entitiesToReady.push_back(_id);
    }

    System* Scene::CreateSystem(System* _system)
    {
        if (_system == nullptr)
            return nullptr;

        m_updateSystems.push_back(_system);
        ReadySystem(_system);
        return _system;
    }

    System* Scene::CreateRenderSystem(System* _system)
    {
        if (_system == nullptr)
            return nullptr;

        m_renderSystems.push_back(_system);
        ReadySystem(_system);
        return _system;
    }

    void Scene::ReadySystem(System *_system)
    {
        _system->scene = this;
        _system->window = m_window;
        _system->inputManager = m_inputManager;
        //_system->time = m_time;
        //_system->camera = camera;
        m_systems.push_back(_system);
        m_systemTimings.push_back(SystemTiming{
            .name = _system->GetName(),
            .updateMs = 0.0f,
            .renderMs = 0.0f,
            .system = _system
        });
    }

    Scene::SystemTiming* Scene::GetSystemTiming(System* _system)
    {
        for (SystemTiming& timing : m_systemTimings)
        {
            if (timing.system == _system)
                return &timing;
        }

        return nullptr;
    }
}
