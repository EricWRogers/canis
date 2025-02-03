#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/SceneManager.hpp>

#include <unordered_map>

namespace Canis
{
    Scene::Scene(std::string _name)
    {
        name = _name;
    }

    Scene::Scene(std::string _name, std::string _path)
    {
        name = _name;
        path = _path;
    }

    Scene::~Scene()
    {
        for(System* s : systems)
            delete s;
    }

    void Scene::PreLoad()
    {
        
    }

    void Scene::Load()
    {

    }

    void Scene::UnLoad()
    {

    }

    void Scene::Update()
    {
        for(System* s : m_updateSystems)
            s->Update(entityRegistry, deltaTime);
    }

    void Scene::LateUpdate()
    {

    }

    void Scene::Draw()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        for (int i = 0; i < m_renderSystems.size(); i++)
        {
            //m_drawStart = high_resolution_clock::now();
            m_renderSystems[i]->Update(entityRegistry, deltaTime);
            //m_drawEnd = high_resolution_clock::now();
            //m_drawTime = std::chrono::duration_cast<std::chrono::nanoseconds>(m_drawEnd - m_drawStart).count() / 1000000000.0f;
            glFlush();
            //Canis::Log(std::to_string(i)+" "+std::to_string(m_drawTime));
        }
        
        //for(System* s : m_renderSystems)
        //    s->Update(entityRegistry, deltaTime);
    }

    void Scene::InputUpdate()
    {
        
    }

    bool Scene::IsPreLoaded()
    {
        return true;//preLoaded;
    }

    void Scene::ReadySystem(System *_system)
    {
        _system->scene = this;
        _system->window = window;
        _system->inputManager = inputManager;
        _system->time = time;
        _system->camera = camera;
        systems.push_back(_system);
    }

    Entity Scene::CreateEntity()
    {
        return Entity(
            entityRegistry.create(),
            this
        );
    }

    Entity Scene::CreateEntity(const std::string &_tag)
    {
        Entity e = Entity(
            entityRegistry.create(),
            this
        );

        char tag[20] = "";

        int i = 0;
        while(i < 20-1 && i < _tag.size())
        {
            tag[i] = _tag[i];
            i++;
        }
        tag[i] = '\0';

        TagComponent tagComponent = {};
        strcpy(tagComponent.tag, tag);
        e.AddComponent<TagComponent>(tagComponent);

        return e;
    }

    Entity Scene::FindEntityWithTag(const std::string &_tag)
    {
        Canis::Entity e(this);
        e.entityHandle = e.GetEntityWithTag(_tag);
        return e;
    }

    std::vector<Canis::Entity> Scene::Instantiate(const std::string &_path)
    {
        std::vector<Canis::Entity> entitiesReturn = {};
        YAML::Node& root = AssetManager::GetPrefab(_path).GetNode();
        SceneManager* sm = (SceneManager*)sceneManager;

        sm->m_loadingType = LoadingType::PREFAB;
        entityAndUUIDToConnect.clear();

        auto entities = root["Entities"];

        std::unordered_map<uint64_t, uint32_t> entityIds = {};

        if(entities)
        {
            for(auto e : entities)
            {
                Canis::Entity entity = CreateEntity();
                entitiesReturn.push_back(entity);

                uint64_t id = e["Entity"].as<uint64_t>(0);
                if (id != 0)
                    entityIds[id] = (uint32_t)(entity.entityHandle);

                for(int d = 0;  d < ((SceneManager*)sceneManager)->decodeEntity.size(); d++)
                    sm->decodeEntity[d](e, entity, sm);
                
                // think of a better way post StopTheSlime
                if (auto transform = e["Canis::Transform"])
                {
                    uint64_t parentId = transform["parent"].as<uint64_t>(0);

                    if (parentId != 0)
                    {
                        if (entityIds.find(parentId) != entityIds.end())
                        {
                            Canis::Entity parent = Canis::Entity((entt::entity)(entityIds[parentId]), this);
                            entity.SetParent(parent, false);
                        }
                    }
                }

                if (auto scriptComponent = e["Canis::ScriptComponent"])
                {
                    std::string secomponent = scriptComponent.as<std::string>();
                    for (int d = 0; d < sm->decodeScriptableEntity.size(); d++)
                        if (sm->decodeScriptableEntity[d](secomponent, entity))
                            continue;
                }
            }
        }

        sm->m_loadingType = LoadingType::SCENE;

        return entitiesReturn;
    }

    std::vector<Canis::Entity> Scene::Instantiate(const std::string &_path, glm::vec3 _position)
    {
        std::vector<Canis::Entity> entities = Instantiate(_path);

        for(auto e : entities)
            e.SetPosition(_position);
        
        return entities;
    }
} // end of Canis namespace
