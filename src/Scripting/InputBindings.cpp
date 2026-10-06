#include <Canis/AssetManager.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/App.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/Scripting/InputBindings.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <limits>
#include <stdexcept>

namespace Canis::Scripting {
namespace {
uint32_t Identity(uint64_t id) {
    if (!id || id > std::numeric_limits<uint32_t>::max())
        throw std::invalid_argument("Input ID must be a nonzero uint32");
    return uint32_t(id);
}
void Controller(int index) {
    if (index < -1) throw std::invalid_argument("Invalid controller index");
}
}
void RegisterInputBindings(App& app) {
    auto& b = NativeBindingRegistry::Get();
    auto input = [&app] { return &app.scene.GetInputManager(); };
    const auto owner = "Canis.Scene";
    b.Method(owner, "Input", "GetKey", [input](int key, int edge) -> bool {
        if (key < 0 || key >= 512) throw std::invalid_argument("Invalid key code");
        if (edge == 0) return input()->GetKey(key);
        if (edge == 1) return input()->JustPressedKey(key);
        if (edge == 2) return input()->JustReleasedKey(key);
        throw std::invalid_argument("Invalid input edge");
    });
    b.Method(owner, "Input", "GetButton", [input](int index, int button, int edge) -> bool {
        Controller(index);
        if (button <= 0 || button > 32767) throw std::invalid_argument("Invalid gamepad button");
        if (edge == 0) return index < 0 ? input()->GetButton(button) : input()->GetButton(index, button);
        if (edge == 1) return index < 0 ? input()->JustPressedButton(button) : input()->JustPressedButton(index, button);
        if (edge == 2) return index < 0 ? input()->JustReleasedButton(button) : input()->JustReleasedButton(index, button);
        throw std::invalid_argument("Invalid input edge");
    });
    b.Method(owner, "Input", "MouseButton", [input](bool right, int edge) -> bool {
        if (edge == 0) return right ? input()->GetRightClick() : input()->GetLeftClick();
        if (edge == 1) return right ? input()->JustRightClicked() : input()->JustLeftClicked();
        if (edge == 2) return right ? input()->RightClickReleased() : input()->LeftClickReleased();
        throw std::invalid_argument("Invalid input edge");
    });
    b.Method(owner, "Input", "Stick", [input](int index, bool right) -> Vector3 {
        Controller(index);
        return Vector3(right ? (index < 0 ? input()->GetRightStick() : input()->GetRightStick(index)) :
                              (index < 0 ? input()->GetLeftStick() : input()->GetLeftStick(index)), 0);
    });
    b.Method(owner, "Input", "Trigger", [input](int index, bool right) -> float {
        Controller(index);
        return right ? (index < 0 ? input()->GetRightTrigger() : input()->GetRightTrigger(index)) :
                       (index < 0 ? input()->GetLeftTrigger() : input()->GetLeftTrigger(index));
    });
    b.Method(owner, "Input", "MousePosition", [input]() -> Vector3 { return Vector3(input()->mouse, 0); });
    b.Method(owner, "Input", "Scroll", [input]() -> int { return input()->VerticalScroll(); });
    b.Method(owner, "Input", "Text", [input]() -> std::string { return input()->GetTextInput(); });
    b.Method(owner, "Input", "Focused", [input]() -> bool { return input()->GameInputFocused(); });
    b.Method(owner, "Input", "GamepadConnected", [input]() -> bool { return input()->HasGamepad(); });
    b.Method(owner, "Input", "Scheme", [input]() -> int { return int(input()->Actions().PromptScheme()); });
    // One typed call reads an immutable action snapshot. Vector2 travels in XY;
    // Z contains flags and W contains type/cancellation (all exactly representable).
    b.Method(owner, "Input", "Action", [input](uint64_t id, int index) -> Vector4 {
        Controller(index);
        const auto s = index < 0 ? input()->Action(ActionId{Identity(id)}) : input()->Action(ActionId{Identity(id)}, index);
        return {s.value.x, s.value.y, float(s.down | (s.pressed << 1) | (s.released << 2) | (s.canceled << 3)), float(int(s.type) + 4 * int(s.cancellation))};
    });
    b.Method(owner, "Input", "Prompt", [input](uint64_t id, int scheme) -> std::string {
        if (scheme < -1 || scheme > 1) throw std::invalid_argument("Invalid input scheme");
        return input()->Actions().Prompt({Identity(id)}, scheme < 0 ? input()->Actions().PromptScheme() : InputScheme(scheme)).text;
    });
    b.Method(owner, "Input", "PromptRevision", [input]() -> uint64_t { return input()->Actions().PromptRevision(); });
    b.Method(owner, "Input", "PromptData", [input](uint64_t id, int scheme) -> std::string {
        if(scheme < -1 || scheme > 1)throw std::invalid_argument("Invalid input scheme");
        const auto prompt=input()->Actions().Prompt({Identity(id)},scheme < 0 ? input()->Actions().PromptScheme() : InputScheme(scheme));
        ManagedJson glyphs=ManagedJson::array();
        for(const auto& glyph:prompt.glyphs) {
            auto* asset=AssetManager::GetTexture(glyph.texture);
            if(!asset)continue;
            const auto texture=asset->GetGLTexture();
            const float width=(glyph.uv.z-glyph.uv.x)*texture.width;
            const float height=(glyph.uv.w-glyph.uv.y)*texture.height;
            if(width<=0 || height<=0)continue;
            glyphs.push_back({{"Texture",glyph.texture},{"Uv",{glyph.uv.x,glyph.uv.y,glyph.uv.z,glyph.uv.w}},{"Aspect",width/height}});
        }
        return ManagedJson{{"Text",prompt.text},{"Glyphs",glyphs}}.dump();
    });
    b.Method(owner, "Input", "Map", [input](uint64_t id, bool enabled) {
        if (enabled) input()->EnableMap({Identity(id)}); else input()->DisableMap({Identity(id)});
    });
    b.Method(owner, "Input", "MapEnabled", [input](uint64_t id) -> bool { return input()->Actions().IsMapEnabled({Identity(id)}); });
    b.Method(owner, "Input", "LoadActions", [input](std::string path) {
        InputDocument document;
        std::string error;
        if (!LoadInputDocument(path, document, error)) throw std::runtime_error(error);
        if (!input()->Actions().RegisterSchema(document, error) || !input()->Actions().QueueDocument(document, error))
            throw std::runtime_error(error);
        input()->Actions().SetGlyphCatalog("assets/textures/input/glyphs.canis");
    });
}
}
