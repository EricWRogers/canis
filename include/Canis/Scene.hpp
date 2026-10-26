#pragma once
#include <Canis/AssetHandle.hpp>
#include <Canis/UUID.hpp>
#include <Canis/Tag.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Math.hpp>
#include <Canis/LightingProbe.hpp>
#include <Canis/LightingScenario.hpp>
#include <Canis/CloudVolume.hpp>
#include <Canis/SurfaceWetness.hpp>
#include <Canis/PostProcessPipeline.hpp>
#include <Canis/Tweening/TweenWorld.hpp>
#include <Canis/External/entt.hpp>

#include <string>
#include <limits>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace YAML
{
    class Node;
}

namespace Canis
{
    
    class App;
    class Window;
    class InputManager;
    class Entity;
    class System;
    class ScriptableEntity;
    struct ScriptConf;
    struct SystemConf;

    struct RaycastHit
    {
        Entity entity = nullptr;
        Vector3 point = Vector3(0.0f);
        Vector3 normal = Vector3(0.0f);
        float distance = 0.0f;
        float fraction = 0.0f;
    };

    struct Ray
    {
        Vector3 origin = Vector3(0.0f);
        Vector3 direction = Vector3(0.0f, 0.0f, -1.0f);
    };

    struct DebugGizmoLine
    {
        Vector3 start = Vector3(0.0f);
        Vector3 end = Vector3(0.0f);
        Color color = Color(1.0f);
    };

    class Scene
    {
        friend class Entity;
        RenderTarget m_qualityRenderTarget;
        #if CANIS_EDITOR
            friend class Editor;
        #endif
    public:
        struct SystemTiming
        {
            std::string name = "";
            float updateMs = 0.0f;
            float renderMs = 0.0f;
            const System* system = nullptr;
        };

        App* app = nullptr;
        Tweening::TweenWorld tweens{this};
        std::function<void(int, int)> managedTween;
        std::function<bool(uint64_t, const std::string&)> managedUIAction;
        
        ~Scene();
        void Init(App *_app, Window *_window, InputManager *_inputManger);
        void Update(float _deltaTime);
        // Optional managed host participates before Gameplay scripts, after Interaction.
        std::function<void(float)> managedUpdate;
        std::function<void()> managedStop;
        uint64_t ScriptingEpoch() const { return m_scriptingEpoch; }
        void UpdateEditor();
        void Render(float _deltaTime);
        void Render(float _deltaTime, bool _deferScreenSpaceUI);
        void CompositeScreenSpaceUI(unsigned int _framebuffer, int _width, int _height);
        void Unload();
        
        bool Save();
        YAML::Node EncodeScene();
        YAML::Node EncodeEntity(Entity &_entity);

        void Load(std::string _path);
        void LoadSceneNode(YAML::Node &_root);
        std::vector<Entity*> LoadEntityNodes(YAML::Node &_entities, bool _copyUUID = true);
        // Editor keeps authored children until Play; Stop restores its snapshot.
        bool ActivateCombinedModelsForPlay();
        Canis::Entity& DecodeEntity(YAML::Node _node, bool _copyUUID = true);
        void GetEntityAfterLoad(Canis::UUID uuid, Entity& variable);
        std::vector<Entity*> Instantiate(const SceneAssetHandle &_sceneAssetHandle);
        void ForceReady(Entity& _entity);
        // Keeps self-removing scripts alive until their signal action returns.
        void InvokeScriptAction(ScriptableEntity& script, const std::function<void()>& action);

        Window& GetWindow() { return *m_window; }
        InputManager& GetInputManager() { return *m_inputManager; }
        entt::registry& GetRegistry() { return m_registry; }
        const entt::registry& GetRegistry() const { return m_registry; }
        const std::string& GetPath() const { return m_path; }
        bool Raycast(const Vector3 &_origin, const Vector3 &_direction, RaycastHit &_hit, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max());
        bool Raycast(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max());
        std::vector<RaycastHit> RaycastAll(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max());
        bool BuildNavMesh(Entity& _surfaceEntity);
        void InvalidateNavMesh(Entity& _surfaceEntity);
        std::vector<Vector3> FindNavMeshPath(
            Entity& _surfaceEntity,
            const Vector3& _start,
            const Vector3& _destination);
        std::size_t GetNavMeshPointCount(const Entity& _surfaceEntity) const;
        bool BuildCloudNav(Entity& _surfaceEntity);
        void InvalidateCloudNav(Entity& _surfaceEntity);
        std::vector<Vector3> FindCloudNavPath(
            Entity& _surfaceEntity,
            const Vector3& _start,
            const Vector3& _destination);
        std::size_t GetCloudNavPointCount(const Entity& _surfaceEntity) const;
        bool TryGetRayFromCamera(const Entity &_cameraEntity, const Vector2 &_screenPosition, Ray &_ray) const;
        bool TryGetMouseRayFromCamera(const Entity &_cameraEntity, Ray &_ray) const;
        void DrawDebugGizmoLine(const Vector3 &_start, const Vector3 &_end, const Color &_color = Color(1.0f));
        void ClearDebugGizmoLines();
        const std::vector<DebugGizmoLine>& GetDebugGizmoLines() const { return m_debugGizmoLines; }

