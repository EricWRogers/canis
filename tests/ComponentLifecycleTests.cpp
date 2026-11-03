#include <Canis/ConfigHelper.hpp>
#include <Canis/ComponentLifecycle.hpp>
#include <iostream>
#include <stdexcept>

using namespace Canis;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Dependency
{
    Entity* entity = nullptr;
    bool initialized = false;
    void Create() { initialized = true; }
};
struct SpawnedScript : ScriptableEntity
{
    static inline int calls = 0;
    using ScriptableEntity::ScriptableEntity;
    void Create() override { ++calls; }
};
struct Counter
{
    static constexpr const char* ScriptName = "Lifecycle::Counter";
    static inline int calls = 0;
    static inline std::vector<int> values;
    Entity* entity = nullptr;
    Entity target = nullptr;
    int value = 5, observedTarget = -1;
    Counter() = default;
    explicit Counter(int v) : value(v) {}
    void EnsureDependencies() { entity->AddComponent<Dependency>(); }
    void Create()
    {
        Check(entity && entity->TryGetComponent<Counter>() == this, "Owner was not bound before Create");
        Check(entity->GetComponent<Dependency>().initialized, "Required component was not created first");
        ++calls; values.push_back(value);
        if (target) observedTarget = target.GetComponent<Counter>().value;
        if (value == 27) entity->AddScript<SpawnedScript>();
    }
};
struct NoOwner
{
    static inline int calls = 0;
    bool initialized = false;
    void Create() { initialized = true; ++calls; }
};
struct ScriptCreatesNative : ScriptableEntity
{
    static constexpr const char* ScriptName = "Lifecycle::Script";
    static inline int calls = 0;
    using ScriptableEntity::ScriptableEntity;
    void Create() override
    {
        ++calls;
        Check(entity.GetComponent<Counter>().observedTarget == 9, "Script Create ran before native initialization");
        const int before = NoOwner::calls;
        entity.AddComponent<NoOwner>();
        Check(NoOwner::calls == before + 1, "AddComponent inside script Create was deferred unnecessarily");
    }
};
struct DestroyOwner
{
    Entity* entity = nullptr;
    static inline bool finished = false;
    void Create() { entity->Destroy(); finished = true; }
};
static ComponentConf counterConf;
static ScriptConf scriptConf;
static void Register(App& app)
{
    counterConf = {}; scriptConf = {};
    REGISTER_PROPERTY(counterConf, Counter, value);
    REGISTER_PROPERTY(counterConf, Counter, target);
    DEFAULT_COMPONENT_CONFIG(counterConf, Counter);
    DEFAULT_CONFIG(scriptConf, ScriptCreatesNative);
    app.RegisterComponent(counterConf); app.RegisterScript(scriptConf);
    app.scene.app = &app;
    Counter::calls = 0; Counter::values.clear(); NoOwner::calls = 0;
    SpawnedScript::calls = ScriptCreatesNative::calls = 0;
}
static void DirectAndRegisteredAdds()
{
    App app; Register(app); auto owner = app.scene.CreateEntity("Direct");
    auto* component = owner.AddComponent<Counter>(42);
    Check(component && Counter::calls == 1 && Counter::values.back() == 42, "Direct AddComponent did not Create after construction");
    Check(owner.AddComponent<Counter>() == component && Counter::calls == 1, "Existing AddComponent called Create again");
    owner.GetOrAddComponent<Counter>(); Check(Counter::calls == 1, "GetOrAdd reinitialized a component");
    owner.AddOrReplaceComponent<Counter>(17);
    Check(Counter::calls == 2 && Counter::values.back() == 17, "Replacement component did not Create once");
    auto registered = app.scene.CreateEntity("Registered");
    auto* config = app.GetComponentConf(Counter::ScriptName);
    config->Add(registered); config->Add(registered);
    Check(Counter::calls == 3, "Registered Add skipped or duplicated native Create");
    ComponentConf factory;
    factory.name = "Lifecycle::Factory";
    factory.Add = [](Entity& e) { e.AddComponent<Counter>()->value = 44; };
    app.RegisterComponent(factory);
    auto configured = app.scene.CreateEntity("Editor defaults");
    app.GetComponentConf(factory.name)->Add(configured);
    Check(Counter::calls == 4 && Counter::values.back() == 44, "Create ran before registered attachment finished setting defaults");
    owner.AddComponent<NoOwner>(); Check(NoOwner::calls == 1, "Create requires an entity field unnecessarily");
}
static void DirectDecode()
{
    App app; Register(app);
    auto owner = app.scene.CreateEntity("Decoded"); auto node = YAML::Load("Lifecycle::Counter: {value: 13}");
    app.GetComponentConf(Counter::ScriptName)->Decode(node, owner, true);
    Check(Counter::calls == 1 && Counter::values.back() == 13, "Registered Decode created before saved fields or created twice");
    auto other = app.scene.CreateEntity("Raw config"); node = YAML::Load("Lifecycle::Counter: {value: 16}");
    counterConf.Decode(node, other, true);
    Check(Counter::calls == 2 && Counter::values.back() == 16, "Generic decoder bypassed lifecycle deferral");
    Editor editor; app.RegisterDefaults(editor);
    auto cameraOwner = app.scene.CreateEntity("Authored camera");
    node = YAML::Load("Canis::Camera2D: {position: [12, 34], scale: 2}");
    app.GetComponentConf(Camera2D::ScriptName)->Decode(node, cameraOwner, true);
    const auto& camera = cameraOwner.GetComponent<Camera2D>();
    Check(camera.GetPosition() == Vector2(12, 34) && camera.GetScale() == 2, "Camera Create overwrote saved position/scale");
}
static void SceneAndPrefabLoad()
{
    App app; Register(app);
    auto nodes = YAML::Load(R"(
- Entity: 71
  Name: First
  Lifecycle::Counter: {value: 27, target: 72}
  Lifecycle::Script: {}
- Entity: 72
  Name: Second
  Lifecycle::Counter: {value: 9}
)");
    auto loaded = app.scene.LoadEntityNodes(nodes, false);
    Check(loaded.size() == 2 && Counter::calls == 2, "Scene native Create missing or duplicated");
    Check(loaded[0]->GetComponent<Counter>().observedTarget == 9, "Create did not see forward reference fields");
    Check(loaded[0]->GetComponent<Counter>().target == loaded[1], "Prefab reference remap was not ready before Create");
    Check(SpawnedScript::calls == 1 && ScriptCreatesNative::calls == 1, "Script created by native Create was initialized twice");
    auto duplicate = app.scene.LoadEntityNodes(nodes, false);
    Check(Counter::calls == 4 && duplicate[0]->GetComponent<Counter>().target == duplicate[1], "Prefab clone did not initialize independently");
    auto direct = YAML::Load("Entity: 73\nLifecycle::Counter: {value: 33}\n");
    app.scene.DecodeEntity(direct);
    Check(Counter::calls == 5 && Counter::values.back() == 33, "Direct DecodeEntity missed native Create");
}
static void DeferredChangesAndFailures()
{
    App app; Register(app);
    auto owner = app.scene.CreateEntity("Deferred");
    {
        ComponentCreateScope scope(app.scene);
        owner.AddComponent<Counter>(1);
        Check(Counter::calls == 0, "Deferred component ran too early");
        owner.AddOrReplaceComponent<Counter>(2);
    }
    Check(Counter::calls == 1 && Counter::values.back() == 2, "Replacement left a stale pending Create");
    {
        ComponentCreateScope scope(app.scene);
        auto removed = app.scene.CreateEntity("Removed");
        removed.AddComponent<Counter>(); removed.RemoveComponent<Counter>();
        auto destroyed = app.scene.CreateEntity("Destroyed");
        destroyed.AddComponent<Counter>(); destroyed.Destroy();
    }
    Check(Counter::calls == 1, "Removed or destroyed component received Create");
    ComponentConf failingConf;
    failingConf.name = "Lifecycle::FailingDecoder";
    failingConf.Decode = [](YAML::Node&, Entity& owner, bool) {
        owner.AddComponent<Counter>();
        throw std::runtime_error("Decoder failed after attaching its component");
    };
    app.RegisterComponent(failingConf);
    auto failed = app.scene.CreateEntity("Bad input"); bool threw = false;
    try {
        YAML::Node node;
        app.GetComponentConf(failingConf.name)->Decode(node, failed, true);
    } catch (const std::runtime_error&) { threw = true; }
    { ComponentCreateScope scope(app.scene); }
    Check(threw && Counter::calls == 1, "Failed decode retained a pending initializer");
    auto dead = app.scene.CreateEntity("Self destroy");
    Check(dead.AddComponent<DestroyOwner>() == nullptr && !dead && DestroyOwner::finished, "Create did not safely finish after destroying its owner");
}
int main()
{
    try { DirectAndRegisteredAdds(); DirectDecode(); SceneAndPrefabLoad(); DeferredChangesAndFailures();
          std::cout << "Native component lifecycle checks passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
