#pragma once
#include <Canis/AssetHandle.hpp>
#include <Canis/UUID.hpp>
#include <Canis/Math.hpp>
#include <Canis/External/entt.hpp>

#include <string>
#include <limits>
#include <vector>

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
    struct ScriptConf;

    struct RaycastHit
    {
        Entity* entity = nullptr;
        Vector3 point = Vector3(0.0f);
        Vector3 normal = Vector3(0.0f);
        float distance = 0.0f;
        float fraction = 0.0f;
    };

    class Scene
    {
    public:
        struct SystemTiming
        {
            std::string name = "";
            float updateMs = 0.0f;
            float renderMs = 0.0f;
            const System* system = nullptr;
        };

        App* app = nullptr;
        
        void Init(App *_app, Window *_window, InputManager *_inputManger, std::string _path);
        void Update(float _deltaTime);
        void Render(float _deltaTime);
        void Unload();
        
        void Save(std::vector<ScriptConf>& _scriptRegistry);
        YAML::Node EncodeScene(std::vector<ScriptConf>& _scriptRegistry);
        YAML::Node EncodeEntity(std::vector<ScriptConf>& _scriptRegistry, Entity &_entity);

        void Load(std::vector<ScriptConf>& _scriptRegistry);
        void LoadSceneNode(std::vector<ScriptConf>& _scriptRegistry, YAML::Node &_root);
        std::vector<Entity*> LoadEntityNodes(std::vector<ScriptConf>& _scriptRegistry, YAML::Node &_entities, bool _copyUUID = true);
        Canis::Entity& DecodeEntity(std::vector<ScriptConf>& _scriptRegistry, YAML::Node _node, bool _copyUUID = true);
        void GetEntityAfterLoad(Canis::UUID _uuid, Canis::Entity* &_variable);
        std::vector<Entity*> Instantiate(const SceneAssetHandle &_sceneAssetHandle);

        Window& GetWindow() { return *m_window; }
        InputManager& GetInputManager() { return *m_inputManager; }
        entt::registry& GetRegistry() { return m_registry; }
        const entt::registry& GetRegistry() const { return m_registry; }
        bool Raycast(const Vector3 &_origin, const Vector3 &_direction, RaycastHit &_hit, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max());
        bool Raycast(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max());
        std::vector<RaycastHit> RaycastAll(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance = std::numeric_limits<float>::infinity(), u32 _mask = std::numeric_limits<u32>::max());

        void SetEditorCamera3DOverride(const Matrix4 &_view, const Matrix4 &_projection);
        void SetEditorCamera2DOverride(const Matrix4 &_cameraMatrix, const Vector2 &_position);
        void ClearEditorCameraOverrides();

        bool HasEditorCamera3DOverride() const { return m_editorCamera3DOverrideEnabled; }
        bool HasEditorCamera2DOverride() const { return m_editorCamera2DOverrideEnabled; }
        const Matrix4& GetEditorCamera3DView() const { return m_editorCamera3DView; }
        const Matrix4& GetEditorCamera3DProjection() const { return m_editorCamera3DProjection; }
        const Matrix4& GetEditorCamera2DMatrix() const { return m_editorCamera2DMatrix; }
        const Vector2& GetEditorCamera2DPosition() const { return m_editorCamera2DPosition; }
        UUID GetEnvironmentSkyboxUUID() const { return m_environmentSkyboxUUID; }
        void SetEnvironmentSkyboxUUID(UUID _uuid) { m_environmentSkyboxUUID = _uuid; }
        bool IsPaused() const { return m_paused; }
        void SetPaused(bool _paused) { m_paused = _paused; }


        Entity* CreateEntity(std::string _name = "", std::string _tag = "");
        Entity* GetEntity(int _id);
        Entity* GetEntityWithUUID(Canis::UUID _uuid);
        Entity* FindEntityWithName(std::string _name);

        Entity* GetEntityWithTag(std::string _tag);
        std::vector<Entity*> GetEntitiesWithTag(std::string _tag);

        void Destroy(int _id);
        void Destroy(Entity& _entity);

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
            System *s = new T();

            m_updateSystems.push_back(s);
            ReadySystem(s);
        }

        template <typename T>
        void CreateRenderSystem()
        {
            System *s = new T();

            m_renderSystems.push_back(s);
            ReadySystem(s);
        }

        std::vector<Entity*>& GetEntities() { return m_entities; }
        const std::vector<SystemTiming>& GetSystemTimings() const { return m_systemTimings; }
    private:
        std::string m_name = "main";
        std::string m_path = "assets/scenes/main.scene";
        Window *m_window;
        InputManager *m_inputManager;

        std::vector<Entity*> m_entities = {};
        entt::registry m_registry = {};
        std::vector<System*> m_systems = {};
        std::vector<System*> m_updateSystems = {};
        std::vector<System*> m_renderSystems = {};
        std::vector<SystemTiming> m_systemTimings = {};
        std::vector<int> m_entitiesToReady = {};
        std::vector<int> m_entitiesToDestroy = {};
        bool m_isUpdating = false;
        bool m_isLoadingEntityNodes = false;
        bool m_editorCamera3DOverrideEnabled = false;
        bool m_editorCamera2DOverrideEnabled = false;
        Matrix4 m_editorCamera3DView = Matrix4(1.0f);
        Matrix4 m_editorCamera3DProjection = Matrix4(1.0f);
        Matrix4 m_editorCamera2DMatrix = Matrix4(1.0f);
        Vector2 m_editorCamera2DPosition = Vector2(0.0f);
        UUID m_environmentSkyboxUUID = UUID(0);
        bool m_paused = false;

        // this is used when duplicating entity
        std::unordered_map<UUID, UUID> m_targetUUIDNewUUID;

        struct EntityConnectInfo {
            Canis::UUID targetUUID;
            Canis::Entity** variable;
        };

        std::vector<EntityConnectInfo> m_entityConnectInfo = {};

        void QueueEntityForReady(int _id);
        void DestroyNow(int _id);
        void ReadySystem(System *_system);
        SystemTiming* GetSystemTiming(System* _system);
    };
}