        struct VRCamera
        {
            Matrix4 view{1.0f}, projection{1.0f};
            float nearClip = 0.05f, farClip = 200.0f;
            bool enabled = false;
        };
        void SetVRCamera(const Matrix4& view, const Matrix4& projection, float nearClip, float farClip)
        { m_vrCamera = {view, projection, nearClip, farClip, true}; }
        void ClearVRCamera() { m_vrCamera.enabled = false; }
        const VRCamera& GetVRCamera() const { return m_vrCamera; }

        void SetEditorCamera3DOverride(const Matrix4 &_view, const Matrix4 &_projection);
        void SetEditorCamera2DOverride(const Matrix4 &_cameraMatrix, const Vector2 &_position);
        void ClearEditorCameraOverrides();

        bool HasEditorCamera3DOverride() const { return m_editorCamera3DOverrideEnabled; }
        bool HasEditorCamera2DOverride() const { return m_editorCamera2DOverrideEnabled; }
        const Matrix4& GetEditorCamera3DView() const { return m_editorCamera3DView; }
        const Matrix4& GetEditorCamera3DProjection() const { return m_editorCamera3DProjection; }
        const Matrix4& GetEditorCamera2DMatrix() const { return m_editorCamera2DMatrix; }
        const Vector2& GetEditorCamera2DPosition() const { return m_editorCamera2DPosition; }
        const Color& GetEnvironmentAmbientLight() const { return m_environmentAmbientLight; }
        void SetEnvironmentAmbientLight(const Color &_ambientLight) { m_environmentAmbientLight = _ambientLight; }
        float GetEnvironmentAmbientLightIntensity() const { return m_environmentAmbientLightIntensity; }
        void SetEnvironmentAmbientLightIntensity(float _intensity) { m_environmentAmbientLightIntensity = std::max(_intensity, 0.0f); }
        bool GetEnvironmentReflectionsEnabled() const { return m_environmentReflectionsEnabled; }
        void SetEnvironmentReflectionsEnabled(bool _enabled) { m_environmentReflectionsEnabled = _enabled; }
        UUID GetEnvironmentSkyboxUUID() const { return m_environmentSkyboxUUID; }
        void SetEnvironmentSkyboxUUID(UUID _uuid) { m_environmentSkyboxUUID = _uuid; }
        UUID GetEnvironmentPostProcessUUID() const { return m_environmentPostProcessUUID; }
        void SetEnvironmentPostProcessUUID(UUID _uuid) { m_environmentPostProcessUUID = _uuid; }
        CloudVolumeSettings& GetCloudVolumeSettings() { return m_cloudVolume; }
        const CloudVolumeSettings& GetCloudVolumeSettings() const { return m_cloudVolume; }
        SurfaceWetnessSettings& GetSurfaceWetnessSettings() { return m_surfaceWetness; }
        const SurfaceWetnessSettings& GetSurfaceWetnessSettings() const { return m_surfaceWetness; }
        double GetCloudSimulationTime() const { return m_cloudSimulationTime; }
        std::vector<LightingProbe>& GetLightingProbes() { return m_lightingProbes; }
        const std::vector<LightingProbe>& GetLightingProbes() const { return m_lightingProbes; }
        const YAML::Node& GetLightingScenarioSources() const { return m_lightingScenarioSources; }
        void SetLightingScenarioSources(const YAML::Node& sources) { m_lightingScenarioSources = YAML::Clone(sources); }
        std::vector<LightingScenario>& GetLightingScenarios() { return m_lightingScenarios; }
        const std::vector<LightingScenario>& GetLightingScenarios() const { return m_lightingScenarios; }
        LightingScenario* FindLightingScenario(const std::string& idOrName);
        const LightingScenario* FindLightingScenario(const std::string& idOrName) const;
        LightingScenario CaptureLightingScenario(const std::string& name) const;
        const std::string& GetInitialLightingScenario() const { return m_initialLightingScenario; }
        bool SetInitialLightingScenario(const std::string& idOrName);
        LightingScenarioBlend GetLightingScenarioBlend() const;
        bool SetLightingScenarioPreview(const std::string& fromId, const std::string& toId, float blend);
        void ClearLightingScenarioPreview();
        bool HasLightingScenarioPreview() const { return m_lightingScenarioPreview; }
        // Animation-driven control validates both endpoints once. Subsequent
        // weights only change transient runtime state and never scan assets.
        bool BeginLightingScenarioBlend(const std::string& fromId, const std::string& toId);
        bool SetLightingScenarioBlend(float blend);
        void EndLightingScenarioBlend();
        bool IsLightingScenarioBlendManual() const { return m_lightingScenarioManual; }
        bool TransitionLightingScenario(const std::string& idOrName, float seconds);
        bool IsLightingScenarioTransitioning() const { return m_lightingScenarioTransitioning; }
        bool HasQueuedLightingScenarioTransition() const { return !m_lightingScenarioQueued.empty(); }
        void UpdateLightingScenarioTransition(float dt);
        void ResetLightingScenarioRuntime();
        struct BakingSettings {
            bool lightmaps = true;
            bool staticShadows = true;
            bool occlusion = true;
        };
        BakingSettings& GetBakingSettings() { return m_baking; }
        const BakingSettings& GetBakingSettings() const { return m_baking; }
        bool GetShowColliders() const { return m_showColliders; }
        void SetShowColliders(bool _showColliders) { m_showColliders = _showColliders; }
        void SetLastRenderCamera(const Matrix4& _view, const Matrix4& _projection, const Vector3& _cameraPosition, float _nearClip, float _farClip);
        void ClearLastRenderCamera();
        const Matrix4& GetLastRenderProjection() const { return m_lastRenderProjection; }
        const Matrix4& GetLastRenderView() const { return m_lastRenderView; }
        const Vector3& GetLastRenderCameraPosition() const { return m_lastRenderCameraPosition; }
        float GetLastRenderCameraNearClip() const { return m_lastRenderCameraNearClip; }
        float GetLastRenderCameraFarClip() const { return m_lastRenderCameraFarClip; }
        bool HasLastRenderCamera() const { return m_lastRenderCameraValid; }
        bool IsPaused() const { return m_paused; }
        void SetPaused(bool paused);
        void QuitGame();


