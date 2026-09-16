#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scripting/SceneBindings.hpp>
#include <Canis/Scripting/NativeComponentFields.generated.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/App.hpp>
#include <Canis/Canis.hpp>
#include <Canis/VFX/Particles.hpp>
#include <Canis/Terrain.hpp>
#include <Canis/Blockout.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/AudioManager.hpp>
#include <Canis/VR/VRSystem.hpp>
#include <SDL3/SDL.h>
#include <cmath>
#include <unordered_map>

namespace Canis::Scripting
{
namespace
{
    uint64_t nextEntityHandle = 1;
    struct Context
    {
        App& app;
        uint64_t epoch = 0;
        std::unordered_map<uint64_t, Entity> handles;
        std::unordered_map<uint32_t, uint64_t> identities;
        uint64_t nextComponent = 1;
        std::map<std::pair<uint32_t,std::string>,uint64_t> componentTokens;
        ComponentConf& Configuration(const std::string& type) {
            const auto name = type.find("::") == std::string::npos ? "Canis::" + type : type;
            for (auto& conf : app.GetComponentRegistry())
                if (conf.name == name && conf.kind == RegistryEntryKind::Component) return conf;
            throw std::invalid_argument("Unknown native component: " + name);
        }
        template<class T> void Removed(entt::registry&, entt::entity entity) {
            componentTokens.erase({static_cast<uint32_t>(entity), T::ScriptName});
        }
        explicit Context(App& app) : app(app) {
            auto& r = app.scene.GetRegistry();
            r.on_destroy<AnimationPlayer>().connect<&Context::Removed<AnimationPlayer>>(this);
            r.on_destroy<Animator>().connect<&Context::Removed<Animator>>(this);
            r.on_destroy<BlockoutShape>().connect<&Context::Removed<BlockoutShape>>(this);
            r.on_destroy<BoneAttachment>().connect<&Context::Removed<BoneAttachment>>(this);
            r.on_destroy<BoxCollider>().connect<&Context::Removed<BoxCollider>>(this);
            r.on_destroy<Camera>().connect<&Context::Removed<Camera>>(this);
            r.on_destroy<Camera2D>().connect<&Context::Removed<Camera2D>>(this);
            r.on_destroy<Canvas>().connect<&Context::Removed<Canvas>>(this);
            r.on_destroy<CapsuleCollider>().connect<&Context::Removed<CapsuleCollider>>(this);
            r.on_destroy<CloudNavSurface>().connect<&Context::Removed<CloudNavSurface>>(this);
            r.on_destroy<ConvexMeshCollider>().connect<&Context::Removed<ConvexMeshCollider>>(this);
            r.on_destroy<DirectionalLight>().connect<&Context::Removed<DirectionalLight>>(this);
            r.on_destroy<Material>().connect<&Context::Removed<Material>>(this);
            r.on_destroy<MeshCollider>().connect<&Context::Removed<MeshCollider>>(this);
            r.on_destroy<Model>().connect<&Context::Removed<Model>>(this);
            r.on_destroy<ModelAnimation>().connect<&Context::Removed<ModelAnimation>>(this);
            r.on_destroy<NavMeshSurface>().connect<&Context::Removed<NavMeshSurface>>(this);
            r.on_destroy<NetworkIdentity>().connect<&Context::Removed<NetworkIdentity>>(this);
            r.on_destroy<ParticleEmitter>().connect<&Context::Removed<ParticleEmitter>>(this);
            r.on_destroy<PointLight>().connect<&Context::Removed<PointLight>>(this);
            r.on_destroy<PrefabInstance>().connect<&Context::Removed<PrefabInstance>>(this);
            r.on_destroy<RectTransform>().connect<&Context::Removed<RectTransform>>(this);
            r.on_destroy<Rigidbody>().connect<&Context::Removed<Rigidbody>>(this);
            r.on_destroy<SphereCollider>().connect<&Context::Removed<SphereCollider>>(this);
            r.on_destroy<Sprite2D>().connect<&Context::Removed<Sprite2D>>(this);
            r.on_destroy<SpriteAnimation>().connect<&Context::Removed<SpriteAnimation>>(this);
            r.on_destroy<Terrain>().connect<&Context::Removed<Terrain>>(this);
            r.on_destroy<Text>().connect<&Context::Removed<Text>>(this);
            r.on_destroy<Transform>().connect<&Context::Removed<Transform>>(this);
            r.on_destroy<UIButton>().connect<&Context::Removed<UIButton>>(this);
            r.on_destroy<UIDragSource>().connect<&Context::Removed<UIDragSource>>(this);
            r.on_destroy<UIDropTarget>().connect<&Context::Removed<UIDropTarget>>(this);
            r.on_destroy<UIInputField>().connect<&Context::Removed<UIInputField>>(this);
        }
        ~Context() {
            auto& r = app.scene.GetRegistry();
            r.on_destroy<AnimationPlayer>().disconnect(this);
            r.on_destroy<Animator>().disconnect(this);
            r.on_destroy<BlockoutShape>().disconnect(this);
            r.on_destroy<BoneAttachment>().disconnect(this);
            r.on_destroy<BoxCollider>().disconnect(this);
            r.on_destroy<Camera>().disconnect(this);
            r.on_destroy<Camera2D>().disconnect(this);
            r.on_destroy<Canvas>().disconnect(this);
            r.on_destroy<CapsuleCollider>().disconnect(this);
            r.on_destroy<CloudNavSurface>().disconnect(this);
            r.on_destroy<ConvexMeshCollider>().disconnect(this);
            r.on_destroy<DirectionalLight>().disconnect(this);
            r.on_destroy<Material>().disconnect(this);
            r.on_destroy<MeshCollider>().disconnect(this);
            r.on_destroy<Model>().disconnect(this);
            r.on_destroy<ModelAnimation>().disconnect(this);
            r.on_destroy<NavMeshSurface>().disconnect(this);
            r.on_destroy<NetworkIdentity>().disconnect(this);
            r.on_destroy<ParticleEmitter>().disconnect(this);
            r.on_destroy<PointLight>().disconnect(this);
            r.on_destroy<PrefabInstance>().disconnect(this);
            r.on_destroy<RectTransform>().disconnect(this);
            r.on_destroy<Rigidbody>().disconnect(this);
            r.on_destroy<SphereCollider>().disconnect(this);
            r.on_destroy<Sprite2D>().disconnect(this);
            r.on_destroy<SpriteAnimation>().disconnect(this);
            r.on_destroy<Terrain>().disconnect(this);
            r.on_destroy<Text>().disconnect(this);
            r.on_destroy<Transform>().disconnect(this);
            r.on_destroy<UIButton>().disconnect(this);
            r.on_destroy<UIDragSource>().disconnect(this);
            r.on_destroy<UIDropTarget>().disconnect(this);
            r.on_destroy<UIInputField>().disconnect(this);
        }
        uint64_t Token(uint64_t id, const std::string& type) {
            auto e = Resolve(id);
            auto& conf = Configuration(type);
            auto key = std::make_pair(static_cast<uint32_t>(e.GetHandle()), conf.name);
            if (!conf.Has || !conf.Has(e)) { componentTokens.erase(key); return 0; }
            auto [it, inserted] = componentTokens.try_emplace(key, nextComponent);
            if (inserted) ++nextComponent;
            return it->second;
        }
        void Refresh()
        {
            if (epoch != app.scene.ScriptingEpoch())
            { handles.clear(); identities.clear(); componentTokens.clear(); epoch = app.scene.ScriptingEpoch(); }
        }
        uint64_t Handle(Entity* entity)
        {
            Refresh(); if (!entity || !entity->IsValid()) return 0;
            const auto identity = static_cast<uint32_t>(entity->GetHandle());
            if (auto it = identities.find(identity); it != identities.end()) return it->second;
            const auto id = nextEntityHandle++; handles.emplace(id, *entity); identities[identity] = id; return id;
        }
        Entity Resolve(uint64_t handle)
        {
            Refresh();
            const auto it = handles.find(handle);
            if (it == handles.end() || !it->second.IsValid()) throw std::runtime_error("Entity has been destroyed or belongs to an unloaded scene");
            return it->second;
        }
        template<class T> T& Component(uint64_t handle)
        {
            auto entity = Resolve(handle);
            auto* component = entity.TryGetComponent<T>();
            if (!component) throw std::runtime_error("Required native component is missing");
            return *component;
        }
    };
    ManagedJson NativeFieldsJson(const YAML::Node& node) {
        if (!node || node.IsNull()) return nullptr;
        if (node.IsSequence()) { auto out=ManagedJson::array(); for(auto x:node) out.push_back(NativeFieldsJson(x)); return out; }
        if (node.IsMap()) { auto out=ManagedJson::object(); for(auto x:node) out[x.first.as<std::string>()]=NativeFieldsJson(x.second); return out; }
        if (node.Tag()=="!") return node.Scalar();
        try { return ManagedJson::parse(node.Scalar()); } catch (...) { return node.Scalar(); }
    }
    void Finite(Vector3 value)
    { if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) throw std::invalid_argument("Non-finite vector"); }
    VR::System& Runtime(App& app)
    { if (!app.GetVR()) throw std::runtime_error("VR runtime is not active"); return *app.GetVR(); }
    VR::Hand Hand(App& app, int index)
    { if (index < 0 || index > 1) throw std::invalid_argument("Hand must be 0 (left) or 1 (right)"); return Runtime(app).GetState().hands[index]; }
}
void RegisterSceneBindings(App& app)
{
    auto& bindings = NativeBindingRegistry::Get();
    bindings.RemoveOwner("Canis.Scene");
    auto context = std::make_shared<Context>(app);
    RegisterManagedBindings(app, [context](Entity* e){return context->Handle(e);}, [context](uint64_t id){return context->Resolve(id);});
    constexpr auto owner = "Canis.Scene";
    bindings.Method(owner,"Component","Token",[context](uint64_t id,std::string type)->uint64_t{return context->Token(id,type);});
    bindings.Method(owner,"Component","Types",[context]()->std::string {
        auto types = ManagedJson::array();
        for (auto& conf : context->app.GetComponentRegistry())
            if (conf.kind == RegistryEntryKind::Component) types.push_back(conf.name);
        return types.dump();
    });
    bindings.Method(owner,"Component","Add",[context](uint64_t id,std::string type)->uint64_t {
        auto e = context->Resolve(id); auto& conf = context->Configuration(type);
        if (!conf.Add) throw std::invalid_argument("Component cannot be added: " + type);
        if (conf.Has(e)) throw std::invalid_argument("Component already attached: " + type);
        conf.Add(e); return context->Token(id,type);
    });
    bindings.Method(owner,"Component","Remove",[context](uint64_t id,std::string type) {
        auto e = context->Resolve(id); auto& conf = context->Configuration(type);
        if (!conf.Remove) throw std::invalid_argument("Component cannot be removed: " + type);
        conf.Remove(e);
        context->componentTokens.erase({static_cast<uint32_t>(e.GetHandle()),conf.name});
    });
    bindings.Method(owner,"Component","Read",[context](uint64_t id,std::string type)->std::string {
        auto e = context->Resolve(id); auto& conf = context->Configuration(type);
        if (!conf.Has(e) || !conf.Encode) throw std::invalid_argument("Component is missing or has no serializer");
        YAML::Node node; conf.Encode(node,e);
        auto fields = NativeFieldsJson(node[conf.name]);
        if (auto strings=NativeStringFields.find(conf.name); strings!=NativeStringFields.end())
            for (const auto& key:strings->second) if (node[conf.name][key]) fields[key]=node[conf.name][key].as<std::string>();
        return fields.dump();
    });
    bindings.Method(owner,"Component","Write",[context](uint64_t id,std::string type,std::string json) {
        auto e = context->Resolve(id); auto& conf = context->Configuration(type);
        if (!conf.Has(e) || !conf.Encode || !conf.Decode) throw std::invalid_argument("Component is missing or has no serializer");
        auto patch = ManagedJson::parse(json);
        if (!patch.is_object()) throw std::invalid_argument("Component fields must be an object");
        YAML::Node original; conf.Encode(original,e);
        auto node = YAML::Clone(original);
        for (auto& [key,value] : patch.items()) {
            const auto schema = NativeSerializedFields.find(conf.name);
            const bool known = node[conf.name][key].IsDefined() || conf.registry.setters.contains(key) ||
                (schema != NativeSerializedFields.end() && schema->second.contains(key));
            if (!known) throw std::invalid_argument("Unknown serialized field: " + key);
            if (node[conf.name][key].IsDefined()) {
                auto current = NativeFieldsJson(node[conf.name][key]);
                if (auto strings=NativeStringFields.find(conf.name); strings!=NativeStringFields.end() && strings->second.contains(key))
                    current=node[conf.name][key].as<std::string>();
                const bool compatible = current.is_null() ||
                    (current.is_number() && value.is_number()) || current.type() == value.type();
                if (!compatible) throw std::invalid_argument("Wrong value type for serialized field: " + key);
                const auto vectors = NativeVectorFields.find(conf.name);
                if (current.is_array() && vectors != NativeVectorFields.end() && vectors->second.contains(key)) {
                    if (value.size() != current.size() ||
                        !std::all_of(value.begin(),value.end(),[](const auto& item){return item.is_number();}))
                        throw std::invalid_argument("Wrong vector size or element type: " + key);
                }
            }
            node[conf.name][key] = YAML::Load(value.dump());
        }
        // Decode a complete configuration so unrelated authored values are retained.
        // Do not run Create again on an already attached component.
        try { conf.Decode(node,e,false); }
        catch (...) { conf.Decode(original,e,false); throw; }
    });
    bindings.Method(owner,"Presentation","Color",[context](uint64_t id)->Vector4{return context->Component<Model>(id).color;});
    bindings.Method(owner,"Presentation","Text",[context](uint64_t id)->std::string{return context->Component<Text>(id).text;});
    bindings.Method(owner, "Scene", "Find", [context](std::string name) -> uint64_t { return context->Handle(context->app.scene.FindEntityWithName(name)); });
    bindings.Method(owner, "Scene", "IsValid", [context](uint64_t id) -> bool { try { return context->Resolve(id).IsValid(); } catch (...) { return false; } });
    bindings.Method(owner, "Scene", "Name", [context](uint64_t id) -> std::string { return context->Resolve(id).GetName(); });
    bindings.Method(owner, "Scene", "SetActive", [context](uint64_t id, bool active) { context->Resolve(id).SetActive(active); });
    bindings.Method(owner, "Scene", "Active", [context](uint64_t id) -> bool { return context->Resolve(id).IsActive(); });
    bindings.Method(owner, "Transform", "Position", [context](uint64_t id) -> Vector3 { return context->Component<Transform>(id).GetGlobalPosition(); });
    bindings.Method(owner, "Transform", "SetPosition", [context](uint64_t id, Vector3 position)
    {
        Finite(position); auto& t = context->Component<Transform>(id);
        Matrix4 parent(1);
        if (auto* p = t.parent.TryGetComponent<Transform>()) parent = p->GetModelMatrix();
        if (t.useLocalMatrixPrefix) parent *= t.localMatrixPrefix;
        if (std::abs(glm::determinant(parent)) < 1e-8f) throw std::runtime_error("Singular parent transform");
        t.position = Vector3(glm::inverse(parent) * Vector4(position, 1));
    });
    bindings.Method(owner, "Transform", "Rotation", [context](uint64_t id) -> Quaternion { return context->Component<Transform>(id).GetGlobalRotation(); });
    bindings.Method(owner, "Transform", "SetRotation", [context](uint64_t id, Quaternion rotation)
    {
        if (!std::isfinite(glm::length(rotation)) || glm::length(rotation) < 1e-6f) throw std::invalid_argument("Invalid quaternion");
        auto& t = context->Component<Transform>(id);
        if (auto* p = t.parent.TryGetComponent<Transform>()) rotation = glm::inverse(p->GetGlobalRotation()) * rotation;
        if (t.useLocalMatrixPrefix) throw std::runtime_error("Bone-space rotation writes are not supported by this binding");
        t.rotation = glm::normalize(rotation);
    });
    bindings.Method(owner, "Transform", "Scale", [context](uint64_t id) -> Vector3 { return context->Component<Transform>(id).scale; });
    bindings.Method(owner, "Transform", "SetScale", [context](uint64_t id, Vector3 scale)
    { Finite(scale); context->Component<Transform>(id).scale = scale; });
    bindings.Method(owner, "Presentation", "SetColor", [context](uint64_t id, Vector4 color) { context->Component<Model>(id).color = color; });
    bindings.Method(owner, "Presentation", "SetText", [context](uint64_t id, std::string text) { context->Component<Text>(id).SetText(text); });
    bindings.Method(owner, "Presentation", "PlaySound", [](std::string path, float volume) { AudioManager::PlaySFX(path, std::clamp(volume, 0.f, 1.f)); });
    bindings.Method(owner, "Input", "Key", [&app](std::string name) -> bool
    {
        const auto key = SDL_GetScancodeFromName(name.c_str());
        if (key == SDL_SCANCODE_UNKNOWN) throw std::invalid_argument("Unknown key: " + name);
        return app.scene.GetInputManager().GetKey(key);
    });
    bindings.Method(owner, "Input", "MouseDelta", [&app]() -> Vector3 { const auto& input=app.scene.GetInputManager(); return input.active ? Vector3(input.mouseRel,0) : Vector3(0); });
    bindings.Method(owner, "Input", "RightMouse", [&app]() -> bool { return app.scene.GetInputManager().GetRightClick(); });
    bindings.Method(owner, "Input", "LeftMouse", [&app]() -> bool { return app.scene.GetInputManager().GetLeftClick(); });
    bindings.Method(owner, "VR", "Available", [&app]() -> bool { return app.GetVR() != nullptr; });
    bindings.Method(owner, "VR", "Simulated", [&app]() -> bool { return app.GetVR() && app.GetVR()->IsSimulated(); });
    bindings.Method(owner, "VR", "Tracked", [&app](int index) -> bool
    {
        const auto hand = Hand(app, index); const auto& state = Runtime(app).GetState();
        return state.focused && state.shouldRender && hand.active && hand.grip.valid;
    });
    bindings.Method(owner, "VR", "Position", [&app](int index) -> Vector3 { return Runtime(app).WorldPose(Hand(app,index).grip).position; });
    bindings.Method(owner, "VR", "Rotation", [&app](int index) -> Quaternion { return Runtime(app).WorldPose(Hand(app,index).grip).orientation; });
    bindings.Method(owner, "VR", "Squeeze", [&app](int index) -> float { return Hand(app,index).squeeze; });
    bindings.Method(owner, "VR", "Trigger", [&app](int index) -> float { return Hand(app,index).trigger; });
    bindings.Method(owner, "VR", "Haptic", [&app](int index, float amplitude, float seconds) -> bool
    {
        if (index < 0 || index > 1 || !std::isfinite(amplitude) || !std::isfinite(seconds)) throw std::invalid_argument("Invalid haptic arguments");
        return Runtime(app).Haptic(index, std::clamp(amplitude,0.f,1.f), std::clamp(seconds,0.f,.3f));
    });
    // Explicit simulation hook for deterministic tests; never changes real headset input.
    bindings.Method(owner, "VR", "SimulateHands", [&app](Vector3 left, Vector3 right, float leftGrip, float rightGrip)
    {
        auto& vr = Runtime(app); if (!vr.IsSimulated()) throw std::runtime_error("Simulation cannot override real XR input");
        Finite(left); Finite(right);
        auto state = vr.GetState();
        const auto inverse = glm::inverse(vr.GetOrigin());
        for (int index = 0; index < 2; ++index)
        {
            auto& hand = state.hands[index]; hand.active = true;
            hand.grip = {Vector3(inverse * Vector4(index ? right : left,1)), Quaternion(1,0,0,0), true};
            hand.aim = hand.grip; hand.squeeze = index ? rightGrip : leftGrip;
        }
        vr.SetSimulatedInput(state.head, state.hands, true);
    });
    bindings.Method(owner, "Diagnostics", "Fail", [&app](std::string message) { app.FailRuntimeTest(message); });
}
void UnregisterSceneBindings() { NativeBindingRegistry::Get().RemoveOwner("Canis.Scene"); }
}
