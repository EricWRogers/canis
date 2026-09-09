#pragma once
#include <Canis/Entity.hpp>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <yaml-cpp/yaml.h>
#include <Canis/Math.hpp>
#include <Canis/AnimationTypes.hpp>

namespace Canis
{
class App;
class Entity;
class Editor;
class ScriptableEntity;
class System;

enum class RegistryEntryKind
{
    Component,
    Script
};

enum class SystemPipeline
{
    Update,
    Render
};

struct UIActionContext
{
    Entity sourceEntity = nullptr;
    Entity targetEntity = nullptr;
    Vector2 pointerPosition = Vector2(0.0f);
    std::string payloadType = "";
    std::string payloadValue = "";
};

using UIActionInvoker = std::function<void(ScriptableEntity&, const UIActionContext&)>;

struct AnimationEventContext
{
    Entity sourceEntity = nullptr;
    Entity targetEntity = nullptr;
    std::string clipPath = "";
    std::string eventName = "";
    std::string stringPayload = "";
    float floatPayload = 0.0f;
    int intPayload = 0;
};

using AnimationEventInvoker = std::function<void(ScriptableEntity&, const AnimationEventContext&)>;

using PropertySetter = std::function<void(YAML::Node&, void*)>;
using PropertyGetter = std::function<YAML::Node(void*)>;
using PropertyDrawer = std::function<void(Editor&, const std::string&, void*, const std::string&)>;
using AnimationPropertySetter = std::function<void(void*, const AnimationValue&)>;
using AnimationPropertyGetter = std::function<AnimationValue(void*)>;

struct PropertyRegistry {
    std::map<std::string, PropertySetter> setters;
    std::map<std::string, PropertyGetter> getters;
    std::map<std::string, PropertyDrawer> drawers;
    std::map<std::string, AnimationPropertySetter> animationSetters;
    std::map<std::string, AnimationPropertyGetter> animationGetters;
    std::map<std::string, AnimationValueType> animationTypes;
    std::map<std::string, AnimationInterpolation> animationInterpolations;
    std::vector<std::string> propertyOrder;
};

struct ScriptConf {
    std::string name;
    RegistryEntryKind kind = RegistryEntryKind::Component;
    bool registeredFromGameCode = false;
    PropertyRegistry registry;
    std::function<ScriptableEntity*(Entity&, bool)> Construct = nullptr;
    std::function<void(Entity&)> Add = nullptr;
    std::function<bool(Entity&)> Has = nullptr;
    std::function<void(Entity&)> Remove = nullptr;
    std::function<void*(Entity&)> Get = nullptr;
    std::function<void(YAML::Node &_node, Entity &_entity)> Encode = nullptr;
    std::function<void(YAML::Node &_node, Entity &_entity, bool _callCreate)> Decode = nullptr;
    std::function<void(Editor&, Entity&, const ScriptConf&)> DrawInspector = nullptr;
    std::unordered_map<std::string, UIActionInvoker> uiActions = {};
    std::unordered_map<std::string, AnimationEventInvoker> animationEvents = {};
};

using ComponentConf = ScriptConf;

struct SystemConf {
    std::string name;
    SystemPipeline pipeline = SystemPipeline::Update;
    std::function<System*()> Construct = nullptr;
    bool autoCreate = true;
};

struct InspectorItemRightClick {
    std::string name;
    std::function<void(App&, Editor&, Entity&, std::vector<ScriptConf>&)> Func = nullptr;
};
} // namespace Canis
