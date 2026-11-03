#include <Canis/Yaml.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/App.hpp>
#include <Canis/Components.hpp>
#include <Canis/Editor.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Window.hpp>
#include <Canis/ECS/Systems/UIInteractionSystem.hpp>
#include <iostream>
#include <array>
#include <SDL3/SDL.h>
#include <stdexcept>
using namespace Canis;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Receiver : ScriptableEntity { using ScriptableEntity::ScriptableEntity; int clicks = 0; };
int main()
{
    Window window("Button navigation tests", 320, 240, true);
    App app;
    InputManager input;
    app.scene.Init(&app, &window, &input);
    Editor editor;
    app.RegisterDefaults(editor);
    auto fadeRoot=app.scene.CreateEntity("UI fade group");
    fadeRoot.AddComponent<RectTransform>()->opacity=.5f;
    auto fadeChild=app.scene.CreateEntity("UI fade child");
    auto& childRect=*fadeChild.AddComponent<RectTransform>();
    childRect.parent=fadeRoot;childRect.opacity=.5f;
    Check(childRect.GetOpacity()==.25f,"UI subtree opacity did not multiply through parents");
    YAML::Node encodedOpacity;
    app.GetScriptConf("Canis::RectTransform")->Encode(encodedOpacity,fadeChild);
    Check(encodedOpacity["Canis::RectTransform"]["opacity"].as<float>()==.5f,"UI opacity did not serialize");
    auto opacitySource=YAML::Load(
        "- {Entity: 201, Name: Fading, 'Canis::RectTransform': {opacity: 0.5}}\n"
        "- {Entity: 202, Name: Backdrop, 'Canis::RectTransform': {}}\n");
    auto opacityNodes=app.scene.LoadEntityNodes(opacitySource,false);
    Check(opacityNodes[0]->GetComponent<RectTransform>().GetOpacity()==.5f &&
          opacityNodes[1]->GetComponent<RectTransform>().GetOpacity()==1,
          "UI opacity decode changed the default background opacity");

    // Passive HUD queries use the same transformed bounds as native controls.
    {
        auto root=app.scene.CreateEntity("Picking canvas");
        root.AddComponent<RectTransform>()->size={320,240};
        auto& queryCanvas=*root.AddComponent<Canvas>();queryCanvas.screenSize={320,240};
        auto front=app.scene.CreateEntity("Raised card"),back=app.scene.CreateEntity("Lower card");
        for(auto widget:{front,back}) {
            auto& rect=*widget.AddComponent<RectTransform>();rect.parent=root;rect.size={80,20};
            root.GetComponent<RectTransform>().children.push_back(widget);
        }
        auto& frontRect=front.GetComponent<RectTransform>();frontRect.rotation=glm::radians(45.f);frontRect.depth=.2f;
        back.GetComponent<RectTransform>().depth=.1f;
        input.mouse={195,120};
        Check(!UIInteractionSystem::HitTest(front),"Rotated UI hit test used its unrotated box");
        input.mouse={175,135};
        Check(UIInteractionSystem::HitTest(front),"Rotated UI hit test missed visible artwork");
        input.mouse={160,120};
        Check(UIInteractionSystem::Pick(std::array{front,back})==front,"UI picking ignored rendered overlay order");
        queryCanvas.screenSize={640,480};frontRect.rotation=0;
        input.mouse={185,120};Check(!UIInteractionSystem::HitTest(front),"UI hit test ignored canvas scaling");
        input.mouse={175,120};Check(UIInteractionSystem::HitTest(front),"Scaled UI hit test missed artwork");
        auto& button=*front.AddComponent<UIButton>();button.active=false;
        frontRect.size={10,10};back.GetComponent<RectTransform>().size={100,100};
        input.mouse={180,120};
        Check(!UIInteractionSystem::HitTest(front),"Passive query used another widget's rectangle");
        frontRect.size={100,100};
        Check(UIInteractionSystem::HitTest(front),"Passive query ignored a disabled control's own rectangle");
        root.Active()=false;
        Check(!UIInteractionSystem::HitTest(front),"UI hit test accepted a hidden ancestor");
        root.Destroy();input.mouse=Vector2(0);
    }

    ScriptConf receiver;
    receiver.name = "Receiver";
    receiver.kind = RegistryEntryKind::Script;
    receiver.Get = [](Entity& e) -> void* { return e.GetScript<Receiver>(); };
    receiver.uiActions["Click"] = [](ScriptableEntity& script, const UIActionContext&) { ++static_cast<Receiver&>(script).clicks; };
    app.RegisterScript(receiver);
    auto owner = app.scene.CreateEntity("Receiver");
    auto* listener = owner.AddScript<Receiver>();
    auto makeButton = [&](const char* name) {
        auto e = app.scene.CreateEntity(name);
        auto& rect = *e.AddComponent<RectTransform>();
        rect.position = Vector2(1000); // Keep the pointer outside the fixture.
        rect.size = Vector2(40);
        auto& button = *e.AddComponent<UIButton>();
        button.targetEntity = owner;
        button.targetScript = "Receiver";
        button.actionName = "Click";
        return e;
    };
    Entity first = makeButton("First"), second = makeButton("Second"), disabled = makeButton("Disabled");
    first.AddComponent<Text>()->color = Color(1);
    first.GetComponent<UIButton>().hoverColor = Color(.26f, .9f, .376f, 1);
    first.GetComponent<UIButton>().hoverScale = 1.08f;
    second.AddComponent<Sprite2D>()->color = Color(1);
    second.GetComponent<UIButton>().hoverColor = Color(1);
    second.GetComponent<UIButton>().hoverScale = 1.08f;
    // Enabling and disabled appearance belong to the native button system.
    first.GetComponent<UIButton>().disabledColor = Color(.2f,.3f,.4f,1);
    UIInteractionSystem::SetButtonActive(first,false);
    Check(!first.GetComponent<UIButton>().active &&
          first.GetComponent<Text>().color == Color(.2f,.3f,.4f,1),
          "Native disabling did not apply the authored disabled color immediately");
    UIInteractionSystem::SetButtonActive(first,true);
    Check(first.GetComponent<Text>().color == Color(1) &&
          first.GetComponent<UIButton>().hoverScale == 1.08f,
          "Re-enabling lost the authored button appearance");
    first.GetComponent<UIButton>().defaultSelected = true;
    first.GetComponent<UIButton>().down = disabled;
    disabled.GetComponent<UIButton>().active = false;
    disabled.GetComponent<UIButton>().down = second;
    second.GetComponent<UIButton>().up = first;
    auto initialCanvas = app.scene.CreateEntity("Menu canvas");
    initialCanvas.AddComponent<RectTransform>();
    auto& initial = *initialCanvas.AddComponent<Canvas>();
    initial.navigateAction = 31; initial.confirmAction = 32;
    for (auto e : {first, second, disabled}) e.GetComponent<RectTransform>().parent = initialCanvas;
    InputDocument initialDocument;
    initialDocument.maps = {{30, "Menu"}};
    initialDocument.actions = {
        {31, 30, "Navigate", ActionType::Axis2D, {
            {41, InputScheme::KeyboardMouse, "Composite/Axis2D", {{"up", "Keyboard/Up"}, {"down", "Keyboard/Down"}, {"left", "Keyboard/Left"}, {"right", "Keyboard/Right"}}},
            {42, InputScheme::Gamepad, "Composite/Axis2D", {{"up", "Gamepad/DpadUp"}, {"down", "Gamepad/DpadDown"}, {"left", "Gamepad/DpadLeft"}, {"right", "Gamepad/DpadRight"}}}}},
        {32, 30, "Confirm", ActionType::Button, {{43, InputScheme::KeyboardMouse, "Keyboard/Enter"}, {44, InputScheme::Gamepad, "Gamepad/South"}}}
    };
    auto initialSchema = initialDocument;
    for (auto& action : initialSchema.actions) action.bindings.clear();
    std::string initialError;
    Check(input.Actions().RegisterSchema(initialSchema, initialError), initialError.c_str());
    Check(input.Actions().QueueDocument(initialDocument, initialError), initialError.c_str());
    input.Actions().EnableMap({30});
    UIInteractionSystem system;
    system.scene = &app.scene; system.window = &window; system.inputManager = &input;
    auto frame = [&] { input.Actions().Evaluate(true); system.Update(app.scene.GetRegistry(), .016f); input.BeginSyntheticInputFrame(); };
    auto key = [&](unsigned int code, bool down) { input.SetSyntheticKey(code, down); frame(); };
    input.mouse = Vector2(0); input.mouseRel = Vector2(0);
    frame();
    key(Key::DOWN, true);
    Check(first.GetComponent<UIButton>().hovered, "First direction must acquire default focus");
    Check(first.GetComponent<Text>().color == first.GetComponent<UIButton>().hoverColor, "Focused text did not turn green");
    Check(first.GetComponent<RectTransform>().scale.x == 1.08f, "Focused text did not enlarge");
    key(Key::DOWN, false); key(Key::DOWN, true);
    Check(second.GetComponent<UIButton>().hovered && !first.GetComponent<UIButton>().hovered, "Disabled link was not skipped");
    Check(second.GetComponent<Sprite2D>().color == Color(1), "Focused artwork was tinted");
    Check(second.GetComponent<RectTransform>().scale.x == 1.08f, "Focused artwork did not enlarge");
    Check(first.GetComponent<RectTransform>().scale.x == 1, "Old focus did not restore its size");
    key(Key::DOWN, false); key(Key::RETURN, true);
    Check(second.GetComponent<UIButton>().pressed && listener->clicks == 0, "Confirm should press before activating");
    frame(); key(Key::RETURN, false);
    Check(listener->clicks == 1, "Confirm release must dispatch exactly once");
    frame(); Check(listener->clicks == 1, "Idle frame repeated activation");
    input.SetSyntheticGamepadButton(ControllerButton::DPAD_UP, true); frame();
    Check(first.GetComponent<UIButton>().hovered, "Gamepad direction failed");
    input.SetSyntheticGamepadButton(ControllerButton::DPAD_UP, false); frame();
    input.SetSyntheticGamepadButton(ControllerButton::A, true); frame();
    input.SetSyntheticGamepadButton(ControllerButton::A, false); frame();
    Check(listener->clicks == 2, "Gamepad confirmation failed");
    key(Key::RETURN, true);
    first.Destroy(); key(Key::RETURN, false);
    Check(listener->clicks == 2, "Destroyed pressed button dispatched an action");

    // Both activation paths route C# bindings through the managed attachment identity.
    Scripting::DecodeManagedComponents(YAML::Load("Canis::ManagedScripts: [{type: test.buttons, enabled: true, fields: {}}]"), owner);
    const auto token = owner.GetComponent<Scripting::ManagedComponents>().items[0]->token;
    int managedClicks = 0;
    app.scene.managedUIAction = [&](uint64_t attachment, const std::string& action) {
        Check(attachment == token && action == "Click", "Button lost managed callback identity");
        ++managedClicks;
        return true;
    };
    second.GetComponent<UIButton>().defaultSelected = true;
    second.GetComponent<UIButton>().targetScript = "CSharp:test.buttons";
    key(Key::RETURN, true); key(Key::RETURN, false);
    Check(managedClicks == 1, "Keyboard activation did not route to C#");
    auto& rect = second.GetComponent<RectTransform>();
    input.mouse = rect.GetRectMin() + rect.originOffset + rect.GetResolvedSize() * .5f + Vector2(160, 120);
    input.mouseRel = Vector2(1);
    input.SetSyntheticMouseButton(1, true); frame();
    input.mouseRel = Vector2(0);
    input.SetSyntheticMouseButton(1, false); frame();
    Check(managedClicks == 2, "Mouse activation did not route to C#");
    rect.position = Vector2(0);
    rect.size = Vector2(100);
    input.mouse = Vector2(160, 120); input.mouseRel = Vector2(1);
    input.SetSyntheticMouseButton(1, true); frame();
    input.mouseRel = Vector2(0); input.SetSyntheticMouseButton(1, false); frame();
    Check(managedClicks == 3, "Button's own rectangle did not activate the button");
    input.mouse = Vector2(0);

    auto canvasEntity = app.scene.CreateEntity("Action-mapped menu");
    canvasEntity.AddComponent<RectTransform>();
    auto& canvas = *canvasEntity.AddComponent<Canvas>();
    canvas.navigateAction = 11; canvas.confirmAction = 12;
    canvas.nextAction = 13; canvas.previousAction = 14; canvas.reverseNavigationAction = 15;
    second.GetComponent<RectTransform>().parent = canvasEntity;
    Entity third = makeButton("Third");
    third.GetComponent<RectTransform>().parent = canvasEntity;
    third.GetComponent<UIButton>().targetScript = "CSharp:test.buttons";
    second.GetComponent<UIButton>().down = third;
    third.GetComponent<UIButton>().down = second;
    third.GetComponent<UIButton>().up = second;
    second.GetComponent<UIButton>().up = third;
    InputDocument document;
    document.maps = {{10, "Menu"}};
    document.actions = {
        {11, 10, "Navigate", ActionType::Axis2D, {{21, InputScheme::Gamepad, "Gamepad/LeftStick"}}},
        {12, 10, "Confirm", ActionType::Button, {{22, InputScheme::KeyboardMouse, "Keyboard/Z"}}},
        {13, 10, "Next", ActionType::Button, {{23, InputScheme::KeyboardMouse, "Keyboard/N"}}},
        {14, 10, "Previous", ActionType::Button, {{24, InputScheme::KeyboardMouse, "Keyboard/P"}}},
        {15, 10, "Reverse", ActionType::Button, {{25, InputScheme::KeyboardMouse, "Keyboard/LeftShift"}}}
    };
    auto schema = document; for (auto& action : schema.actions) action.bindings.clear();
    std::string error;
    Check(input.Actions().RegisterSchema(schema, error), error.c_str());
    Check(input.Actions().QueueDocument(document, error), error.c_str());
    input.Actions().EnableMap({10});
    auto mappedFrame = [&](float dt = .016f) { input.Actions().Evaluate(true); system.Update(app.scene.GetRegistry(), dt); input.BeginSyntheticInputFrame(); };
    mappedFrame();
    input.Actions().Record("Gamepad/LeftStick", Vector2(0, -1)); mappedFrame();
    Check(third.GetComponent<UIButton>().hovered, "Mapped stick did not follow button links");
    mappedFrame(.1f); Check(third.GetComponent<UIButton>().hovered, "Navigation repeated too soon");
    mappedFrame(.3f); Check(second.GetComponent<UIButton>().hovered, "Held mapped direction did not repeat");
    input.Actions().Record("Gamepad/LeftStick", Vector2(0)); mappedFrame();
    input.Actions().Record("Keyboard/Z", Vector2(1, 0)); mappedFrame();
    input.Actions().Record("Keyboard/Z", Vector2(0)); mappedFrame();
    Check(managedClicks == 4, "Rebound confirmation did not call the C# binding");
    system.SetTabOrder({second, disabled, third});
    input.Actions().Record("Keyboard/N", Vector2(1, 0)); mappedFrame();
    Check(system.SelectedButton() == third, "Mapped Next did not move focus");
    input.Actions().Record("Keyboard/N", Vector2(0)); mappedFrame();
    input.Actions().Record("Keyboard/LeftShift", Vector2(1, 0)); mappedFrame();
    Check(system.SelectedButton() == third, "Reverse modifier moved focus without Next");
    input.Actions().Record("Keyboard/N", Vector2(1, 0)); mappedFrame();
    Check(system.SelectedButton() == second, "Mapped reverse modifier did not reverse traversal");
    input.Actions().Record("Keyboard/N", Vector2(0));
    input.Actions().Record("Keyboard/LeftShift", Vector2(0)); mappedFrame();
    input.Actions().Record("Keyboard/P", Vector2(1, 0)); mappedFrame();
    Check(system.SelectedButton() == third, "Mapped Previous did not move focus");
    input.Actions().Record("Keyboard/P", Vector2(0)); mappedFrame();
    system.SelectButton(second);
    Check(second.GetComponent<UIButton>().activated, "Native owner did not receive confirmation event");
    second.GetComponent<UIButton>().activated = false;
    second.GetComponent<UIButton>().adjustable = true;
    input.Actions().Record("Gamepad/LeftStick", Vector2(1, 0)); mappedFrame();
    Check(second.GetComponent<UIButton>().adjustment == 1, "Slider did not receive mapped horizontal adjustment");
    Check(system.SelectedButton() == second, "Slider adjustment moved focus");
    input.Actions().Record("Gamepad/LeftStick", Vector2(0)); mappedFrame();
    input.SetSyntheticKey(Key::RETURN, true); mappedFrame();
    input.SetSyntheticKey(Key::RETURN, false); mappedFrame();
    Check(managedClicks == 4, "Unbound Enter bypassed the mapped confirm action");
    canvas.receivesEvents = false; mappedFrame();
    Check(system.SelectedButton() == second, "Temporary input gate discarded saved focus");
    canvas.receivesEvents = true; mappedFrame();
    canvas.navigationEnabled = false;
    input.Actions().Record("Keyboard/Z", Vector2(1, 0)); mappedFrame();
    canvas.navigationEnabled = true;
    input.Actions().Record("Keyboard/Z", Vector2(0)); mappedFrame();
    Check(managedClicks == 4, "Text editing leaked a confirmation into button navigation");
    input.Actions().Record("Keyboard/Z", Vector2(1, 0)); mappedFrame();
    input.Actions().DisableMap({10}); mappedFrame();
    Check(managedClicks == 4, "Canceled action incorrectly activated a button");
    app.scene.managedUIAction = {};

    // XR pointer reaches a world-space button without a desktop camera (the VR lobby).
    auto vrButton = makeButton("VR button");
    vrButton.AddComponent<Transform>()->position = Vector3(0,0,-4);
    auto& vrCanvas = *vrButton.AddComponent<Canvas>();
    vrCanvas.renderMode = CanvasRenderMode::WORLD_SPACE;
    vrCanvas.interactionDistance = 6;
    vrCanvas.navigationEnabled = false;
    auto& vrRect = vrButton.GetComponent<RectTransform>();
    vrRect.position = Vector2(0); vrRect.size = Vector2(1);
    auto& pointer = input.worldUIPointer;
    pointer.enabled = pointer.active = pointer.moved = true;
    frame();
    Check(vrButton.GetComponent<UIButton>().hovered, "XR ray did not highlight a distant button without a camera");
    const int beforeVR = listener->clicks;
    pointer.down = pointer.pressed = true; frame();
    pointer.pressed = false; frame();
    Check(vrButton.GetComponent<UIButton>().pressed && listener->clicks == beforeVR, "XR trigger activated before release");
    pointer.down = false; pointer.released = true; frame();
    Check(listener->clicks == beforeVR+1, "XR trigger release did not activate the pointed button");
    pointer.released = false; frame();
    Check(listener->clicks == beforeVR+1, "XR pointer repeated a click");
    pointer.down = pointer.pressed = true; frame();
    pointer.active = false; pointer.pressed = pointer.down = false; frame();
    pointer.active = true; pointer.released = true; frame();
    Check(listener->clicks == beforeVR+1, "Tracking loss dispatched a stale XR click");
    pointer.released = false; pointer.direction = Vector3(1,0,0); frame();
    Check(!vrButton.GetComponent<UIButton>().hovered, "Parallel XR ray highlighted a button");
    pointer = {}; vrButton.Destroy();

    // UIInputField owns SDL text entry for native and managed focus requests.
    auto fieldEntity = app.scene.CreateEntity("Editable text");
    fieldEntity.AddComponent<RectTransform>()->position = Vector2(1000);
    auto& field = *fieldEntity.AddComponent<UIInputField>();
    field.maxLength = 3;
    Check(system.FocusInputField(fieldEntity), "Programmatic field focus failed");
    Check(field.focused && SDL_TextInputActive(static_cast<SDL_Window*>(window.GetSDLWindow())), "Field did not begin platform text input");
    UIInteractionSystem::InsertInputText(field, "aé😀z");
    Check(field.text == "aé😀", "Character limit split a UTF-8 character");
    UIInteractionSystem::BackspaceInputText(field);
    Check(field.text == "aé", "Backspace corrupted UTF-8");
    UIInteractionSystem::SetInputText(field, "");
    field.allowedCharacters = "abc";
    UIInteractionSystem::InsertInputText(field, "a!b\nc");
    Check(field.text == "abc", "Input validation did not filter pasted text");
    field.active = false;
    frame();
    Check(!field.focused && !SDL_TextInputActive(static_cast<SDL_Window*>(window.GetSDLWindow())), "Disabled field retained text focus");
    field.active = true;
    Check(system.FocusInputField(fieldEntity), "Field did not regain focus");
    key(Key::RETURN, true);
    Check(!field.focused, "Enter did not submit the input field");
    key(Key::RETURN, false);
    field.submitOnEnter = false;
    Check(system.FocusInputField(fieldEntity), "Managed-submit field did not focus");
    key(Key::RETURN, true);
    Check(field.focused, "Managed-submit field consumed Enter");
    key(Key::RETURN, false);
    fieldEntity.Destroy();
    frame();
    Check(!SDL_TextInputActive(static_cast<SDL_Window*>(window.GetSDLWindow())), "Destroyed field left platform text input active");
    auto otherField = app.scene.CreateEntity("Unload text focus");
    otherField.AddComponent<RectTransform>()->position = Vector2(1000);
    otherField.AddComponent<UIInputField>();
    Check(system.FocusInputField(otherField), "Second field did not focus");
    system.OnDestroy();
    Check(!SDL_TextInputActive(static_cast<SDL_Window*>(window.GetSDLWindow())), "System teardown left text input active");

    canvas.nextAction = 13; canvas.previousAction = 14;
    YAML::Node encodedCanvas;
    app.GetScriptConf("Canis::Canvas")->Encode(encodedCanvas, canvasEntity);
    Check(encodedCanvas["Canis::Canvas"]["nextAction"].as<unsigned>() == 13 &&
          encodedCanvas["Canis::Canvas"]["previousAction"].as<unsigned>() == 14,
          "Canvas traversal action IDs were not serialized");

    // Scene serialization must preserve links and remap them on duplication.
    auto* conf = app.GetScriptConf("Canis::UIButton");
    second.GetComponent<UIButton>().defaultSelected = true;
    second.GetComponent<UIButton>().right = disabled;
    YAML::Node encoded;
    conf->Encode(encoded, second);
    Check(encoded["Canis::UIButton"]["targetScript"].as<std::string>() == "CSharp:test.buttons", "C# callback binding not serialized");
    Check(encoded["Canis::UIButton"]["defaultSelected"].as<bool>(), "Default selection not serialized");
    Check(encoded["Canis::UIButton"]["disabledColor"].as<Color>() == second.GetComponent<UIButton>().disabledColor, "Disabled color was not serialized");
    Check(encoded["Canis::UIButton"]["right"].as<UUID>() == disabled.GetUUID(), "Directional link not serialized");
    YAML::Node nodes = YAML::Load("- {Entity: 101, Name: A, 'Canis::UIButton': {defaultSelected: true, down: 102, disabledColor: [0.1, 0.2, 0.3, 1]}}\n- {Entity: 102, Name: B, 'Canis::UIButton': {up: 101}}\n");
    auto loaded = app.scene.LoadEntityNodes(nodes, false);
    Check(loaded[0]->GetComponent<UIButton>().down == loaded[1], "Duplicated directional link did not resolve");
    Check(loaded[1]->GetComponent<UIButton>().up == loaded[0], "Reverse link did not resolve");
    Check(loaded[0]->GetComponent<UIButton>().disabledColor == Color(.1f,.2f,.3f,1), "Authored disabled color did not decode");
    std::cout << "Button navigation, confirmation, lifetime and serialization passed.\n";
}
