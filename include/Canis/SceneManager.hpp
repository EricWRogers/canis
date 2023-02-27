#pragma once
#include <vector>

#include <Canis/Scene.hpp>
#include <Canis/Yaml.hpp>

#include <functional>
#include <Canis/ECS/Components/ScriptComponent.hpp>

namespace Canis
{
class SceneManager
{
private:
    int index = -1;
    int patientLoadIndex = -1;
    Scene *scene = nullptr;
    std::vector<Scene *> scenes;
    high_resolution_clock::time_point m_updateStart;
    high_resolution_clock::time_point m_updateEnd;
    high_resolution_clock::time_point m_drawStart;
    high_resolution_clock::time_point m_drawEnd;

    void Load(int _index);

public:
    SceneManager();
    ~SceneManager();

    int Add(Scene *s);
    void PreLoad(Window *_window,
                    InputManager *_inputManager,
                    Time *_time,
                    Camera *_camera,
                    AssetManager *_assetManager,
                    unsigned int _seed);

    void ForceLoad(std::string _name);
    void Load(std::string _name);
    void HotReload();
    
    std::vector<Canis::Entity> Instantiate(const std::string &_path);

    void Update();
    void LateUpdate();
    void Draw();
    void InputUpdate();

    void UnLoad();

    void SetDeltaTime(double _deltaTime);

    std::vector<std::function<bool(YAML::Node &_n, int _index, Canis::Scene *scene)>> decodeSystem = {};
    std::vector<std::function<bool(YAML::Node &_n, int _index, Canis::Scene *scene)>> decodeRenderSystem = {};
    std::vector<std::function<void(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)>> decodeEntity = {};
    std::vector<std::function<bool(const std::string &_name, Canis::Entity &_entity)>> decodeScriptableEntity = {};

    Window *window;
    InputManager *inputManager;
    Time *time;
    AssetManager *assetManager;
    Camera *camera;

    float updateTime = 0.0f;
    float drawTime = 0.0f;

    unsigned int seed = 0;
};
} // end of Canis namespace