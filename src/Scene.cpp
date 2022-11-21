#include <Canis/Scene.hpp>

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
        _system->window = window;
        _system->inputManager = inputManager;
        _system->time = time;
        _system->assetManager = assetManager;
        _system->camera = camera;
    }
} // end of Canis namespace