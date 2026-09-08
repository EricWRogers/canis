#include <Canis/InputActions.hpp>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_keycode.h>
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
bool InputActionSystem::RegisterSchema(const InputDocument& schema, std::string& error)
{
    if (!ValidateInputDocument(schema, error)) return false;
    UnregisterSchema();
    m_schema = schema; m_document = schema; m_registered = true;
    for (size_t i = 0; i < schema.actions.size(); ++i) m_indices.emplace(schema.actions[i].id, i);
    m_states.resize(schema.actions.size());
    for (size_t i = 0; i < m_states.size(); ++i) m_states[i].snapshot.type = schema.actions[i].type;
    return true;
}
void InputActionSystem::UnregisterSchema()
{
    Cancel(InputCancellation::Reload);
    m_schema = {}; m_document = {}; m_defaults = {}; m_pendingDocument.reset(); m_indices.clear(); m_states.clear();
    m_enabled.clear(); m_thresholdDown.clear(); m_controllers.clear(); m_registered = false;
    SetNativeInput(false); CancelCapture();
    // Keep physical samples across module reload so held controls can be gated.
}
bool InputActionSystem::QueueDocument(const InputDocument& document, std::string& error)
{
    if (!ValidateInputDocument(document, error)) return false;
    if (!m_registered || document.actions.size() != m_schema.actions.size() || document.maps.size() != m_schema.maps.size()) {
        error = "Input schema changed; rebuild and reload game code"; return false;
    }
    for (const auto& map : document.maps) {
        if (std::none_of(m_schema.maps.begin(), m_schema.maps.end(), [&](const auto& m){ return m.id == map.id && m.name == map.name; })) {
            error = "Input maps changed; rebuild and reload game code"; return false;
        }
    }
    InputDocument ordered = document;
    ordered.actions.clear();
    for (const auto& schema : m_schema.actions) {
        auto it = std::find_if(document.actions.begin(), document.actions.end(), [&](const auto& a){ return a.id == schema.id; });
        if (it == document.actions.end() || it->type != schema.type || it->name != schema.name || it->map != schema.map) {
            error = "Input action schema changed; rebuild and reload game code"; return false;
        }
        ordered.actions.push_back(*it);
    }
    m_defaults = ordered;
    m_pendingDocument = std::move(ordered); error.clear(); return true;
}
bool InputActionSystem::Load(const std::string& path, std::string& error)
{
    InputDocument next;
    return LoadInputDocument(path, next, error) && QueueDocument(next, error);
}
void InputActionSystem::EnableMap(InputMapId map)
{
    if (!IsMapEnabled(map)) {
        m_enabled[map.value] = true;
        for (auto& [source, controller] : m_controllers)
            for (size_t i = 0; i < controller.states.size(); ++i) if (m_document.actions[i].map == map.value) {
                controller.states[i].blocked = true; controller.states[i].reactivating = true;
            }
        for (size_t i = 0; i < m_states.size(); ++i) if (m_document.actions[i].map == map.value) { m_states[i].blocked = true; m_states[i].reactivating = true; }
    }
}
void InputActionSystem::DisableMap(InputMapId map)
{
    if (!IsMapEnabled(map)) return;
    m_enabled[map.value] = false;
    for (auto& [source, controller] : m_controllers)
        for (size_t i = 0; i < controller.states.size(); ++i) if (m_document.actions[i].map == map.value) {
            controller.states[i].pending = InputCancellation::MapDisabled; controller.states[i].blocked = true;
        }
    for (size_t i = 0; i < m_states.size(); ++i) if (m_document.actions[i].map == map.value) {
        m_states[i].pending = InputCancellation::MapDisabled; m_states[i].blocked = true;
    }
}
bool InputActionSystem::IsMapEnabled(InputMapId map) const
{
    auto it = m_enabled.find(map.value); return it != m_enabled.end() && it->second;
}
ActionSnapshot InputActionSystem::Action(ActionId action) const
{
    auto it = m_indices.find(action.value);
    return it == m_indices.end() ? ActionSnapshot{} : m_states[it->second].snapshot;
}
ActionSnapshot InputActionSystem::Action(ActionId action, uint64_t source) const
{
    const auto index = m_indices.find(action.value);
    if (index == m_indices.end()) return {};
    const auto controller = m_controllers.find(source);
    if (controller != m_controllers.end()) return controller->second.states[index->second].snapshot;
    ActionSnapshot neutral; neutral.type = m_schema.actions[index->second].type;
    return neutral;
}
void InputActionSystem::TrackController(uint64_t source, const std::string& family, bool native)
{
    auto [it, inserted] = m_controllers.try_emplace(source);
    auto& controller = it->second;
    if (inserted) {
        controller.states.resize(m_schema.actions.size());
        for (size_t i = 0; i < controller.states.size(); ++i) {
            controller.states[i].snapshot.type = m_schema.actions[i].type;
            controller.states[i].blocked = controller.states[i].reactivating = true;
        }
    } else if (controller.native != native) {
        for (auto& state : controller.states) {
            state.pending = InputCancellation::BackendChanged;
            state.blocked = state.reactivating = true;
        }
        controller.thresholds.clear();
    }
    controller.family = family; controller.native = native;
}
void InputActionSystem::ForgetController(uint64_t source)
{
    m_controllers.erase(source);
}
std::vector<InputBinding> InputActionSystem::Bindings(ActionId action) const
{
    auto it = m_indices.find(action.value);
    auto result = it == m_indices.end() ? std::vector<InputBinding>{} : m_document.actions[it->second].bindings;
    if (m_nativeInput && it != m_indices.end()) {
        std::erase_if(result, [](const auto& b){return b.scheme == InputScheme::Gamepad;});
        InputBinding native; native.scheme = InputScheme::Gamepad; native.path = "Steam/Action_" + std::to_string(action.value);
        result.push_back(std::move(native));
    }
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if(a.primaryPrompt != b.primaryPrompt) return a.primaryPrompt;
        return a.id < b.id;
    });
    return result;
}
std::string InputActionSystem::ControlLabel(const std::string& path) const
{
    if (const auto* control = FindInputControl(path); control && control->key) {
        const auto key = SDL_GetKeyFromScancode(static_cast<SDL_Scancode>(control->key), SDL_KMOD_NONE, false);
        const char* localized = SDL_GetKeyName(key);
        if (localized && *localized) return localized;
    }
    auto label = m_glyphLabels.find(m_glyphFamily + ":" + path);
    if (label != m_glyphLabels.end()) return label->second;
    const auto name = path.substr(path.find('/') + 1);
    return std::regex_replace(name, std::regex("([a-z])([A-Z])"), "$1 $2");
}
InputPrompt InputActionSystem::Prompt(ActionId action, InputScheme scheme) const
{
    if (scheme == InputScheme::Gamepad && m_nativeInput) {
        auto native = m_nativePrompts.find(action.value);
        return native != m_nativePrompts.end() ? native->second : InputPrompt{"Steam Input", {}};
    }
    for (const auto& b : Bindings(action)) if (b.scheme == scheme) {
        InputPrompt prompt;
        auto append = [&](const std::string& path) {
            if (!prompt.text.empty()) prompt.text += " / ";
            const std::string label = ControlLabel(path);
            const std::string family = path.starts_with("Keyboard/") ? "keyboard_en_us" : m_glyphFamily;
            auto glyph = m_glyphs.find(family + ":" + path);
            // US key artwork is only valid when its printed label matches the current layout.
            static const std::unordered_map<std::string, std::string> punctuation = {
                {"Keyboard/Grave", "`"}, {"Keyboard/Minus", "-"}, {"Keyboard/Equals", "="},
                {"Keyboard/Leftbracket", "["}, {"Keyboard/Rightbracket", "]"}, {"Keyboard/Backslash", "\\"},
                {"Keyboard/Semicolon", ";"}, {"Keyboard/Apostrophe", "'"}, {"Keyboard/Comma", ","},
                {"Keyboard/Period", "."}, {"Keyboard/Slash", "/"}
            };
            const auto printed = punctuation.find(path);
            const bool layoutMatches = printed != punctuation.end() ? label == printed->second :
                !path.starts_with("Keyboard/") || path.size() != 10 || label == path.substr(9);
            if (glyph != m_glyphs.end() && layoutMatches) prompt.glyphs.push_back(glyph->second);
            prompt.text += label;
        };
        if (b.parts.empty()) append(b.path);
        else {
            const std::vector<std::string> order = b.path == "Composite/Axis2D" ? std::vector<std::string>{"up","left","down","right"} : std::vector<std::string>{"negative","positive"};
            for (const auto& part : order) append(b.parts.at(part));
        }
        if (!b.iconOverride.empty()) prompt.glyphs = {{b.iconOverride, Vector4(0,0,1,1)}};
        return prompt;
    }
    return {"Unbound", {}};
}
void InputActionSystem::SetNativeInput(bool active)
{
    if (active == m_nativeInput) return;
    m_nativeInput = active;
    RemoveSource(UINT64_MAX, InputCancellation::BackendChanged);
    m_nativePrompts.clear();
}
void InputActionSystem::RecordNative(ActionId action, Vector2 value)
{
    Record("Native/" + std::to_string(action.value), value, UINT64_MAX);
}
void InputActionSystem::Record(const std::string& path, Vector2 value, uint64_t source)
{
    if (!std::isfinite(value.x) || !std::isfinite(value.y)) value = Vector2(0.0f);
    auto& previous = m_recorded[source].controls[path];
    if (previous == value) return;
    previous = value;
    m_events.push_back({path, value, source});
}
void InputActionSystem::RecordComponent(const std::string& path, float value, unsigned int component, uint64_t source)
{
    if (component > 1) return;
    Vector2 next = m_recorded[source].controls[path];
    next[component] = value;
    Record(path, next, source);
}
void InputActionSystem::RemoveSource(uint64_t source, InputCancellation reason)
{
    m_sources.erase(source); m_recorded.erase(source);
    std::erase_if(m_events, [&](const auto& e){return e.source == source;});
    for (auto& state : m_states) {
        state.pending = reason; state.blocked = state.reactivating = true;
    }
    for (auto& [id, controller] : m_controllers) if (id == source || (source == UINT64_MAX && controller.native))
        for (auto& state : controller.states) {
            state.pending = reason; state.blocked = state.reactivating = true;
        }
}
void InputActionSystem::DiscardSourceEvents(uint64_t source)
{
    std::erase_if(m_events, [&](const auto& e){ return e.source == source; });
    m_recorded.erase(source);
}
void InputActionSystem::Cancel(InputCancellation reason)
{
    for (auto& [source, controller] : m_controllers)
        for (auto& state : controller.states) {
            state.pending = reason; state.blocked = true;
            if (reason == InputCancellation::BackendChanged || reason == InputCancellation::Disconnected)
                state.reactivating = true;
        }
    for (auto& state : m_states) {
        state.pending = reason; state.blocked = true;
        if (reason == InputCancellation::BackendChanged || reason == InputCancellation::Disconnected)
            state.reactivating = true;
    }
}
Vector2 InputActionSystem::Control(const std::string& path) const
{
    Vector2 value(0.0f); uint64_t winner = UINT64_MAX;
    for (const auto& [id, source] : m_sources) {
        if (m_controllerSource && id != *m_controllerSource) continue;
        auto it = source.controls.find(path);
        if (it == source.controls.end()) continue;
        const auto v = it->second;
        if (glm::dot(v,v) > glm::dot(value,value) || (glm::dot(v,v) == glm::dot(value,value) && id < winner)) { value=v; winner=id; }
    }
    return value;
}
Vector2 InputActionSystem::BindingValue(const InputBinding& b)
{
    Vector2 value(0.0f);
    if (b.path == "Composite/Axis2D") {
        value = Vector2(Control(b.parts.at("right")).x - Control(b.parts.at("left")).x,
                        Control(b.parts.at("up")).x - Control(b.parts.at("down")).x);
        if (glm::length(value) > 1.0f) value = glm::normalize(value);
    } else if (b.path == "Composite/Axis1D") value.x = Control(b.parts.at("positive")).x - Control(b.parts.at("negative")).x;
    else value = Control(b.path);
    const float length = glm::length(value);
    if (length <= b.deadZone) value = Vector2(0.0f);
    else if (b.deadZone > 0) value *= (std::min(length,1.0f)-b.deadZone) / ((1.0f-b.deadZone)*length);
    return value * b.scale * (b.invert ? -1.0f : 1.0f);
}
void InputActionSystem::Step(bool active)
{
    for (size_t i=0; i<m_states.size(); ++i) {
        auto& state = m_states[i]; auto& snapshot = state.snapshot; const auto& action = m_document.actions[i];
        Vector2 value(0.0f); uint32_t winner = UINT32_MAX;
        for (const auto& b : action.bindings) {
            if (m_controllerSource && (b.scheme != InputScheme::Gamepad || m_nativeInput)) continue;
            auto contribution = BindingValue(b);
            if (action.type == ActionType::Button) {
                const bool previous = m_thresholdDown[b.id];
                const float magnitude = std::abs(contribution.x);
                const bool down = previous ? magnitude > b.releaseThreshold : magnitude >= b.pressThreshold;
                m_thresholdDown[b.id] = down; contribution = Vector2(down ? 1.0f : 0.0f, 0.0f);
            }
            const float magnitude = glm::dot(contribution, contribution);
            if (magnitude > glm::dot(value,value) || (magnitude == glm::dot(value,value) && b.id < winner)) {value = contribution; winner = b.id;}
        }
        if (m_nativeInput) {
            auto native = Control("Native/" + std::to_string(action.id));
            if (glm::dot(native,native) > glm::dot(value,value)) value = native;
        }
        if (state.reactivating) continue;
        if (!active || !IsMapEnabled({action.map})) state.blocked = true;
        else if (glm::dot(value,value) == 0.0f) state.blocked = false;
        if (!active || !IsMapEnabled({action.map}) || state.blocked) value = Vector2(0.0f);
        const bool down = glm::dot(value,value) > 0.0f;
        snapshot.pressed |= down && !snapshot.down;
        snapshot.released |= !down && snapshot.down;
        snapshot.down = down; snapshot.value = value;
    }
}
void InputActionSystem::Evaluate(bool active)
{
    if (m_pendingDocument) {
        m_document = std::move(*m_pendingDocument); m_pendingDocument.reset();
        m_thresholdDown.clear();
        for (auto& [source, controller] : m_controllers) controller.thresholds.clear();
        Cancel(InputCancellation::Reload);
    }
    // Replay the same ordered samples with independent state and hysteresis per device.
    // Queries only read the published snapshots, even on their first call.
    if (m_controllers.empty()) {
        EvaluateCurrent(active); m_events.clear(); return;
    }
    const auto previousSources = m_sources;
    const bool controllerActive = active && !m_capturing;
    for (auto& [source, controller] : m_controllers) {
        m_controllerSource = controller.native ? UINT64_MAX : source;
        std::swap(m_states, controller.states);
        std::swap(m_thresholdDown, controller.thresholds);
        std::swap(m_wasActive, controller.wasActive);
        std::swap(m_glyphFamily, controller.family);
        std::swap(m_nativeInput, controller.native);
        const auto scheme = m_promptScheme;
        m_promptScheme = InputScheme::Gamepad;
        EvaluateCurrent(controllerActive);
        m_promptScheme = scheme;
        std::swap(m_nativeInput, controller.native);
        std::swap(m_glyphFamily, controller.family);
        std::swap(m_wasActive, controller.wasActive);
        std::swap(m_thresholdDown, controller.thresholds);
        std::swap(m_states, controller.states);
        m_sources = previousSources;
    }
    m_controllerSource.reset();
    EvaluateCurrent(active);
    m_events.clear();
}
void InputActionSystem::EvaluateCurrent(bool active)
{
    if (!m_controllerSource && m_capturing && std::chrono::steady_clock::now() - m_captureStart > std::chrono::seconds(10)) CancelCapture();
    const bool captureOwnedFrame = m_capturing;
    active = active && !captureOwnedFrame;
    if (!m_controllerSource && m_capturing && CaptureNeutral()) {
        if (m_captureResolved) m_capturing = false;
        else m_captureArmed = true;
    }
    if (!active && m_wasActive) {
        for (auto& state : m_states) {
            if (state.pending == InputCancellation::None) state.pending = InputCancellation::Capture;
            state.blocked = true;
        }
    }
    if (active && !m_wasActive)
        for (auto& state : m_states) { state.blocked = true; state.reactivating = true; }
    m_wasActive = active;
    for (auto& state : m_states) {
        auto& s = state.snapshot;
        s.pressed = s.released = s.canceled = false; s.cancellation = InputCancellation::None;
        if (state.pending != InputCancellation::None) {
            s.canceled = s.down; s.cancellation = s.canceled ? state.pending : InputCancellation::None;
            s.down = false; s.value = Vector2(0.0f); state.pending = InputCancellation::None;
        }
    }
    // First evaluate the previous sample: a newly enabled map must see neutral
    // before accepting this frame's first transition.
    Step(active);
    for (const auto& event : m_events) {
        if (m_controllerSource && event.source != *m_controllerSource) continue;
        auto& previous = m_sources[event.source].controls[event.path];
        if (!m_controllerSource && event.path != "Mouse/Delta" && event.path != "Mouse/Wheel" &&
            glm::length(event.value) > 0.25f && glm::length(event.value-previous) > 0.15f)
            m_promptScheme = (event.path.starts_with("Gamepad/") || event.path.starts_with("Native/")) ? InputScheme::Gamepad : InputScheme::KeyboardMouse;
        previous = event.value;
        if (!m_controllerSource) CaptureEvent(event);
        Step(active);
    }
    if (!m_controllerSource && m_capturing && CaptureNeutral()) {
        if (m_captureResolved) m_capturing = false;
        else m_captureArmed = true;
    }
    // SDL can report held controls again on focus gain. A reactivated action
    // inspects the final collected sample before opening its neutral gate.
    for (auto& state : m_states) state.reactivating = false;
    Step(active);
    for (size_t i=0; i<m_states.size(); ++i) {
        auto& s = m_states[i].snapshot;
        s.key.reset(); s.button.reset();
        bool keyboardSeen=false, gamepadSeen=false;
        for (const auto& b : Bindings({m_document.actions[i].id})) {
            const auto* control = FindInputControl(b.path);
            if (b.scheme == InputScheme::KeyboardMouse && !b.path.starts_with("Mouse/") && !keyboardSeen) {
                keyboardSeen=true;
                if (control && control->key) s.key = control->key;
            }
            if (b.scheme == InputScheme::Gamepad && !gamepadSeen) {
                gamepadSeen=true;
                if (control && control->button) s.button = static_cast<GamepadButton>(control->button);
            }
        }
        if (m_nativeInput) s.button.reset();
        if (m_controllerSource) s.key.reset();
        auto prompt = Prompt({m_document.actions[i].id}, m_promptScheme);
        // Reuse immutable prompt data while its presentation remains unchanged.
        if (!s.prompt || s.prompt->text != prompt.text || s.prompt->glyphs != prompt.glyphs) s.prompt = std::make_shared<const InputPrompt>(std::move(prompt));
        bool composite = false;
        if (!(m_nativeInput && m_promptScheme == InputScheme::Gamepad))
            for (const auto& b : Bindings({m_document.actions[i].id})) if (b.scheme == m_promptScheme) { composite = !b.parts.empty(); break; }
        s.icon = !composite && s.prompt->glyphs.size() == 1 ? s.prompt->glyphs.front() : InputGlyph{};
    }
}
}
