#pragma once
#ifdef __ANDROID__
    #include <android/log.h>
    #define LOGW(...) __android_log_print(ANDROID_LOG_WARN,__VA_ARGS__)
#endif

#include <Canis/Canis.hpp>
#include <Canis/SceneManager.hpp>
#include <Canis/Scene.hpp>

namespace Canis
{

enum AppState
{
    ON,
    OFF
};

class App
{
public:
    App(const std::string &_organization, const std::string &_app);
    ~App();

    void AddDecodeSystem(std::function<bool(const std::string &_name, Canis::Scene *scene)> _func);
    void AddDecodeRenderSystem(std::function<bool(const std::string &_name, Canis::Scene *scene)> _func);
    void AddDecodeComponent(std::function<void(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)> _func);
    void AddDecodeScriptableEntity(std::function<bool(const std::string &_name, Canis::Entity &_entity)> _func);

    void AddEncodeComponent(std::function<void(YAML::Emitter &_out, Canis::Entity &_entity)> _func);
    void AddEncodeScriptableEntity(std::function<bool(YAML::Emitter &_out, Canis::ScriptableEntity* _scriptableEntity)> _func);

    void AddScene(Scene *_scene);
    void AddSplashScene(Scene *_scene);

    void Run( std::string _windowName, std::string _sceneName);

private:
    void Loop();

    Canis::SceneManager sceneManager;

    AppState appState = AppState::OFF;

    Canis::Window window;
    Canis::Time time;
    Canis::InputManager inputManager;
    Canis::Camera camera = Canis::Camera(glm::vec3(0.0f, 0.15f, -0.3f),glm::vec3(0.0f, 1.0f, 0.0f),Canis::YAW+90.0f,Canis::PITCH+0.0f);
    
    double deltaTime;

    unsigned int seed;
};
} 