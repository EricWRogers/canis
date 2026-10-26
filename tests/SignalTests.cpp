#include <Canis/ConfigHelper.hpp>
#include <iostream>
#include <stdexcept>

using namespace Canis;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Payload { float max, current, delta; };
struct Receiver : ScriptableEntity
{
    static constexpr const char* ScriptName = "SignalTest::Receiver";
    explicit Receiver(Entity& e) : ScriptableEntity(e) {}
    int calls = 0;
    float value = 0;
    std::string text;
    void TakeFloat(float v) { ++calls; value = v; }
    void TakePayload(const Payload& p) { ++calls; value = p.delta; }
    void Ping() { ++calls; }
    void TakeBool(bool v) { ++calls; value = v ? 1 : 0; }
    void TakeInt(int v) { ++calls; value = static_cast<float>(v); }
    void TakeText(const std::string& v) { ++calls; text = v; }
    void RemoveSelf() { entity.RemoveScript<Receiver>(); Check(calls == 0, "Script deleted during callback"); }
};
struct Emitter : ScriptableEntity
{
    static constexpr const char* ScriptName = "SignalTest::Emitter";
    explicit Emitter(Entity& e) : ScriptableEntity(e) {}
    Signal<float> value;
    Signal<> pulse;
    Signal<Payload> changed;
};
static ScriptConf receiverConf, emitterConf;
static void Register(App& app)
{
    receiverConf = {}; emitterConf = {};
    RegisterSignalAction(receiverConf, "TakeFloat", &Receiver::TakeFloat);
    RegisterSignalAction(receiverConf, "TakePayload", &Receiver::TakePayload);
    RegisterSignalAction(receiverConf, "Ping", &Receiver::Ping);
    RegisterSignalAction(receiverConf, "TakeBool", &Receiver::TakeBool);
    RegisterSignalAction(receiverConf, "TakeInt", &Receiver::TakeInt);
    RegisterSignalAction(receiverConf, "TakeText", &Receiver::TakeText);
    RegisterSignalAction(receiverConf, "RemoveSelf", &Receiver::RemoveSelf);
    DEFAULT_CONFIG(receiverConf, Receiver);
    REGISTER_PROPERTY(emitterConf, Emitter, value);
    REGISTER_PROPERTY(emitterConf, Emitter, pulse);
    REGISTER_PROPERTY(emitterConf, Emitter, changed);
    DEFAULT_CONFIG(emitterConf, Emitter);
    app.RegisterScript(receiverConf); app.RegisterScript(emitterConf); app.scene.app = &app;
}
static SignalListener Listener(Entity target, const char* method)
{
    SignalListener result; result.target = target; result.script = Receiver::ScriptName; result.method = method; return result;
}
static void RuntimeAndPersistence()
{
    App app; Register(app);
    auto owner = app.scene.CreateEntity("Sender"), target = app.scene.CreateEntity("Receiver");
    auto* sender = owner.AddScript<Emitter>(); auto* receiver = target.AddScript<Receiver>();
    sender->value.listeners.push_back(Listener(target, "TakeFloat"));
    sender->value.listeners.push_back(Listener(target, "Ping"));
    sender->value.Emit(2.75f);
    Check(receiver->value == 2.75f && receiver->calls == 2, "Typed or zero-argument dispatch failed");
    auto constant = Listener(target, "TakeFloat"); constant.argumentMode = SignalArgumentMode::Constant; constant.constant = 9.5f;
    sender->pulse.listeners.push_back(constant); sender->pulse.Emit();
    Check(receiver->value == 9.5f, "Saved constant was not used by no-argument signal");
    for (SignalConstant value : {SignalConstant{true}, SignalConstant{7}, SignalConstant{std::string("hello")}})
    {
        Signal<> signal; auto l = Listener(target, std::holds_alternative<bool>(value) ? "TakeBool" : std::holds_alternative<int>(value) ? "TakeInt" : "TakeText");
        l.argumentMode = SignalArgumentMode::Constant; l.constant = value; signal.listeners.push_back(l);
        Signal<> loaded; Check(DecodeSignal(EncodeSignal(signal), app.scene, loaded), "Constant decode failed"); loaded.Emit();
    }
    Check(receiver->value == 7 && receiver->text == "hello", "Primitive constants changed type or value");
    sender->changed.listeners.push_back(Listener(target, "TakePayload")); sender->changed.Emit({100, 80, -20});
    Check(receiver->value == -20, "Structured payload was not dispatched with its real type");
    auto wrong = Listener(target, "TakePayload"); sender->value.listeners.push_back(wrong);
    auto disabled = Listener(target, "Ping"); disabled.enabled = false; sender->value.listeners.push_back(disabled);
    Entity missingTarget; app.scene.GetEntityAfterLoad(UUID(999), missingTarget);
    auto missing = Listener(missingTarget, "Ping"); sender->value.listeners.push_back(missing);
    const auto calls = receiver->calls; sender->value.Emit(1.25f);
    Check(receiver->calls == calls + 2 && !sender->value.IsListenerValid(wrong), "Disabled, missing or incompatible binding fired");
    int runtime = 0; sender->value.Connect([&](const float&) { ++runtime; });
    const auto saved = emitterConf.registry.getters.at("value")(sender);
    Signal<float> loaded; Check(DecodeSignal(saved, app.scene, loaded), "Registered signal did not serialize");
    Check(YAML::Dump(saved) == YAML::Dump(EncodeSignal(loaded)), "Listener settings did not round-trip");
    loaded.Emit(3); Check(runtime == 0, "Native callbacks were serialized");
    Check(loaded.listeners.back().target.GetUUID() == UUID(999), "Missing target UUID was lost");
    for (const auto* input : {"null", "42", "{bad: map}", "[17]", "[{mode: Unknown}]", "[{mode: Constant, constant: {type: bad, value: 1}}]"})
    {
        Check(!DecodeSignal(YAML::Load(input), app.scene, loaded), "Malformed bindings were accepted");
        Check(YAML::Dump(saved) == YAML::Dump(EncodeSignal(loaded)), "Malformed data replaced authored bindings");
    }
    // Missing method stays authored across reload; no fallback to a different action.
    loaded.listeners.front().method = "RemovedMethod";
    Signal<float> again; Check(DecodeSignal(EncodeSignal(loaded), app.scene, again), "Missing method decode failed");
    Check(again.listeners.front().method == "RemovedMethod" && !again.IsListenerValid(again.listeners.front()), "Missing method was silently reassigned");
    target.RemoveScript<Receiver>(); sender->value.Emit(1);
    receiver = target.AddScript<Receiver>(); sender->value.Emit(4.5f);
    Check(receiver->value == 4.5f, "Signal retained a stale script instance after reattachment");
}
static void PrefabRemapping()
{
    App app; Register(app);
    auto source = app.scene.CreateEntity("Source"), target = app.scene.CreateEntity("Target");
    auto* emitter = source.AddScript<Emitter>(); target.AddScript<Receiver>();
    emitter->value.listeners.push_back(Listener(target, "TakeFloat"));
    YAML::Node entities(YAML::NodeType::Sequence);
    entities.push_back(app.scene.EncodeEntity(source)); entities.push_back(app.scene.EncodeEntity(target));
    auto first = app.scene.LoadEntityNodes(entities, false), second = app.scene.LoadEntityNodes(entities, false);
    auto* a = first[0]->GetScript<Emitter>(); auto* b = second[0]->GetScript<Emitter>();
    Check(a->value.listeners[0].target == first[1] && b->value.listeners[0].target == second[1] &&
        a->value.listeners[0].target != target, "Prefab listener targets escaped their instance");
    a->value.Emit(12.5f);
    Check(first[1]->GetScript<Receiver>()->value == 12.5f && second[1]->GetScript<Receiver>()->value == 0,
        "Prefab signal fired on another instance");
    auto yaml = emitterConf.registry.getters.at("value")(a);
    Check(yaml[0]["target"].as<UUID>() == first[1]->GetUUID(), "Resaving did not use remapped UUID");
}
static void CallbackMutationAndLifetime()
{
    Signal<> signal; int a = 0, b = 0;
    Signal<>::Connection first = 0;
    first = signal.Connect([&] { ++a; signal.Disconnect(first); signal.Connect([&] { ++b; }); });
    signal.Emit(); Check(a == 1 && b == 0, "Connections changed during an active emission");
    signal.Emit(); Check(a == 1 && b == 1, "Connection changes were not applied on next emission");
    App app; Register(app); auto target = app.scene.CreateEntity("Self removing");
    target.AddScript<Receiver>(); Signal<> removal; removal.listeners.push_back(Listener(target, "RemoveSelf"));
    removal.Emit(); Check(!target.HasScript<Receiver>(), "Action could not remove its own script"); removal.Emit();
    auto* receiver = target.AddScript<Receiver>();
    Signal<> weak; weak.Connect(ScriptHandle<Receiver>(receiver), &Receiver::Ping);
    target.RemoveScript<Receiver>(); weak.Emit();
    auto* emitter = new Signal<>; bool later = false;
    emitter->Connect([&] { delete emitter; }); emitter->Connect([&] { later = true; }); emitter->Emit();
    Check(later, "Destroying emitter invalidated its emission snapshot");
    struct Owner { Signal<std::string> signal; std::string text = std::string(100, 'x'); };
    auto* data = new Owner;
    bool copied = false;
    data->signal.Connect([&](const auto&) { delete data; });
    data->signal.Connect([&](const auto& text) { copied = text == std::string(100, 'x'); });
    data->signal.Emit(data->text);
    Check(copied, "Payload referred into a destroyed emitter");
    auto* selfRemoving = target.AddScript<Receiver>();
    Signal<> weakRemoval;
    weakRemoval.Connect(ScriptHandle<Receiver>(selfRemoving), &Receiver::RemoveSelf);
    weakRemoval.Emit(); Check(!target.HasScript<Receiver>(), "Native receiver could not remove itself safely");
}
#if CANIS_EDITOR
static void InspectorControls()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = ImVec2(900, 800); io.DeltaTime = 1.f / 60;
    unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    {
        App app; Register(app); auto target = app.scene.CreateEntity("Receiver"); target.AddScript<Receiver>();
        Editor editor; int targetsDrawn = 0; Signal<float> first, second;
        first.listeners.push_back(Listener(target, "TakeFloat")); second.listeners.push_back(Listener(target, "Ping"));
        editor.RegisterInspectorFieldDrawer<Entity>([&](Editor&, const char*, const char*, Entity&) { ++targetsDrawn; ImGui::TextUnformatted("Receiver"); });
        ImVec2 add;
        auto frame = [&](ImVec2 mouse = {-100, -100}, bool down = false, bool expanded = true)
        {
            targetsDrawn = 0; io.AddMousePosEvent(mouse.x, mouse.y); io.AddMouseButtonEvent(0, down);
            ImGui::NewFrame(); ImGui::SetNextWindowPos({0, 0}); ImGui::SetNextWindowSize({880, 780});
            ImGui::Begin("Signals", nullptr, ImGuiWindowFlags_NoSavedSettings);
            ImGui::SetNextItemOpen(expanded, ImGuiCond_Always); DrawInspectorField(editor, "Changed", "first", first);
            const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax(); add = {(min.x + max.x) / 2, (min.y + max.y) / 2};
            ImGui::SetNextItemOpen(expanded, ImGuiCond_Always); DrawInspectorField(editor, "Changed", "second", second);
            ImGui::End(); ImGui::Render();
        };
        frame(); Check(targetsDrawn == 2, "Signal targets did not use the registered entity picker");
        const auto point = add; frame(point); frame(point, true); frame(point, false);
        Check(first.listeners.size() == 2 && second.listeners.size() == 1, "Add listener failed or IDs collided between signals");
        frame({-100, -100}, false, false); Check(targetsDrawn == 0, "Collapsed signals still draw listener fields");
    }
    ImGui::DestroyContext();
}
#endif
int main()
{
    try { RuntimeAndPersistence(); PrefabRemapping(); CallbackMutationAndLifetime();
#if CANIS_EDITOR
        InspectorControls();
#endif
        std::cout << "Serializable signal checks passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