        Entity CreateEntity(std::string _name = "", std::string _tag = "");
        Entity FindEntity(UUID uuid) { return GetEntityWithUUID(uuid); }
        Entity* GetEntity(int _id);
        // Scene-owned transient ordering slot, not persistent entity identity.
        int GetEntityIndex(const Entity& entity) const;
        Entity* GetEntityWithUUID(Canis::UUID _uuid);
        UUID GetLiveEntityUUID(const Entity* _entity) const;
        UUID GetLiveEntityUUID(const Entity& entity) const { return entity.GetUUID(); }
        UUID GetReferenceUUID(entt::entity handle) const;
        Entity* FindEntityWithName(std::string _name);

        Entity* GetEntityWithTag(TagId tag);
        Entity* GetEntityWithTag(std::string_view tag) { return GetEntityWithTag(TagIdFromName(tag)); }
        std::vector<Entity*> GetEntitiesWithTag(TagId tag);
        std::vector<Entity*> GetEntitiesWithTag(std::string_view tag) { return GetEntitiesWithTag(TagIdFromName(tag)); }

        template <typename Tag> requires requires(Tag value) { ToTagId(value); }
        Entity* GetEntityWithTag(Tag value)
        {
            return GetEntityWithTag(ToTagId(value));
        }

        template <typename Tag> requires requires(Tag value) { ToTagId(value); }
        std::vector<Entity*> GetEntitiesWithTag(Tag value)
        {
            return GetEntitiesWithTag(ToTagId(value));
        }

        void Destroy(int _id);
        void Destroy(Entity& _entity);
        void RetireScript(ScriptableEntity* script);

        template <typename T>
        T *GetSystem()
        {
            T *castedSystem = nullptr;

            for (System *s : m_systems)
                if ((castedSystem = dynamic_cast<T *>(s)) != nullptr)
                    return castedSystem;

            return nullptr;
        }

        template <typename T>
        void CreateSystem()
        {
            (void)CreateSystem(new T());
        }

        template <typename T>
        void CreateRenderSystem()
        {
            (void)CreateRenderSystem(new T());
        }

