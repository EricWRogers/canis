#include <Canis/ComponentClipboard.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/VFX/Particles.hpp>
#include <imgui.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace Canis;
using namespace Canis::Scripting;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

struct Tuning : ScriptableEntity
{
    static constexpr const char* ScriptName = "ClipboardTest::Tuning";
    explicit Tuning(Entity& owner) : ScriptableEntity(owner) {}
    float speed = 1;
    std::string label = "default";
    Entity target = nullptr;
    std::vector<Entity> targets;
    Signal<> pulse;
    int created = 0, runtimeCounter = 0;
    float received = 0;
    void Create() override { ++created; }
    void Configure(float amount, bool enabled) { received = enabled ? amount : 0; }
};
struct NativeValueComponent
{
    static constexpr const char* ScriptName = "ClipboardTest::NativeValue";
    Entity* entity = nullptr;
    float value = 0;
    int created = 0;
    void Create() { ++created; }
};
static ScriptConf tuningConf;
static ComponentConf valueConf, transformConf;
struct Fixture
{
    App app;
    Fixture()
    {
        app.scene.app = &app;
        tuningConf = {}; valueConf = {}; transformConf = {};
        REGISTER_PROPERTY(tuningConf, Tuning, speed);
        REGISTER_PROPERTY(tuningConf, Tuning, label);
        REGISTER_PROPERTY(tuningConf, Tuning, target);
        REGISTER_PROPERTY(tuningConf, Tuning, targets);
        REGISTER_PROPERTY(tuningConf, Tuning, pulse);
        RegisterSignalAction(tuningConf, "Configure", &Tuning::Configure, std::array<std::string, 2>{"amount", "enabled"});
        DEFAULT_CONFIG(tuningConf, Tuning);
        app.RegisterScript(tuningConf);
        REGISTER_PROPERTY(valueConf, NativeValueComponent, value);
        DEFAULT_COMPONENT_CONFIG(valueConf, NativeValueComponent);
        app.RegisterComponent(valueConf);
        REGISTER_PROPERTY(transformConf, Transform, active);
        REGISTER_PROPERTY(transformConf, Transform, position);
        REGISTER_PROPERTY(transformConf, Transform, rotation);
        REGISTER_PROPERTY(transformConf, Transform, scale);
        REGISTER_PROPERTY(transformConf, Transform, parent);
        REGISTER_PROPERTY(transformConf, Transform, children);
        DEFAULT_COMPONENT_CONFIG(transformConf, Transform);
        app.RegisterComponent(transformConf);
    }
};

static void NativeValues()
{
    Fixture f;
    auto source = f.app.scene.CreateEntity("Source"), destination = f.app.scene.CreateEntity("Destination");
    auto receiver = f.app.scene.CreateEntity("Receiver");
    auto* sink = receiver.AddScript<Tuning>();
    auto* from = source.AddScript<Tuning>();
    auto* to = destination.AddScript<Tuning>();
    from->speed = 7.5f; from->label = "custom"; from->target = receiver; from->targets = {receiver, nullptr};
    from->runtimeCounter = 99; to->runtimeCounter = 12;
    SignalListener listener;
    listener.target = receiver; listener.script = Tuning::ScriptName; listener.method = "Configure";
    listener.argumentMode = SignalArgumentMode::Configured;
    listener.arguments = YAML::Load("{amount: 4.5, enabled: true}");
    from->pulse.listeners.push_back(listener);
    int nativeCallbacks = 0;
    to->pulse.Connect([&] { ++nativeCallbacks; });
    ComponentClipboard clipboard;
    std::string error;
    Check(!clipboard.CanPaste(tuningConf, destination), "Empty clipboard accepted paste");
    Check(clipboard.Copy(tuningConf, source, error), error.c_str());
    from->speed = 100;
    from->pulse.listeners[0].arguments["amount"] = 999;
    source.Destroy();
    Check(clipboard.Paste(tuningConf, destination, error), error.c_str());
    Check(to == destination.GetScript<Tuning>() && to->speed == 7.5f && to->label == "custom" && to->target == receiver &&
          to->targets.size() == 2 && to->targets[0] == receiver && !to->targets[1], "Serialized values/references were not pasted");
    Check(to->created == 1 && to->runtimeCounter == 12 && destination.GetName() == "Destination",
          "Paste recreated the script or changed private runtime state/entity identity");
    to->pulse.Emit();
    Check(sink->received == 4.5f && nativeCallbacks == 1, "Signal paste lost configured arguments or native callbacks");
    to->pulse.listeners[0].arguments["amount"] = 2;
    Check(clipboard.Paste(tuningConf, destination, error), error.c_str());
    to->pulse.Emit();
    Check(sink->received == 4.5f, "Pasted Signal arguments aliased clipboard data");
    destination.AddComponent<NativeValueComponent>();
    Check(!clipboard.Paste(valueConf, destination, error), "Different component type accepted copied script values");
    Check(!clipboard.Copy(valueConf, receiver, error) && clipboard.TypeName() == Tuning::ScriptName,
          "Failed copy discarded the existing clipboard");

    const auto decode = tuningConf.Decode;
    auto rejecting = tuningConf;
    rejecting.Decode = [decode](YAML::Node& node, Entity& entity, bool create) {
        decode(node, entity, create);
        if (entity.GetScript<Tuning>()->speed == 7.5f) throw std::runtime_error("Rejected tuning");
    };
    to->speed = 3;
    Check(!clipboard.Paste(rejecting, destination, error) && to->speed == 3,
          "Failed decoder left partially pasted values");
}

