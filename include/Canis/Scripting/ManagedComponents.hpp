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
bool HasManagedScripts(Scene& scene);
uint64_t NextManagedToken();
ManagedJson EncodeAttachments(Entity& entity);
void DecodeManagedComponents(const YAML::Node& node, Entity& entity);
void EncodeManagedComponents(YAML::Node& node, Entity& entity);
void RegisterManagedBindings(App& app, std::function<uint64_t(Entity*)> handle, std::function<Entity(uint64_t)> resolve);
void DrawManagedInspector(Editor& editor, Entity& entity);
}
