#pragma once
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <yaml-cpp/yaml.h>
#include <Canis/Math.hpp>

namespace Canis
{
class App;
class Entity;
class Editor;
class ScriptableEntity;

struct UIActionContext
{
    Entity* sourceEntity = nullptr;
    Entity* targetEntity = nullptr;
    Vector2 pointerPosition = Vector2(0.0f);
    std::string payloadType = "";
    std::string payloadValue = "";
};

using UIActionInvoker = std::function<void(ScriptableEntity&, const UIActionContext&)>;

using PropertySetter = std::function<void(YAML::Node&, void*)>;
using PropertyGetter = std::function<YAML::Node(void*)>;
using PropertyDrawer = std::function<void(Editor&, const std::string&, void*, const std::string&)>;

struct PropertyRegistry {
    std::map<std::string, PropertySetter> setters;
    std::map<std::string, PropertyGetter> getters;
    std::map<std::string, PropertyDrawer> drawers;
    std::vector<std::string> propertyOrder;
};

struct ScriptConf {
    std::string name;
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
};

struct InspectorItemRightClick {
    std::string name;
    std::function<void(App&, Editor&, Entity&, std::vector<ScriptConf>&)> Func = nullptr;
};
} // namespace Canis
