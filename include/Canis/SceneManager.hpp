#pragma once
#include <vector>
#include <functional>

#include <Canis/Editor.hpp>
#include <Canis/ScriptableEntity.hpp>
#include <Canis/ECS/Components/ScriptComponent.hpp>


namespace Canis
{
struct SceneData
{
    Scene* scene;
    bool preloaded = false;
    bool splashScreen = false;
    bool splashScreenHasCalledLoadAll = false;
};

struct EntityAndUUID {
    Entity *entity = nullptr;
    UUID uuid;
};

struct HierarchyElementInfo {
    std::string name;
    Entity entity;
};

class SceneManager
{
friend class Editor;

private:
    int m_sceneIndex = -1;
    int patientLoadIndex = -1;
    Scene *scene = nullptr;
    std::vector<SceneData> m_scenes;
    high_resolution_clock::time_point m_updateStart;
    high_resolution_clock::time_point m_updateEnd;
    high_resolution_clock::time_point m_drawStart;
    high_resolution_clock::time_point m_drawEnd;

    Editor m_editor;

    void Load(int _index);
    void SetUpIMGUI(Window *_window);
public:
    SceneManager();
    ~SceneManager();

    int Add(Scene *s);
    int AddSplashScene(Scene *s);
    void PreLoad(std::string _sceneName);
    void PreLoadAll();

    void ForceLoad(std::string _name);
    void Load(std::string _name);

    void HotReload();

    void Save();

    void Update();
    void LateUpdate();
    void Draw();
    void InputUpdate();

    void UnLoad();

    void SetDeltaTime(double _deltaTime);

    bool IsSplashScene(std::string _sceneName);

    void FindEntityEditor(Entity& _entity, UUID &_uuid);

    std::vector<std::function<bool(const std::string &_name, Canis::Scene *scene)>> decodeSystem = {};
    std::vector<std::function<bool(const std::string &_name, Canis::Scene *scene)>> decodeRenderSystem = {};
    std::vector<std::function<void(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)>> decodeEntity = {};
    std::vector<std::function<bool(const std::string &_name, Canis::Entity &_entity)>> decodeScriptableEntity = {};

    std::vector<std::function<void(YAML::Emitter &_out, Canis::Entity &_entity)>> encodeEntity = {};
    std::vector<std::function<bool(YAML::Emitter &_out, Canis::ScriptableEntity* _scriptableEntity)>> encodeScriptableEntity = {};

    Window *window;
    InputManager *inputManager;
    Time *time;
    Camera *camera;

    float updateTime = 0.0f;
    float drawTime = 0.0f;

    unsigned int seed = 0;
    bool running = true;

    std::unordered_map<std::string, std::string> message = {};
    std::unordered_map<std::string, std::string> nextMessage = {};

    std::vector<EntityAndUUID> entityAndUUIDToConnect = {};
    std::vector<HierarchyElementInfo> hierarchyElements = {};
};
} // end of Canis namespace