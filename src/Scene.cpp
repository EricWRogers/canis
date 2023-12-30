#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/SceneManager.hpp>

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

        auto entities = root["Entities"];

        if(entities)
        {
            for(auto e : entities)
            {
                Canis::Entity entity = CreateEntity();
                entitiesReturn.push_back(entity);

                for(int d = 0;  d < ((SceneManager*)sceneManager)->decodeEntity.size(); d++) {
                    ((SceneManager*)sceneManager)->decodeEntity[d](e, entity, ((SceneManager*)sceneManager));
                }

                if (auto scriptComponent = e["Canis::ScriptComponent"])
                {
                    std::string secomponent = scriptComponent.as<std::string>();
                    for (int d = 0; d < ((SceneManager*)sceneManager)->decodeScriptableEntity.size(); d++)
                        if (((SceneManager*)sceneManager)->decodeScriptableEntity[d](secomponent, entity))
                            continue;
                }
            }
        }

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
