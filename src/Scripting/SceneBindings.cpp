#include <Canis/Time.hpp>
#include <Canis/PlayerPrefs.hpp>
#include <Canis/Scripting/TweenBindings.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scripting/SceneBindings.hpp>
#include <Canis/Scripting/NativeComponentFields.generated.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Canis.hpp>
#include <Canis/VFX/Particles.hpp>
#include <Canis/VFX/Trails.hpp>
#include <Canis/Terrain.hpp>
#include <Canis/Blockout.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/Scripting/InputBindings.hpp>
#include <Canis/AudioManager.hpp>
#include <Canis/Audio.hpp>
#include <Canis/VR/VRSystem.hpp>
#include <Canis/ConfigData.hpp>
#include <Canis/Window.hpp>
#include <Canis/ECS/Systems/UIInteractionSystem.hpp>
#include <Canis/ECS/Systems/NavMeshSystem.hpp>
#include <unordered_set>
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
            r.on_destroy<AudioSource>().connect<&Context::Removed<AudioSource>>(this);
            r.on_destroy<AudioListener>().connect<&Context::Removed<AudioListener>>(this);
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
            r.on_destroy<TrailRenderer>().connect<&Context::Removed<TrailRenderer>>(this);
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
            r.on_destroy<AudioSource>().disconnect(this);
            r.on_destroy<AudioListener>().disconnect(this);
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
            r.on_destroy<TrailRenderer>().disconnect(this);
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
    RegisterTweenBindings(app, [context](uint64_t id){return context->Resolve(id);});
    RegisterInputBindings(app);
    constexpr auto owner = "Canis.Scene";
    bindings.Method(owner, "PlayerPrefs", "HasKey", [](std::string key) { return PlayerPrefs::HasKey(key); });
    bindings.Method(owner, "PlayerPrefs", "GetString", [](std::string key, std::string fallback) { return PlayerPrefs::GetString(key, fallback); });
    bindings.Method(owner, "PlayerPrefs", "SetString", [](std::string key, std::string value) { PlayerPrefs::SetString(key, value); });
    bindings.Method(owner, "PlayerPrefs", "GetInt", [](std::string key, int fallback) { return PlayerPrefs::GetInt(key, fallback); });
    bindings.Method(owner, "PlayerPrefs", "SetInt", [](std::string key, int value) { PlayerPrefs::SetInt(key, value); });
    bindings.Method(owner, "PlayerPrefs", "GetFloat", [](std::string key, float fallback) { return PlayerPrefs::GetFloat(key, fallback); });
    bindings.Method(owner, "PlayerPrefs", "SetFloat", [](std::string key, float value) { PlayerPrefs::SetFloat(key, value); });
    bindings.Method(owner, "PlayerPrefs", "GetBool", [](std::string key, bool fallback) { return PlayerPrefs::GetBool(key, fallback); });
    bindings.Method(owner, "PlayerPrefs", "SetBool", [](std::string key, bool value) { PlayerPrefs::SetBool(key, value); });
    bindings.Method(owner, "PlayerPrefs", "DeleteKey", [](std::string key) { PlayerPrefs::DeleteKey(key); });
    bindings.Method(owner, "PlayerPrefs", "DeleteAll", []() { PlayerPrefs::DeleteAll(); });
    bindings.Method(owner, "PlayerPrefs", "Save", []() { PlayerPrefs::SaveToFile(); });

    bindings.Method(owner, "Application", "FrameLimit", []() -> float { return Time::GetTargetFPS(); });
    bindings.Method(owner, "Application", "SetFrameLimit", [](float fps) {
        if(!std::isfinite(fps)||fps<0||fps>100000)throw std::invalid_argument("Frame limit must be finite and between 0 and 100000");
        Time::SetTargetFPS(fps);
    });
    bindings.Method(owner, "Application", "Quit", [&app]() { app.scene.QuitGame(); });
    bindings.Method(owner, "Window", "Fullscreen", [&app]() -> bool {
        return (SDL_GetWindowFlags(static_cast<SDL_Window*>(app.scene.GetWindow().GetSDLWindow())) & SDL_WINDOW_FULLSCREEN) != 0;
    });
    bindings.Method(owner, "Window", "SetFullscreen", [&app](bool fullscreen) {
        auto* window = static_cast<SDL_Window*>(app.scene.GetWindow().GetSDLWindow());
        if (((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0) != fullscreen && !SDL_SetWindowFullscreen(window, fullscreen))
            throw std::runtime_error(SDL_GetError());
    });
    bindings.Method(owner, "Window", "RenderSize", [&app]() -> Vector3 {
        return {static_cast<float>(app.scene.GetWindow().GetScreenWidth()), static_cast<float>(app.scene.GetWindow().GetScreenHeight()), 0};
    });
    bindings.Method(owner, "Input", "CaptureMouse", [&app](bool capture) {
        auto& window = app.scene.GetWindow();
        if (window.IsMouseLocked() != capture)
            window.LockMouse(capture);
    });
    bindings.Method(owner, "Input", "MouseCaptureRequested", [&app]() -> bool { return app.scene.GetWindow().IsMouseLocked(); });
    bindings.Method(owner, "Input", "MouseCaptured", [&app]() -> bool {
        return app.scene.GetWindow().IsMouseLocked() && SDL_GetWindowRelativeMouseMode(static_cast<SDL_Window*>(app.scene.GetWindow().GetSDLWindow()));
    });
    bindings.Method(owner, "Clipboard", "SetText", [](std::string text) {
        if (!SDL_SetClipboardText(text.c_str()))
            throw std::runtime_error(SDL_GetError());
    });
    bindings.Method(owner, "Clipboard", "Text", []() -> std::string {
        char* value = SDL_GetClipboardText();
        if (value == nullptr)
            throw std::runtime_error(SDL_GetError());
        std::string text(value);
        SDL_free(value);
        return text;
    });
    bindings.Method(owner, "Audio", "MasterVolume", []() -> float { return GetProjectConfig().volume; });
    bindings.Method(owner, "Audio", "SetMasterVolume", [](float volume) {
        if (!std::isfinite(volume))
            throw std::invalid_argument("Volume must be finite");
        GetProjectConfig().volume = std::clamp(volume, 0.f, 1.f);
        AudioManager::RefreshMixLevels();
    });
    bindings.Method(owner, "Animator", "SetFloat", [context](uint64_t id, std::string name, float value) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Animator parameter must be finite");
        context->Component<Animator>(id).SetFloat(name, value);
    });
    bindings.Method(owner, "Navigation", "FindPath", [context,&app](uint64_t id, Vector3 start, Vector3 destination) -> std::string {
        Finite(start); Finite(destination);
        auto surface = context->Resolve(id);
        auto* navigation = app.scene.GetSystem<NavMeshSystem>();
        if (!navigation || !surface.HasComponent<NavMeshSurface>())
            throw std::invalid_argument("Navigation requires a NavMeshSurface and active NavMeshSystem");
        auto points = ManagedJson::array();
        for (const auto& point : navigation->FindPath(app.scene.GetRegistry(), surface, start, destination))
            points.push_back({point.x, point.y, point.z});
        return points.dump();
    });
    bindings.Method(owner, "Animator", "GetFloat", [context](uint64_t id, std::string name) -> float {
        return context->Component<Animator>(id).GetFloat(name);
    });
    bindings.Method(owner, "Animator", "CurrentState", [context](uint64_t id) -> std::string {
        return context->Component<Animator>(id).currentState;
    });
    bindings.Method(owner, "Scene", "Descendants", [context](uint64_t id, bool includeSelf) -> std::string {
        auto result = ManagedJson::array();
        std::unordered_set<uint32_t> visited;
        auto visit = [&](auto&& self, Entity entity, bool include) -> void {
            if (!entity || !visited.insert(static_cast<uint32_t>(entity.GetHandle())).second)
                return;
            if (include)
                result.push_back(context->Handle(entity.TryGet()));
            if (auto* transform = entity.TryGetComponent<Transform>())
                for (auto child : transform->children)
                    self(self, child, true);
            if (auto* rect = entity.TryGetComponent<RectTransform>())
                for (auto child : rect->children)
                    self(self, child, true);
        };
        visit(visit, context->Resolve(id), includeSelf);
        return result.dump();
    });
    // Procedural animation uses the same per-entity pose cache as clip playback.
    bindings.Method(owner, "Skeleton", "Sample", [context](uint64_t id, float advance) -> std::string {
        if (!std::isfinite(advance) || advance < 0 || advance > .25f)
            throw std::invalid_argument("Invalid skeleton sample interval");
        auto& renderer = context->Component<Model>(id);
        auto& animation = context->Component<ModelAnimation>(id);
        auto* model = AssetManager::GetModel(renderer.modelId);
        if (!model) throw std::invalid_argument("Skeleton requires a loaded model");
        if (advance > 0 && animation.playAnimation)
            animation.animationTime += advance * animation.animationSpeed;
        float duration = model->GetAnimationDuration(animation.animationIndex);
        if (animation.loop && duration > 0) animation.animationTime = std::fmod(animation.animationTime,duration);
        ModelAsset::Pose3D pose;
        model->ResetPose(pose);
        model->UpdateAnimation(pose, animation.animationIndex, animation.animationTime, animation.rootMotionMask);
        auto world = context->Component<Transform>(id).GetModelMatrix();
        auto result = ManagedJson::array();
        for (int i = 0; i < model->GetNodeCount(); ++i) {
            Matrix4 matrix = world * pose.globalNodeMatrices[i];
            glm::mat3 basis(matrix);
            for (int column = 0; column < 3; ++column) {
                float length = glm::length(basis[column]);
                if (length < .000001f) throw std::invalid_argument("Skeleton has zero scale");
                basis[column] /= length;
            }
            if (glm::determinant(basis) < 0) basis[0] = -basis[0];
            auto rotation = glm::normalize(glm::quat_cast(basis));
            result.push_back({{"Name",model->GetNodeName(i)}, {"Parent",model->GetNode(i)->parent},
                {"Position",{matrix[3].x,matrix[3].y,matrix[3].z}},
                {"Rotation",{rotation.x,rotation.y,rotation.z,rotation.w}}});
        }
        // Store the sampled locals as the base for a sparse world-pose override.
        animation.proceduralBaseLocalNodeMatrices = pose.localNodeMatrices;
        animation.proceduralBaseModelId = renderer.modelId;
        animation.proceduralBaseGeometryRevision = model->GetGeometryRevision();
        animation.externalLocalNodeMatrices = std::move(pose.localNodeMatrices);
        return result.dump();
    });
    bindings.Method(owner, "Skeleton", "Apply", [context](uint64_t id, std::string payload) {
        auto& animation = context->Component<ModelAnimation>(id);
        auto* model = AssetManager::GetModel(context->Component<Model>(id).modelId);
        if (!model) throw std::invalid_argument("Skeleton requires a loaded model");
        auto values = ManagedJson::parse(payload);
        if (!values.is_array() || values.size() > static_cast<size_t>(model->GetNodeCount()))
            throw std::invalid_argument("Invalid skeleton pose");
        ModelAsset::Pose3D pose;
        model->ResetPose(pose);
        if (animation.proceduralBaseModelId == context->Component<Model>(id).modelId &&
            animation.proceduralBaseGeometryRevision == model->GetGeometryRevision() &&
            animation.proceduralBaseLocalNodeMatrices.size() == pose.localNodeMatrices.size())
            model->BlendPoseFromLocalMatrices(pose, animation.proceduralBaseLocalNodeMatrices, 0);
        Matrix4 world = context->Component<Transform>(id).GetModelMatrix();
        std::map<int, Matrix4> overrides;
        for (const auto& value : values) {
            int index = value.at("Index").get<int>();
            if (index < 0 || index >= model->GetNodeCount() || overrides.count(index))
                throw std::invalid_argument("Invalid or duplicate bone index");
            const auto& p = value.at("Position"); const auto& q = value.at("Rotation");
            Vector3 position(p.at(0).get<float>(),p.at(1).get<float>(),p.at(2).get<float>());
            Quaternion rotation(q.at(3).get<float>(),q.at(0).get<float>(),q.at(1).get<float>(),q.at(2).get<float>());
            Finite(position);
            if (!std::isfinite(rotation.x) || !std::isfinite(rotation.y) || !std::isfinite(rotation.z) ||
                !std::isfinite(rotation.w) || glm::length(rotation) < .000001f)
                throw std::invalid_argument("Invalid bone rotation");
            Matrix4 original = world * pose.globalNodeMatrices[index];
            Vector3 scale(glm::length(Vector3(original[0])),glm::length(Vector3(original[1])),glm::length(Vector3(original[2])));
            if (glm::determinant(glm::mat3(original)) < 0) scale.x = -scale.x;
            overrides[index] = glm::translate(Matrix4(1),position) * glm::mat4_cast(glm::normalize(rotation)) * glm::scale(Matrix4(1),scale);
        }
        // Imported bone indices need not be ordered parent-first.
        auto locals = pose.localNodeMatrices;
        auto visit = [&](auto&& self, int index, const Matrix4& parent) -> void {
            Matrix4 global = parent * locals[index];
            if (auto found = overrides.find(index); found != overrides.end()) {
                global = found->second;
                locals[index] = glm::inverse(parent) * global;
            }
            for (int child : model->GetNode(index)->children) self(self, child, global);
        };
        for (int root : model->GetSceneRoots()) visit(visit, root, world);
        animation.externalLocalNodeMatrices = std::move(locals);
    });
    bindings.Method(owner, "Skeleton", "Clear", [context](uint64_t id) {
        auto& animation = context->Component<ModelAnimation>(id);
        animation.externalLocalNodeMatrices.clear();
        animation.proceduralBaseLocalNodeMatrices.clear();
        animation.proceduralBaseModelId = -1;
        animation.proceduralBaseGeometryRevision = 0;
        animation.poseInitialized = false;
        animation.lastEvaluatedAnimationIndex = -1;
    });
    bindings.Method(owner, "Physics", "Raycast", [context, &app](Vector3 origin, Vector3 direction, float distance, uint64_t ignoreRoot) -> std::string {
        Finite(origin);
        Finite(direction);
        if (!std::isfinite(distance) || distance <= 0 || glm::length(direction) < .000001f)
            throw std::invalid_argument("Raycast requires positive distance and a nonzero direction");
        Entity ignoredRoot = ignoreRoot ? context->Resolve(ignoreRoot) : Entity{};
        for (const auto& hit : app.scene.RaycastAll(origin, glm::normalize(direction), distance))
        {
            bool ignored = false;
            std::unordered_set<uint32_t> visited;
            for (Entity node = hit.entity; node && visited.insert(static_cast<uint32_t>(node.GetHandle())).second;)
            {
                if (node == ignoredRoot)
                {
                    ignored = true;
                    break;
                }
                auto* transform = node.TryGetComponent<Transform>();
                node = transform ? transform->parent : Entity{};
            }
            if (!ignored)
                return ManagedJson({{"entity", context->Handle(hit.entity)}, {"point", {hit.point.x, hit.point.y, hit.point.z}},
                    {"normal", {hit.normal.x, hit.normal.y, hit.normal.z}}, {"distance", hit.distance}}).dump();
        }
        return "null";
    });
    bindings.Method(owner, "Canvas", "Pointer", [context, &app](uint64_t id, uint64_t cameraId, float maxDistance, float tolerance) -> Vector3 {
        if (!std::isfinite(maxDistance) || maxDistance <= 0 || !std::isfinite(tolerance) || tolerance < 0)
            throw std::invalid_argument("Invalid canvas interaction distance");
        context->Component<Canvas>(id);
        auto& transform = context->Component<Transform>(id);
        const auto camera = context->Resolve(cameraId);
        Ray ray;
        auto& window = app.scene.GetWindow();
        const Vector2 pointer = window.IsMouseLocked() ? Vector2(window.GetScreenWidth() * .5f, window.GetScreenHeight() * .5f) : app.scene.GetInputManager().mouse;
        if (!app.scene.TryGetRayFromCamera(camera, pointer, ray))
            return {0, 0, 0};
        const auto model = transform.GetModelMatrix();
        // World canvases commonly scale pixel units down to millimeters. A small
        // nonzero determinant is valid; only a singular transform has no inverse.
        const float determinant = glm::determinant(model);
        if (!std::isfinite(determinant) || determinant == 0)
            return {0, 0, 0};
        const auto normal = glm::normalize(Vector3(model * Vector4(0, 0, 1, 0)));
        const float denominator = glm::dot(ray.direction, normal);
        if (std::abs(denominator) < .00001f)
            return {0, 0, 0};
        const float distance = glm::dot(Vector3(model * Vector4(0, 0, 0, 1)) - ray.origin, normal) / denominator;
        if (distance < 0 || distance > maxDistance)
            return {0, 0, 0};
        const auto hits = app.scene.RaycastAll(ray.origin, ray.direction, distance);
        if (!hits.empty() && hits.front().distance < distance - tolerance)
            return {0, 0, 0};
        const auto local = glm::inverse(model) * Vector4(ray.origin + ray.direction * distance, 1);
        return {local.x, local.y, 1};
    });
    bindings.Method(owner, "Canvas", "ScreenPoint", [context, &app](uint64_t id, Vector3 local) -> Vector3 {
        Finite(local);
        context->Component<Canvas>(id);
        if (!app.scene.HasLastRenderCamera())
            return {0, 0, 0};
        const auto clip = app.scene.GetLastRenderProjection() * app.scene.GetLastRenderView() * context->Component<Transform>(id).GetModelMatrix() * Vector4(local, 1);
        if (clip.w <= 0)
            return {0, 0, 0};
        const auto ndc = Vector3(clip) / clip.w;
        return {(ndc.x + 1) * app.scene.GetWindow().GetScreenWidth() * .5f,
            (1 - ndc.y) * app.scene.GetWindow().GetScreenHeight() * .5f, 1};
    });
    bindings.Method(owner, "UIInputField", "Focus", [context, &app](uint64_t id) -> bool {
        context->Component<UIInputField>(id);
        auto* ui = app.scene.GetSystem<UIInteractionSystem>();
        if (!ui)
            throw std::runtime_error("UIInteractionSystem is not active");
        return ui->FocusInputField(context->Resolve(id).TryGet());
    });
    bindings.Method(owner, "UIInputField", "Blur", [context, &app](uint64_t id) {
        if (context->Component<UIInputField>(id).focused)
            if (auto* ui = app.scene.GetSystem<UIInteractionSystem>())
                ui->FocusInputField(nullptr);
    });
    bindings.Method(owner, "UIInputField", "Focused", [context](uint64_t id) -> bool { return context->Component<UIInputField>(id).focused; });
    bindings.Method(owner, "UIInputField", "Text", [context](uint64_t id) -> std::string { return context->Component<UIInputField>(id).text; });
    bindings.Method(owner, "UIInputField", "SetText", [context](uint64_t id, std::string text) {
        UIInteractionSystem::SetInputText(context->Component<UIInputField>(id), text);
    });
    bindings.Method(owner, "UIInputField", "InsertText", [context](uint64_t id, std::string text) {
        UIInteractionSystem::InsertInputText(context->Component<UIInputField>(id), text);
    });
    bindings.Method(owner, "UIInputField", "Backspace", [context](uint64_t id) {
        UIInteractionSystem::BackspaceInputText(context->Component<UIInputField>(id));
    });

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
    bindings.Method(owner,"AudioSource","Play",[context](uint64_t id)->bool {return context->Component<AudioSource>(id).Play();});
    bindings.Method(owner,"AudioSource","PlayOneShot",[context](uint64_t id,std::string path,float scale)->bool {return context->Component<AudioSource>(id).PlayOneShot(AudioAssetHandle{UUID(0),path},scale);});
    bindings.Method(owner,"AudioSource","Stop",[context](uint64_t id){context->Component<AudioSource>(id).Stop();});
    bindings.Method(owner,"TrailRenderer","Clear",[context](uint64_t id){context->Component<TrailRenderer>(id).Clear();});
    bindings.Method(owner,"AudioSource","Pause",[context](uint64_t id){context->Component<AudioSource>(id).Pause();});
    bindings.Method(owner,"AudioSource","UnPause",[context](uint64_t id){context->Component<AudioSource>(id).UnPause();});
    bindings.Method(owner,"AudioSource","IsPlaying",[context](uint64_t id)->bool{return context->Component<AudioSource>(id).IsPlaying();});
    bindings.Method(owner,"AudioSource","ClipUUID",[context](uint64_t id)->uint64_t{return static_cast<uint64_t>(context->Component<AudioSource>(id).clip.uuid);});
    bindings.Method(owner,"AudioSource","SetClipUUID",[context](uint64_t id,uint64_t uuid){context->Component<AudioSource>(id).clip={UUID(uuid),""};});
    bindings.Method(owner,"AudioSource","ClipPath",[context](uint64_t id)->std::string{return context->Component<AudioSource>(id).clip.path;});
    bindings.Method(owner,"AudioSource","SetClipPath",[context](uint64_t id,std::string path){context->Component<AudioSource>(id).clip={UUID(0),path};});
    bindings.Method(owner,"Audio","Backend",[]()->std::string{return Audio::SpatialBackend();});
    bindings.Method(owner,"Presentation","Color",[context](uint64_t id)->Vector4{return context->Component<Model>(id).color;});
    bindings.Method(owner,"Presentation","Text",[context](uint64_t id)->std::string{return context->Component<Text>(id).text;});
    bindings.Method(owner, "Scene", "Find", [context](std::string name) -> uint64_t { return context->Handle(context->app.scene.FindEntityWithName(name)); });
    bindings.Method(owner, "Scene", "IsValid", [context](uint64_t id) -> bool { try { return context->Resolve(id).IsValid(); } catch (...) { return false; } });
    bindings.Method(owner, "Scene", "Name", [context](uint64_t id) -> std::string { return context->Resolve(id).GetName(); });
    bindings.Method(owner, "Scene", "SetActive", [context](uint64_t id, bool active) { context->Resolve(id).SetActive(active); });
    bindings.Method(owner, "Scene", "Active", [context](uint64_t id) -> bool { return context->Resolve(id).IsActive(); });
    bindings.Method(owner, "Transform", "Position", [context](uint64_t id) -> Vector3 { return context->Component<Transform>(id).GetGlobalPosition(); });
    bindings.Method(owner, "Transform", "Parent", [context](uint64_t id)->uint64_t {return context->Handle(context->Component<Transform>(id).parent.TryGet());});
    bindings.Method(owner, "Transform", "SetParent", [context](uint64_t id,uint64_t parent,bool worldPositionStays) {
        auto& t=context->Component<Transform>(id);auto p=parent?context->Resolve(parent):Entity{};
        if(parent && !p.HasComponent<Transform>())throw std::invalid_argument("Parent requires Transform");
        const auto position=t.position;const auto rotation=t.rotation;const auto scale=t.scale;
        t.SetParent(parent? p.TryGet() : nullptr);
        if(!worldPositionStays){t.position=position;t.rotation=rotation;t.scale=scale;}
    });
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
    bindings.Method(owner,"Transform","LocalPosition",[context](uint64_t id)->Vector3{return context->Component<Transform>(id).position;});
    bindings.Method(owner,"Transform","SetLocalPosition",[context](uint64_t id,Vector3 v){Finite(v);context->Component<Transform>(id).position=v;});
    bindings.Method(owner,"Transform","LocalRotation",[context](uint64_t id)->Quaternion{return context->Component<Transform>(id).rotation;});
    bindings.Method(owner,"Transform","SetLocalRotation",[context](uint64_t id,Quaternion v){if(!std::isfinite(glm::length(v))||glm::length(v)<1e-6f)throw std::invalid_argument("Invalid quaternion");context->Component<Transform>(id).rotation=glm::normalize(v);});
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
    bindings.Method(owner, "VR", "Focused", [&app]() -> bool {
        return app.GetVR() && Runtime(app).GetState().focused && Runtime(app).GetState().shouldRender;
    });
    bindings.Method(owner, "VR", "ClipPlanes", [&app]() -> Vector3 { return Vector3(Runtime(app).GetClipPlanes(),0); });
    bindings.Method(owner, "VR", "SetClipPlanes", [&app](float nearClip,float farClip) { Runtime(app).SetClipPlanes(nearClip,farClip); });
    bindings.Method(owner, "VR", "SetOrigin", [&app](Vector3 position, Quaternion rotation) {
        Finite(position);
        if (!std::isfinite(glm::length(rotation)) || glm::length(rotation)<.001f)
            throw std::invalid_argument("Invalid VR origin rotation");
        Runtime(app).SetOrigin(position,rotation);
    });
    bindings.Method(owner, "VR", "HeadTrackingPosition", [&app]() -> Vector3 { return Runtime(app).GetState().head.position; });
    bindings.Method(owner, "VR", "HeadTrackingRotation", [&app]() -> Quaternion { return Runtime(app).GetState().head.orientation; });
    bindings.Method(owner, "VR", "Stick", [&app](int index) -> Vector3 {
        const auto hand=Hand(app,index); const auto& state=Runtime(app).GetState();
        return state.focused && state.shouldRender && hand.active ? Vector3(hand.stick,0) : Vector3(0);
    });
    bindings.Method(owner, "VR", "Button", [&app](int index, int button) -> bool {
        const auto hand=Hand(app,index); const auto& state=Runtime(app).GetState();
        if (!state.focused || !state.shouldRender || !hand.active) return false;
        return button==0 ? hand.select : button==1 ? hand.jump : button==2 ? hand.interact : button==3 ? hand.weaponSwitch : button==4 && hand.grappleToggle;
    });
    bindings.Method(owner, "VR", "Simulated", [&app]() -> bool { return app.GetVR() && app.GetVR()->IsSimulated(); });
    bindings.Method(owner, "VR", "Tracked", [&app](int index) -> bool
    {
        const auto hand = Hand(app, index); const auto& state = Runtime(app).GetState();
        return state.focused && state.shouldRender && hand.active && hand.grip.valid;
    });
    bindings.Method(owner, "VR", "Position", [&app](int index) -> Vector3 { return Runtime(app).WorldPose(Hand(app,index).grip).position; });
    bindings.Method(owner, "VR", "Rotation", [&app](int index) -> Quaternion { return Runtime(app).WorldPose(Hand(app,index).grip).orientation; });
    bindings.Method(owner, "VR", "AimRotation", [&app](int index) -> Quaternion
    {
        const auto hand = Hand(app, index);
        return Runtime(app).WorldPose(hand.aim.valid ? hand.aim : hand.grip).orientation;
    });
    bindings.Method(owner, "VR", "Squeeze", [&app](int index) -> float { return Hand(app,index).squeeze; });
    bindings.Method(owner, "VR", "Trigger", [&app](int index) -> float { return Hand(app,index).trigger; });
    bindings.Method(owner, "VR", "Haptic", [&app](int index, float amplitude, float seconds) -> bool
    {
        if (index < 0 || index > 1 || !std::isfinite(amplitude) || !std::isfinite(seconds)) throw std::invalid_argument("Invalid haptic arguments");
        return Runtime(app).Haptic(index, std::clamp(amplitude,0.f,1.f), std::clamp(seconds,0.f,.3f));
    });
    // Explicit simulation hook for deterministic tests; never changes real headset input.
    bindings.Method(owner, "VR", "SimulateFocus", [&app](bool focused) {
        auto& vr=Runtime(app);if(!vr.IsSimulated())throw std::runtime_error("Simulation cannot override real XR input");
        const auto state=vr.GetState();vr.SetSimulatedInput(state.head,state.hands,focused);
    });
    bindings.Method(owner, "VR", "SimulateControls", [&app](Vector3 left,Vector3 right,float leftGrip,float rightGrip,int buttons) {
        auto& vr=Runtime(app); if(!vr.IsSimulated())throw std::runtime_error("Simulation cannot override real XR input");
        Finite(left);Finite(right);auto state=vr.GetState();const auto inverse=glm::inverse(vr.GetOrigin());
        for(int i=0;i<2;++i) {
            auto& hand=state.hands[i];hand.active=true;
            hand.grip={Vector3(inverse*Vector4(i?right:left,1)),Quaternion(1,0,0,0),true};hand.aim=hand.grip;
            hand.squeeze=i?rightGrip:leftGrip;hand.trigger=(buttons&(i?32:16))?1.f:0.f;
            hand.select=(buttons&(i?4:1))!=0;hand.jump=(buttons&(i?8:2))!=0;hand.interact=i==1 && (buttons&64)!=0;
        }
        vr.SetSimulatedInput(state.head,state.hands,true);
    });
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