        System* CreateSystem(System* _system);
        System* CreateRenderSystem(System* _system);

        std::vector<Entity*>& GetEntities() { return m_entities; }
        const std::vector<SystemTiming>& GetSystemTimings() const { return m_systemTimings; }
    private:
        VRCamera m_vrCamera;
        std::string m_name = "main";
        std::string m_path = "assets/scenes/main.scene";
        Window *m_window = nullptr;
        InputManager *m_inputManager = nullptr;

        std::vector<Entity*> m_entities = {};
        std::unordered_map<entt::entity, int> m_entityIndices;
        struct EntityState { bool pendingDestroy = false; bool destroying = false; };
        std::unordered_map<entt::entity, EntityState> m_entityStates;
        // Serialization history for missing references; cleared when this scene unloads.
        std::unordered_map<entt::entity, UUID> m_referenceUUIDs;
        std::unordered_map<UUID, entt::entity> m_missingReferences;
        EntityState* GetEntityState(entt::entity handle);
        Entity ResolveReference(UUID uuid);
        entt::registry m_registry = {};
        std::vector<System*> m_systems = {};
        std::vector<System*> m_updateSystems = {};
        std::vector<System*> m_renderSystems = {};
        std::vector<SystemTiming> m_systemTimings = {};
        uint64_t m_scriptingEpoch = 1;
        std::vector<DebugGizmoLine> m_debugGizmoLines = {};
        std::vector<Entity> m_entitiesToReady = {};
        std::vector<ScriptableEntity*> m_retiredScripts;
        void FlushRetiredScripts();
        ScriptableEntity* InvokeScriptCallback(ScriptableEntity* script, bool ready);
        unsigned m_scriptCallbackDepth = 0;
        std::vector<Entity> m_entitiesToDestroy = {};
        bool m_isUpdating = false;
        bool m_isLoadingEntityNodes = false;
        bool m_editorCamera3DOverrideEnabled = false;
        bool m_editorCamera2DOverrideEnabled = false;
        Matrix4 m_editorCamera3DView = Matrix4(1.0f);
        Matrix4 m_editorCamera3DProjection = Matrix4(1.0f);
        Matrix4 m_editorCamera2DMatrix = Matrix4(1.0f);
        Vector2 m_editorCamera2DPosition = Vector2(0.0f);
        Color m_environmentAmbientLight = Color(0.24f, 0.26f, 0.32f, 1.0f);
        BakingSettings m_baking;
        std::vector<LightingProbe> m_lightingProbes;
        std::vector<LightingScenario> m_lightingScenarios;
        CloudVolumeSettings m_cloudVolume;
        SurfaceWetnessSettings m_surfaceWetness;
        double m_cloudSimulationTime = 0;
        YAML::Node m_lightingScenarioSources;
        std::string m_initialLightingScenario;
        std::string m_lightingScenarioFrom, m_lightingScenarioTo, m_lightingScenarioQueued;
        std::string m_lightingScenarioPreviewFrom, m_lightingScenarioPreviewTo;
        float m_lightingScenarioBlend = 0, m_lightingScenarioStart = 0, m_lightingScenarioEnd = 1;
        float m_lightingScenarioElapsed = 0, m_lightingScenarioDuration = 0, m_lightingScenarioQueuedDuration = 0;
        float m_lightingScenarioPreviewBlend = 0;
        bool m_lightingScenarioTransitioning = false, m_lightingScenarioPreview = false;
        bool m_lightingScenarioManual = false;
        float m_environmentAmbientLightIntensity = 1.0f;
        bool m_environmentReflectionsEnabled = true;
        UUID m_environmentSkyboxUUID = UUID(0);
        UUID m_environmentPostProcessUUID = UUID(0);
        bool m_showColliders = false;
        Matrix4 m_lastRenderView = Matrix4(1.0f);
        Matrix4 m_lastRenderProjection = Matrix4(1.0f);
        Vector3 m_lastRenderCameraPosition = Vector3(0.0f);
        float m_lastRenderCameraNearClip = 0.1f;
        float m_lastRenderCameraFarClip = 100.0f;
        bool m_lastRenderCameraValid = false;
        bool m_paused = false;

        // this is used when duplicating entity

        std::unordered_map<UUID, Entity> m_loadingEntities;

        void QueueEntityForReady(Entity& entity);
        void DestroyNow(int _id);
        void DestroyNow(const Entity& handle);
        void MarkPendingDestroy(Entity* entity);
        void ReadySystem(System *_system);
        SystemTiming* GetSystemTiming(System* _system);
    };
}
