#pragma once
#ifdef __ANDROID__
    #include <android/log.h>
    #define LOGW(...) __android_log_print(ANDROID_LOG_WARN,__VA_ARGS__)
#endif

#include <Canis/Canis.hpp>
#include <Canis/SceneManager.hpp>
#include <Canis/Scene.hpp>

//////////////// HELL

template <typename ComponentType>
void DecodeComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
{
    if (auto componentNode = _n[std::string(type_name<ComponentType>())])
    {
        auto &component = _entity.AddComponent<ComponentType>();
        auto &registry = GetPropertyRegistry<ComponentType>();

        for (const auto &[propertyName, setter] : registry.setters)
        {
            if (componentNode[propertyName])
            {
                YAML::Node propertyNode = componentNode[propertyName];
                setter(propertyNode, &component, _sceneManager);
            }
        }
    }
}

template <typename ComponentType>
void EncodeComponent(YAML::Emitter &_out, Canis::Entity &_entity)
{
    if (_entity.HasComponent<ComponentType>())
    {
        ComponentType& component = _entity.GetComponent<ComponentType>();

        _out << YAML::Key << std::string(type_name<ComponentType>());
        _out << YAML::BeginMap;

        auto &registry = GetPropertyRegistry<ComponentType>();
        for (const auto &propertyName : registry.propertyOrder)
        {
            _out << YAML::Key << propertyName << YAML::Value << registry.getters[propertyName](&component);
        }

        _out << YAML::EndMap;
    }
}

#define REGISTER_COMPONENT(AppInstance, ComponentType)                      \
{                                                                           \
    static bool registered = (ComponentType::RegisterProperties(), true);   \
    AppInstance.AddDecodeComponent(DecodeComponent<ComponentType>);         \
    /*AppInstance.AddEncodeComponent(EncodeComponent<ComponentType>);*/     \
}

#define REGISTER_UPDATE_SYSTEM(system)                                      \
{                                                                           \
    GetSystemRegistry().updateSystems.push_back(#system);                   \
}

#define REGISTER_RENDER_SYSTEM(system)                                      \
{                                                                           \
    GetSystemRegistry().renderSystems.push_back(#system);                   \
}

#define REGISTER_COMPONENT_NAME(component)                                  \
{                                                                           \
    GetComponent().names.push_back(#component);                             \
}

#define REGISTER_COMPONENT_EDITOR(component)                                                    \
{                                                                                               \
	GetComponent().addComponentFuncs[#component] = [](Canis::Entity &_entity) { 			    \
		if (_entity.HasComponent<component>() == false) { _entity.AddComponent<component>(); }  \
	};                                                                                          \
    GetComponent().removeComponentFuncs[#component] = [](Canis::Entity &_entity) {              \
        if (_entity.HasComponent<component>()) { _entity.RemoveComponent<component>(); }        \
    };                                                                                          \
    GetComponent().hasComponentFuncs[#component] = [](Canis::Entity &_entity) -> bool {         \
        return _entity.HasComponent<component>();                                               \
    };                                                                                          \
	GetComponent().names.push_back(#component);  									            \
}

//////////////// EXIT HELL

//////////////// DECODE

template <typename ComponentType>
bool DecodeScriptComponent(const std::string &_name, Canis::Entity &_entity)
{
    if (_name == std::string(type_name<ComponentType>()))
    {
        Canis::ScriptComponent scriptComponent = {};
        scriptComponent.Bind<ComponentType>();
        _entity.AddComponent<Canis::ScriptComponent>(scriptComponent);
        return true;
    }
    return false;
}

template <typename ComponentType>
std::function<bool(const std::string &, Canis::Entity &)> CreateDecodeFunction()
{
    return [](const std::string &_name, Canis::Entity &_entity) -> bool
    {
        return DecodeScriptComponent<ComponentType>(_name, _entity);
    };
}

template <typename ComponentType>
bool EncodeScriptComponent(YAML::Emitter &_out, Canis::ScriptableEntity *_scriptableEntity)
{
    if (auto castedEntity = dynamic_cast<ComponentType *>(_scriptableEntity))
    {
        _out << YAML::Key << "Canis::ScriptComponent" << YAML::Value << std::string(type_name<ComponentType>()); // typeid(ComponentType).name();
        return true;
    }
    return false;
}

template <typename ComponentType>
std::function<bool(YAML::Emitter &, Canis::ScriptableEntity *)> CreateEncodeFunction()
{
    return [](YAML::Emitter &_out, Canis::ScriptableEntity *_scriptableEntity) -> bool
    {
        return EncodeScriptComponent<ComponentType>(_out, _scriptableEntity);
    };
}

#define REGISTER_SCRIPTABLE_COMPONENT(AppInstance, ComponentType)                                                   \
{                                                                                                                   \
    AppInstance.AddEncodeScriptableEntity(CreateEncodeFunction<ComponentType>());                                   \
    AppInstance.AddDecodeScriptableEntity(CreateDecodeFunction<ComponentType>());                                   \
    GetScriptableComponentRegistry().addScriptableComponent[#ComponentType] = [](Canis::Entity &_entity) { 		    \
		if (_entity.HasScript<ComponentType>() == false) { _entity.AddScript<ComponentType>(); }                    \
	};                                                                                                              \
    GetScriptableComponentRegistry().removeScriptableComponent[#ComponentType] = [](Canis::Entity &_entity) {       \
        if (_entity.HasScript<ComponentType>()) { _entity.RemoveScript(); }                                         \
    };                                                                                                              \
    GetScriptableComponentRegistry().hasScriptableComponent[#ComponentType] = [](Canis::Entity &_entity) -> bool {  \
        return _entity.HasScript<ComponentType>();                                                                  \
    };                                                                                                              \
    GetScriptableComponentRegistry().names.push_back(#ComponentType);                                               \
}

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

    void Run(std::string _windowName);
    void Run(std::string _windowName, std::string _sceneName);
    void Loop();

private:

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