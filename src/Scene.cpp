#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>

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

    void Scene::HierarchyAdd(entt::entity _parent, entt::entity _child)
    {
        int parentIndex = -1;
        for (int i = 0; i < m_hierarchyNodes.size(); i++)
        {
            if (m_hierarchyNodes[i].entity == _parent)
            {
                parentIndex = i;
                break;
            }
        }

        if (parentIndex == -1)
        {
            parentIndex = m_hierarchyNodes.size();

            HierarchyNode node;
            node.entity = _parent;
            m_hierarchyNodes.push_back(node);
        }

        HierarchyNode node;
        node.parent = parentIndex;
        node.entity = _child;
        m_hierarchyNodes.push_back(node);

        m_hierarchyNodes[parentIndex].children.push_back(m_hierarchyNodes.size() - 1);
    }

    void Scene::HierarchyRemove(entt::entity _entity)
    {
        std::vector<entt::entity> toRemove;


    }

    bool Scene::InHierarchy(entt::entity _entity)
    {
        int size = m_hierarchyNodes.size();
        for (int i = 0; i < size; i++)
        {
            if (m_hierarchyNodes[i].entity == _entity)
                return true;
        }

        return false;
    }

    Entity Scene::GetParent(entt::entity _child)
    {
        Entity e;
        e.scene = this;

        for(int i = 0; i < m_hierarchyNodes.size(); i++)
        {
            if (m_hierarchyNodes[i].entity == _child)
            {
                e.entityHandle = m_hierarchyNodes[m_hierarchyNodes[i].parent].entity;
                break;
            }
        }

        return e;
    }
    
    std::vector<entt::entity> Scene::GetRootChildren()
    {
        std::vector<entt::entity> entities;

        for (int i = 0; i < m_hierarchyNodes.size(); i++)
        {
            if (m_hierarchyNodes[i].parent == -1)
            {
                entities.push_back(m_hierarchyNodes[i].entity);
            }
        }

        return entities;
    }

    std::vector<entt::entity> Scene::GetChildren(entt::entity _entity)
    {
        std::vector<entt::entity> entities;

        GetChildren(_entity, entities);

        return entities;
    }

    void Scene::GetRootChildren(std::vector<entt::entity> &_entities)
    {
        for (int i = 0; i < m_hierarchyNodes.size(); i++)
        {
            if (m_hierarchyNodes[i].parent == -1)
            {
                _entities.push_back(m_hierarchyNodes[i].entity);
            }
        }
    }

    void Scene::GetChildren(entt::entity _entity, std::vector<entt::entity> &_entities)
    {
        for (int i = 0; i < m_hierarchyNodes.size(); i++)
        {
            if (m_hierarchyNodes[i].entity == _entity)
            {
                for(int c = 0; i < m_hierarchyNodes[i].children.size(); i++)
                {
                    _entities.push_back(m_hierarchyNodes[m_hierarchyNodes[i].children[c]].entity);
                }
                break;
            }
        }
    }
} // end of Canis namespace
