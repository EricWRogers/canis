#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>

namespace Canis
{
    Scene::Scene(std::string _name)
    {
        name = _name;
    }

    Scene::~Scene()
    {

    }

    void Scene::PreLoad()
    {
        preLoaded = true;
    }

    void Scene::Load()
    {

    }

    void Scene::UnLoad()
    {

    }

    void Scene::Update()
    {

    }

    void Scene::LateUpdate()
    {

    }

    void Scene::Draw()
    {

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
        _system->assetManager = assetManager;
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

    System* Scene::GetSystem(std::string _name)
    {
        System *system = nullptr;

        for(System *s : systems)
        {
            if (s->name == _name)
                return s;
        }

        return system;
    }
} // end of Canis namespace