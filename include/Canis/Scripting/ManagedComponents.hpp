#pragma once
#include <Canis/Entity.hpp>
#include <Canis/External/tinygltf/json.hpp>
#include <yaml-cpp/yaml.h>
#include <map>
namespace Canis { class App; class Editor; class Scene; }
namespace Canis::Scripting {
using ManagedJson = nlohmann::json;
struct ManagedAttachment {
    std::string type;
    uint64_t token = 0;
    bool enabled = true;
    ManagedJson fields = ManagedJson::object();
    std::map<std::string, Entity> references;
};
struct ManagedComponents {
    ManagedComponents() = default;
    ManagedComponents(const ManagedComponents&) = delete;
    ManagedComponents& operator=(const ManagedComponents&) = delete;
    ManagedComponents(ManagedComponents&&) = default;
    ManagedComponents& operator=(ManagedComponents&&) = default;
    static constexpr bool in_place_delete = true;
    std::vector<std::unique_ptr<ManagedAttachment>> items;
};
struct ManagedComponentOption {
    std::string type;
    std::string name;
    std::string source;
};
std::vector<ManagedComponentOption> GetAvailableManagedComponents(Entity& entity);
bool AddManagedComponent(Entity& entity, const std::string& type);
bool AddManagedComponentsFromAsset(Entity& entity, const std::string& path);
struct ManagedUIActionOption {
    std::string script; // CSharp:<stable script identity>
    std::string name;
    std::vector<std::string> actions;
};
std::vector<ManagedUIActionOption> GetManagedUIActionOptions(Entity& entity);
bool DispatchManagedUIAction(Entity& entity, const std::string& script, const std::string& action);
bool HasManagedScripts(Scene& scene);
uint64_t NextManagedToken();
ManagedJson EncodeAttachments(Entity& entity);
YAML::Node GetManagedComponentValues(Entity& entity, uint64_t token);
bool SetManagedComponentValues(Entity& entity, uint64_t token, const YAML::Node& values);
void DecodeManagedComponents(const YAML::Node& node, Entity& entity);
void EncodeManagedComponents(YAML::Node& node, Entity& entity);
void RegisterManagedBindings(App& app, std::function<uint64_t(Entity*)> handle, std::function<Entity(uint64_t)> resolve);
void DrawManagedInspector(Editor& editor, Entity& entity);
}
