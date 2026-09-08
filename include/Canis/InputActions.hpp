#pragma once

#include <Canis/Math.hpp>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Canis
{
struct ActionId { uint32_t value; };
struct InputMapId { uint32_t value; };
using KeyCode = unsigned int;
enum class GamepadButton : unsigned int
{
    South=1, East=2, West=4, North=8, Back=16, Guide=32, Start=64,
    LeftStickPress=128, RightStickPress=256, LeftShoulder=512, RightShoulder=1024,
    DpadUp=2048, DpadDown=4096, DpadLeft=8192, DpadRight=16384
};
enum class ActionType { Button, Axis1D, Axis2D };
enum class InputScheme { KeyboardMouse, Gamepad };
enum class InputCancellation { None, FocusLost, Capture, MapDisabled, Disconnected, Reload, BackendChanged };
template<class T> struct InputActionEnum : std::false_type {};
template<class T> struct InputMapEnum : std::false_type {};
struct InputGlyph
{
    std::string texture;
    Vector4 uv = Vector4(0.0f);
    explicit operator bool() const { return !texture.empty(); }
    bool operator==(const InputGlyph&) const = default;
};
struct InputPrompt { std::string text; std::vector<InputGlyph> glyphs; };
struct ActionSnapshot
{
    ActionType type = ActionType::Button;
    bool down = false, pressed = false, released = false, canceled = false;
    InputCancellation cancellation = InputCancellation::None;
    Vector2 value = Vector2(0.0f);
    InputGlyph icon;
    std::optional<KeyCode> key;
    std::optional<GamepadButton> button;
    std::shared_ptr<const InputPrompt> prompt;
    explicit operator bool() const { return down; }
    template<class T> T Read() const
    {
        if constexpr (std::is_same_v<T, float>) {
            assert(type == ActionType::Axis1D && "Input action value type mismatch");
            return type == ActionType::Axis1D ? value.x : 0.0f;
        } else if constexpr (std::is_same_v<T, Vector2>) {
            assert(type == ActionType::Axis2D && "Input action value type mismatch");
            return type == ActionType::Axis2D ? value : Vector2(0.0f);
        } else {
            static_assert(std::is_same_v<T, float> || std::is_same_v<T, Vector2>, "Unsupported input value type");
        }
    }
};
struct InputControlDefinition { const char* path; KeyCode key; unsigned int button; ActionType type; };
const std::vector<InputControlDefinition>& InputControls();
const InputControlDefinition* FindInputControl(const std::string& path);
struct InputBinding
{
    uint32_t id = 0;
    InputScheme scheme = InputScheme::KeyboardMouse;
    std::string path;
    std::unordered_map<std::string, std::string> parts;
    float deadZone = 0.0f, scale = 1.0f, pressThreshold = 0.5f, releaseThreshold = 0.4f;
    bool invert = false;
    bool primaryPrompt = false;
    std::string iconOverride;
};
struct InputActionDefinition
{
    uint32_t id = 0, map = 0;
    std::string name;
    ActionType type = ActionType::Button;
    std::vector<InputBinding> bindings;
};
struct InputMapDefinition { uint32_t id; std::string name; };
struct InputDocument
{
    std::vector<InputMapDefinition> maps;
    std::vector<InputActionDefinition> actions;
    std::vector<uint32_t> reservedIds;
};
// All strings and collections are owned by the engine, including schema copies.
bool ParseInputDocument(const std::string& yaml, InputDocument& result, std::string& error);
bool LoadInputDocument(const std::string& path, InputDocument& result, std::string& error);
std::string SerializeInputDocument(const InputDocument& document);
bool SaveInputDocument(const std::string& path, const InputDocument& document, std::string& error);
uint32_t AllocateInputId(const InputDocument& document);
std::string InputUserOverridesPath();
bool ValidateInputDocument(const InputDocument& document, std::string& error);

class InputActionSystem
{
public:
    bool RegisterSchema(const InputDocument& schema, std::string& error);
    void UnregisterSchema();
    bool QueueDocument(const InputDocument& document, std::string& error);
    bool Load(const std::string& path, std::string& error);
    const InputDocument& Document() const { return m_document; }
    void EnableMap(InputMapId map);
    void DisableMap(InputMapId map);
    bool IsMapEnabled(InputMapId map) const;
    ActionSnapshot Action(ActionId action) const;
    // Explicit device source, not a controller-list index. Gamepad bindings only.
    ActionSnapshot Action(ActionId action, uint64_t source) const;
    void TrackController(uint64_t source, const std::string& family, bool native = false);
    void ForgetController(uint64_t source);
    std::vector<InputBinding> Bindings(ActionId action) const;
    InputPrompt Prompt(ActionId action, InputScheme scheme) const;
    std::string ControlLabel(const std::string& path) const;
    // Source 0 is local keyboard/mouse, 1 is the synthetic timeline; pads use their SDL instance ID + 2.
    void Record(const std::string& path, Vector2 value, uint64_t source = 0);
    void RecordComponent(const std::string& path, float value, unsigned int component, uint64_t source);
    void RemoveSource(uint64_t source, InputCancellation reason);
    void DiscardSourceEvents(uint64_t source);
    void Cancel(InputCancellation reason);
    void Evaluate(bool active);
    InputScheme PromptScheme() const { return m_promptScheme; }
    void BeginCapture(InputScheme scheme, ActionType type);
    void CancelCapture();
    bool IsCapturing() const { return m_capturing; }
    std::optional<std::string> TakeCapturedControl();
    bool Rebind(ActionId action, uint32_t binding, const InputBinding& replacement, std::string& error);
    void ResetBindings(std::optional<ActionId> action = std::nullopt);
    bool SaveOverrides(const std::string& path, std::string& error) const;
    bool LoadOverrides(const std::string& path, std::string& error);
    std::vector<ActionId> Conflicts(ActionId action, const InputBinding& binding) const;
    void SetGlyphFamily(const std::string& family) { m_glyphFamily = family; }
    void SetGlyphCatalog(const std::string& path);
    void SetNativeInput(bool active);
    bool HasNativeInput() const { return m_nativeInput; }
    void RecordNative(ActionId action, Vector2 value);
    void SetNativePrompt(ActionId action, InputPrompt prompt) { m_nativePrompts[action.value] = std::move(prompt); }
private:
    struct Event { std::string path; Vector2 value; uint64_t source; };
    struct State { ActionSnapshot snapshot; bool blocked = false, reactivating = false; InputCancellation pending = InputCancellation::None; };
    struct ControllerState {
        std::vector<State> states;
        std::unordered_map<uint32_t, bool> thresholds;
        std::string family;
        bool native = false, wasActive = true;
    };
    std::unordered_map<uint64_t, ControllerState> m_controllers;
    std::optional<uint64_t> m_controllerSource;
    struct Source { std::unordered_map<std::string, Vector2> controls; };
    InputDocument m_schema, m_document, m_defaults;
    std::optional<InputDocument> m_pendingDocument;
    bool m_registered = false, m_wasActive = true;
    std::unordered_map<uint32_t, size_t> m_indices;
    std::unordered_map<uint32_t, bool> m_enabled;
    std::vector<State> m_states;
    std::unordered_map<uint64_t, Source> m_sources, m_recorded;
    std::vector<Event> m_events;
    std::unordered_map<uint32_t, bool> m_thresholdDown;
    InputScheme m_promptScheme = InputScheme::KeyboardMouse;
    bool m_capturing = false, m_captureArmed = false, m_captureResolved = false;
    InputScheme m_captureScheme = InputScheme::KeyboardMouse;
    ActionType m_captureType = ActionType::Button;
    std::optional<std::string> m_captureResult;
    std::chrono::steady_clock::time_point m_captureStart;
    bool m_nativeInput = false;
    std::unordered_map<uint32_t, InputPrompt> m_nativePrompts;
    std::string m_glyphFamily = "unknown";
    std::unordered_map<std::string, InputGlyph> m_glyphs;
    std::unordered_map<std::string, std::string> m_glyphLabels;
    bool CaptureNeutral() const;
    void CaptureEvent(const Event& event);
    Vector2 Control(const std::string& path) const;
    Vector2 BindingValue(const InputBinding& binding);
    void Step(bool active);
    void EvaluateCurrent(bool active);
};
}