static void ComponentsAndHierarchy()
{
    Fixture f;
    auto source = f.app.scene.CreateEntity("Source"), destination = f.app.scene.CreateEntity("Destination");
    auto sourceParent = f.app.scene.CreateEntity("Source parent"), destinationParent = f.app.scene.CreateEntity("Destination parent");
    sourceParent.AddComponent<Transform>(); destinationParent.AddComponent<Transform>();
    auto sourceChild = f.app.scene.CreateEntity("Source child"), destinationChild = f.app.scene.CreateEntity("Destination child");
    auto* from = source.AddComponent<Transform>(); from->SetParent(&sourceParent, false);
    auto* to = destination.AddComponent<Transform>(); to->SetParent(&destinationParent, false);
    sourceChild.AddComponent<Transform>()->SetParent(&source, false);
    destinationChild.AddComponent<Transform>()->SetParent(&destination, false);
    from->position = Vector3(3, 4, 5); from->scale = Vector3(2); from->rotation = Quaternion(Vector3(0, .3f, 0));
    const auto uuid = destination.GetUUID();
    ComponentClipboard clipboard; std::string error;
    Check(clipboard.Copy(transformConf, source, error) && clipboard.Paste(transformConf, destination, error), error.c_str());
    Check(to->position == from->position && to->scale == from->scale && to->rotation == from->rotation,
          "Transform values were not pasted");
    Check(to->parent == destinationParent && to->children.size() == 1 && to->children[0] == destinationChild &&
          destinationChild.GetComponent<Transform>().parent == destination && destination.GetUUID() == uuid,
          "Transform paste copied hierarchy or entity identity");
    auto* fromValue = source.AddComponent<NativeValueComponent>(); fromValue->value = 42;
    auto* toValue = destination.AddComponent<NativeValueComponent>();
    Check(clipboard.Copy(valueConf, source, error) && clipboard.Paste(valueConf, destination, error), error.c_str());
    Check(toValue->value == 42 && toValue->created == 1 && toValue->entity == destination.TryGet(),
          "Native component paste copied owner pointer or reran Create");
}

static void ManagedValues()
{
    Fixture f;
    RegisterManagedBindings(f.app, [](Entity*) { return uint64_t(0); }, [](uint64_t) { return Entity(nullptr); });
    const auto manifest = ManagedJson::array({{{"id", "stable.counter"}, {"aliases", ManagedJson::array({"legacy.counter"})}}});
    const auto value = Encode(manifest.dump());
    NativeBindingRegistry::Get().Invoke("Managed.Publish", {&value, 1});
    auto source = f.app.scene.CreateEntity("Source"), destination = f.app.scene.CreateEntity("Destination");
    auto target = f.app.scene.CreateEntity("Target");
    auto attach = [](Entity& owner, const char* type) -> ManagedAttachment& {
        auto item = std::make_unique<ManagedAttachment>(); item->type = type; item->token = NextManagedToken();
        auto& data = owner.GetOrAddComponent<ManagedComponents>();data.items.push_back(std::move(item));return *data.items.back();
    };
    auto& from = attach(source, "legacy.counter");
    from.enabled = false;
    from.fields = {{"count", 17}, {"label", "0017"}, {"target", {{"entity", "0"}}}};
    from.references["/target"] = target;
    auto& to = attach(destination, "stable.counter");
    const auto token = to.token;
    ComponentClipboard clipboard; std::string error;
    Check(clipboard.CopyManaged(source, from.token, error), error.c_str());
    from.fields["count"] = 999;
    source.Destroy();
    Check(clipboard.CanPasteManaged(destination, token) && clipboard.PasteManaged(destination, token, error), error.c_str());
    Check(to.token == token && to.type == "stable.counter" && !to.enabled && to.fields["count"] == 17 && to.fields["label"] == "0017" &&
          to.references["/target"] == target, "C# paste changed identity, lost typed fields or failed to restore entity references");
    const auto saved = GetManagedComponentValues(destination, token);
    Check(saved["fields"]["target"]["entity"].as<std::string>() == std::to_string(uint64_t(target.GetUUID())),
          "C# pasted reference did not serialize");
    auto& other = attach(destination, "other.type");
    Check(!clipboard.CanPasteManaged(destination, other.token), "Different C# type accepted paste");
    NativeBindingRegistry::Get().RemoveOwner("Canis.Scene");
}

