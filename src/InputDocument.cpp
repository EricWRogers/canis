// Input document parsing and validation shared by the runtime and host build tools.
#include <Canis/InputActions.hpp>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <stdexcept>

namespace Canis
{
const std::vector<InputControlDefinition>& InputControls()
{
    static const std::vector<InputControlDefinition> controls = {
#include <Canis/InputControls.inl>
    };
    return controls;
}
const InputControlDefinition* FindInputControl(const std::string& path)
{
    for (const auto& control : InputControls()) if (path == control.path) return &control;
    return nullptr;
}
namespace
{
const std::set<std::string> keywords = {
    "alignas","alignof","and","and_eq","asm","atomic_cancel","atomic_commit","atomic_noexcept",
    "auto","bitand","bitor","bool","break","case","catch","char","char8_t","char16_t","char32_t",
    "class","compl","concept","const","consteval","constexpr","constinit","const_cast","continue",
    "co_await","co_return","co_yield","decltype","default","delete","do","double","dynamic_cast",
    "else","enum","explicit","export","extern","false","float","for","friend","goto","if","inline",
    "int","long","mutable","namespace","new","noexcept","not","not_eq","nullptr","operator","or",
    "or_eq","private","protected","public","reflexpr","register","reinterpret_cast","requires","return",
    "short","signed","sizeof","static","static_assert","static_cast","struct","switch","synchronized",
    "template","this","thread_local","throw","true","try","typedef","typeid","typename","union",
    "unsigned","using","virtual","void","volatile","wchar_t","while","xor","xor_eq",
    "InputAction","InputMap","Canis","GeneratedInput"};
bool Identifier(const std::string& name)
{
    return std::regex_match(name, std::regex("[A-Za-z][A-Za-z0-9_]*")) &&
        name.find("__") == std::string::npos && !keywords.contains(name);
}
void Require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}
void Fields(const YAML::Node& node, std::initializer_list<const char*> allowed)
{
    Require(node.IsMap(), "Expected a mapping");
    std::set<std::string> seen;
    for (const auto& field : node) {
        const auto key = field.first.as<std::string>();
        Require(seen.insert(key).second, "Duplicate YAML field: " + key);
        Require(std::any_of(allowed.begin(), allowed.end(), [&](const char* a){ return key == a; }), "Unknown field: " + key);
    }
}
}
bool ValidateInputDocument(const InputDocument& document, std::string& error)
{
    try {
        std::set<uint32_t> ids, maps;
        std::set<std::string> names, mapNames;
        auto id = [&](uint32_t value) { Require(value != 0 && ids.insert(value).second, "Duplicate, reserved, or zero ID: " + std::to_string(value)); };
        for (auto reserved : document.reservedIds) id(reserved);
        for (const auto& map : document.maps) {
            id(map.id); maps.insert(map.id);
            Require(Identifier(map.name) && mapNames.insert(map.name).second, "Invalid or duplicate map name: " + map.name);
        }
        for (const auto& action : document.actions) {
            id(action.id);
            Require(maps.contains(action.map), "Action refers to an unknown map");
            Require(Identifier(action.name) && names.insert(action.name).second, "Invalid or duplicate action name: " + action.name);
            Require(action.type == ActionType::Button || action.type == ActionType::Axis1D || action.type == ActionType::Axis2D, "Unknown action type");
            std::set<InputScheme> primarySchemes;
            for (const auto& b : action.bindings) {
                id(b.id);
                Require(!b.primaryPrompt || primarySchemes.insert(b.scheme).second, "Multiple primary prompts for one scheme");
                Require(std::isfinite(b.deadZone) && b.deadZone >= 0 && b.deadZone < 1 &&
                    std::isfinite(b.scale) && std::isfinite(b.pressThreshold) && std::isfinite(b.releaseThreshold) &&
                    b.pressThreshold > 0 && b.pressThreshold <= 1 && b.releaseThreshold >= 0 && b.releaseThreshold < b.pressThreshold,
                    "Invalid processors or thresholds for binding " + std::to_string(b.id));
                Require(b.scheme == InputScheme::KeyboardMouse || b.scheme == InputScheme::Gamepad, "Unknown binding scheme");
                auto control = [&](const std::string& path) {
                    const auto* c = FindInputControl(path);
                    Require(c != nullptr, "Unknown control: " + path);
                    Require((path.starts_with("Gamepad/")) == (b.scheme == InputScheme::Gamepad), "Control does not match binding scheme: " + path);
                    return c;
                };
                if (b.path.starts_with("Composite/")) {
                    const bool two = b.path == "Composite/Axis2D";
                    Require(two || b.path == "Composite/Axis1D", "Unknown composite: " + b.path);
                    Require(action.type == (two ? ActionType::Axis2D : ActionType::Axis1D), "Composite/action type mismatch");
                    const std::vector<std::string> expected = two ? std::vector<std::string>{"up","down","left","right"} : std::vector<std::string>{"negative","positive"};
                    Require(b.parts.size() == expected.size(), "Incorrect composite parts");
                    for (const auto& part : expected) {
                        Require(b.parts.contains(part), "Missing composite part: " + part);
                        Require(control(b.parts.at(part))->type == ActionType::Button, "Composite parts must be buttons");
                    }
                } else {
                    Require(b.parts.empty(), "Simple binding cannot have composite parts");
                    const auto* c = control(b.path);
                    Require(c->type == action.type || (action.type == ActionType::Button && c->type == ActionType::Axis1D), "Binding/action type mismatch: " + b.path);
                }
            }
        }
        error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool ParseInputDocument(const std::string& text, InputDocument& result, std::string& error)
{
    try {
        auto root = YAML::Load(text);
        Fields(root, {"version","maps","reservedIds"});
        Require(root["version"].as<int>() == 1, "Unsupported input document version");
        Require(root["maps"].IsSequence(), "maps must be a sequence");
        InputDocument next;
        if (root["reservedIds"]) next.reservedIds = root["reservedIds"].as<std::vector<uint32_t>>();
        for (const auto& map : root["maps"]) {
            Fields(map, {"id","name","actions"});
            InputMapDefinition m{map["id"].as<uint32_t>(), map["name"].as<std::string>()};
            next.maps.push_back(m);
            Require(map["actions"].IsSequence(), "actions must be a sequence");
            for (const auto& action : map["actions"]) {
                Fields(action, {"id","name","type","bindings"});
                InputActionDefinition a;
                a.id = action["id"].as<uint32_t>(); a.map = m.id; a.name = action["name"].as<std::string>();
                const auto type = action["type"].as<std::string>();
                Require(type == "Button" || type == "Axis1D" || type == "Axis2D", "Unknown action type: " + type);
                a.type = type == "Button" ? ActionType::Button : type == "Axis1D" ? ActionType::Axis1D : ActionType::Axis2D;
                Require(action["bindings"].IsSequence(), "bindings must be a sequence");
                for (const auto& binding : action["bindings"]) {
                    Fields(binding, {"id","scheme","path","parts","deadZone","scale","invert","pressThreshold","releaseThreshold","primaryPrompt","iconOverride"});
                    InputBinding b;
                    b.id = binding["id"].as<uint32_t>(); b.path = binding["path"].as<std::string>();
                    const auto scheme = binding["scheme"].as<std::string>();
                    Require(scheme == "KeyboardMouse" || scheme == "Gamepad", "Unknown input scheme: " + scheme);
                    b.scheme = scheme == "Gamepad" ? InputScheme::Gamepad : InputScheme::KeyboardMouse;
                    if (binding["parts"]) {
                        Fields(binding["parts"], {"up","down","left","right","positive","negative"});
                        b.parts = binding["parts"].as<std::unordered_map<std::string, std::string>>();
                    }
                    b.deadZone = binding["deadZone"].as<float>(0.0f); b.scale = binding["scale"].as<float>(1.0f);
                    b.invert = binding["invert"].as<bool>(false);
                    b.primaryPrompt = binding["primaryPrompt"].as<bool>(false);
                    b.iconOverride = binding["iconOverride"].as<std::string>("");
                    b.pressThreshold = binding["pressThreshold"].as<float>(0.5f); b.releaseThreshold = binding["releaseThreshold"].as<float>(0.4f);
                    a.bindings.push_back(std::move(b));
                }
                std::sort(a.bindings.begin(), a.bindings.end(), [](const auto& a, const auto& b){return a.id < b.id;});
                next.actions.push_back(std::move(a));
            }
        }
        if (!ValidateInputDocument(next, error)) return false;
        result = std::move(next); error.clear(); return true;
    } catch (const std::exception& e) { error = e.what(); return false; }
}
bool LoadInputDocument(const std::string& path, InputDocument& result, std::string& error)
{
    std::ifstream file(path);
    if (!file) { error = "Cannot open input document: " + path; return false; }
    return ParseInputDocument(std::string(std::istreambuf_iterator<char>(file), {}), result, error);
}
}