static void BuiltInValues()
{
    App app;app.scene.app = &app;
    Editor editor;
    app.RegisterDefaults(editor);
    auto source = app.scene.CreateEntity("Source"), destination = app.scene.CreateEntity("Destination");
    auto reference = app.scene.CreateEntity("Reference");
    source.AddComponent<Transform>();destination.AddComponent<Transform>();
    source.AddComponent<UIButton>();
    auto* button = destination.AddComponent<UIButton>();
    button->hitRect = button->up = button->down = button->left = button->right = button->targetEntity = reference;
    ComponentClipboard clipboard;std::string error;
    const auto* conf = app.GetComponentConf(UIButton::ScriptName);
    Check(conf && clipboard.Copy(*conf, source, error) && clipboard.Paste(*conf, destination, error), error.c_str());
    Check(!button->hitRect && !button->up && !button->down && !button->left && !button->right && !button->targetEntity,
          "Pasting unassigned built-in references left old targets attached");
    auto* emitter = source.AddComponent<ParticleEmitter>();
    emitter->spriteBurst = true;emitter->spriteFrameRate = 48;emitter->spriteSize = Vector2(.75f);
    auto* target = destination.AddComponent<ParticleEmitter>();target->elapsed = .7f;target->initialized = true;
    conf = app.GetComponentConf(ParticleEmitter::ScriptName);
    Check(conf && clipboard.Copy(*conf, source, error) && clipboard.Paste(*conf, destination, error), error.c_str());
    Check(target->spriteBurst && target->spriteFrameRate == 48 && target->spriteSize == Vector2(.75f) && target->elapsed == .7f,
          "Emitter settings were not pasted independently of runtime playback state");
}

namespace Canis
{
    struct EditorSceneTabsTestAccess
    {
        static void Undo(Fixture& f, Entity& source, Entity& destination)
        {
            Editor editor;
            editor.m_scene = &f.app.scene; editor.m_app = &f.app;
            const auto id = destination.GetUUID();
            editor.ResetSceneHistory();
            std::string error;
            Check(editor.m_componentClipboard.Copy(tuningConf, source, error), error.c_str());
            Check(editor.m_componentClipboard.Paste(tuningConf, destination, error), error.c_str());
            editor.NotifySceneChanged();
            Check(editor.m_sceneHistoryEditWasActive, "Paste did not opt into scene history");
            editor.FlushSceneHistoryPendingChange();
            Check(editor.CanUndoSceneEdit(), "Paste did not create an Undo entry");
            editor.UndoSceneEdit();
            Check(f.app.scene.FindEntity(id).GetScript<Tuning>()->speed == 2, "Paste Undo failed");
            editor.RedoSceneEdit();
            Check(f.app.scene.FindEntity(id).GetScript<Tuning>()->speed == 9, "Paste Redo failed");
        }
    };
}

int main(int argc, char** argv)
{
    const auto original = std::filesystem::current_path();
    const auto temporary = std::filesystem::temp_directory_path() / ("canis-component-clipboard-" + std::to_string(uint64_t(UUID())));
    try
    {
        std::filesystem::create_directories(temporary);std::filesystem::current_path(temporary);
        const auto project = argc > 1 ? std::filesystem::path(argv[1]) : original;
        std::filesystem::create_directory_symlink(project / "assets", temporary / "assets");
        NativeValues();ComponentsAndHierarchy();ManagedValues();BuiltInValues();
        ImGui::CreateContext();
        Window window("Component clipboard tests", 320, 240, true);
        InputManager input;
        Fixture f;
        f.app.scene.Init(&f.app, &window, &input);
        auto source = f.app.scene.CreateEntity("Source"), destination = f.app.scene.CreateEntity("Destination");
        source.AddScript<Tuning>()->speed = 9; destination.AddScript<Tuning>()->speed = 2;
        EditorSceneTabsTestAccess::Undo(f, source, destination);
        ImGui::DestroyContext();
        std::filesystem::current_path(original);std::filesystem::remove_all(temporary);
        std::cout << "Component clipboard tests passed\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::filesystem::current_path(original);std::filesystem::remove_all(temporary);
        std::cerr << exception.what() << '\n';return 1;
    }
}
