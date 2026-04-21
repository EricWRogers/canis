#pragma once

#include <functional>
#include <filesystem>
#include <string>
#include <thread>
#include <mutex>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <Canis/UUID.hpp>
#include <Canis/Asset.hpp>
#include <Canis/AssetHandle.hpp>
#include <Canis/PostProcessPipeline.hpp>

namespace Canis
{
    class Scene;
    class App;
    class Window;
    class Camera2D;
    class Entity;
    struct GameCodeObject;

    enum EditorMode
    {
        EDIT,
        PLAY,
        PAUSE,
        HIDDEN
    };

    enum GuizmoMode
    {
        LOCAL = 0,
        WORLD = 1,
    };

    struct AssetDragData
    {
        UUID uuid;
        char path[1024]; // full path to file
    };

    class Editor
    {
    friend class Scene;
    friend class App;

    public:
        Editor() = default;
        ~Editor();
        void Init(Window* _window);
        void Draw(Scene* _scene, Window* _window, App* _app, GameCodeObject* _gameSharedLib, float _deltaTime);
        void BeginGameRender(Window* _window);
        void BeginPlayRender(Window* _window);
        void EndGameRender(Window* _window);
        void RenderGameDebug();
        unsigned int GetGameInputWindowID() const { return m_gameInputWindowID; }

        EditorMode GetMode() { return m_mode; }
        void RequestStopPlayMode() { m_stopPlayModeRequested = true; }
        bool ConsumeStopPlayModeRequest()
        {
            const bool requested = m_stopPlayModeRequested;
            m_stopPlayModeRequested = false;
            return requested;
        }
        void StopPlayMode();
        void FocusEntity(Canis::Entity* _entity);

        // inspector variables
        void InputEntity(const std::string& _name, Canis::Entity* &_variable);
        void InputEntity(const std::string& _name, const char* _idSuffix, Canis::Entity* &_variable);
        void InputAudioAsset(const std::string& _name, Canis::AudioAssetHandle &_variable);
        void InputAudioAsset(const std::string& _name, const char* _idSuffix, Canis::AudioAssetHandle &_variable);
        void InputAnimationClip(const std::string& _name, Canis::AnimationClip2DID &_variable);
        void InputAnimationClip(const std::string& _name, const char* _idSuffix, Canis::AnimationClip2DID &_variable);
        void InputSceneAsset(const std::string& _name, Canis::SceneAssetHandle &_variable);
        void InputSceneAsset(const std::string& _name, const char* _idSuffix, Canis::SceneAssetHandle &_variable);
        //void InputScriptableEntity(const std::string& _name, const std::string& _script, );

        template <typename T, typename Drawer>
        void RegisterInspectorFieldDrawer(Drawer&& _drawer)
        {
            using FieldType = std::remove_cvref_t<T>;

            m_inspectorFieldDrawers[std::type_index(typeid(FieldType))] =
                [drawer = std::forward<Drawer>(_drawer)](Editor& _editor, const char* _label, const char* _idSuffix, void* _value) mutable
                {
                    drawer(_editor, _label, _idSuffix, *static_cast<FieldType*>(_value));
                };
        }

        template <typename T>
        void UnregisterInspectorFieldDrawer()
        {
            m_inspectorFieldDrawers.erase(std::type_index(typeid(std::remove_cvref_t<T>)));
        }

        template <typename T>
        bool DrawRegisteredInspectorField(const char* _label, const char* _idSuffix, T& _value)
        {
            auto drawerIt = m_inspectorFieldDrawers.find(std::type_index(typeid(std::remove_cvref_t<T>)));
            if (drawerIt == m_inspectorFieldDrawers.end())
                return false;

            drawerIt->second(*this, _label, _idSuffix, static_cast<void*>(&_value));
            return true;
        }
    private:
        using InspectorFieldDrawer = std::function<void(Editor&, const char*, const char*, void*)>;

        void DrawMainDockspace();
        void ApplyInternalSceneCamera(float _deltaTime);
        void DrawSceneView();
        void DrawGameView();
        void DrawSceneViewGizmo();
        void EnsureGameRenderTarget(int _width, int _height);
        void EnsureGamePickingRenderTarget(int _width, int _height);
        void EnsurePlayRenderTarget(int _width, int _height);
        void DestroyGameRenderTarget();
        void DestroyGamePickingRenderTarget();
        void DestroyPlayRenderTarget();
        void DrawInspectorPanel(bool _refresh);
        void DrawAddComponentDropDown(bool _refresh);
        //void DrawSystemPanel();
        bool IsDescendantOf(Canis::Entity* _parent, Canis::Entity* _potentialChild);
        void DrawHierarchyNode(Canis::Entity* _entity, std::vector<Canis::Entity*>& _entities,bool& _refresh);
        bool DrawHierarchyPanel();
        bool DrawHierarchyElement(int _index);
        void DrawEnvironment();
        void DrawAssetsPanel();
        void DrawDirectoryRecursive(const std::string &_dirPath);
        void DrawScriptsPanel();
        void DrawScriptDirectoryRecursive(const std::filesystem::path &_includeRoot, const std::filesystem::path &_currentDir, const std::filesystem::path &_sourceRoot);
        void CommitAssetRename();
        bool DrawMaterialAssetInspector(const std::string &_materialPath);
        bool DrawSkyboxAssetInspector(const std::string &_skyboxPath);
        bool DrawPostProcessAssetInspector(const std::string &_postProcessPath);
        void DrawProjectSettings();
        void DrawSystemPanel();
        void DrawEditorPanel();
        void ApplyEditorTheme(int _theme);
        void RefreshEditorFontOptions();
        void ApplyEditorFont(const std::string &_fontPath);
        void QueueEditorFontApply(const std::string &_fontPath, bool _saveConfig);
        void DrawReloadBuildPopup();
        void FinalizeReloadBuildIfReady();

        void SelectSprite2D();
        void SelectModel3D();
        void DrawBoundingBox(Camera2D *_camera2D);
        void DrawSelectionMouseDebug(Camera2D *_camera2D);

        //bool IsDescendantOf(Entity _potentialAncestor, Entity _entity);

        //SceneManager& GetSceneManager();

        // this should be a seperate system that runs after editor draw
        // that can take elements to draw queue them then draw at the end of a frame
        enum DebugDraw
        {
            NONE,
            RECT,
        };

        enum SceneCameraMode
        {
            SCENE_CAMERA_3D = 0,
            SCENE_CAMERA_2D = 1,
        };

        Scene *m_scene;
        App *m_app;
        Window* m_window;
        GameCodeObject* m_gameSharedLib;
        int m_index = 0;
        bool m_forceRefresh = false;
        EditorMode m_mode = EditorMode::EDIT;
        DebugDraw m_debugDraw = DebugDraw::NONE;
        SceneCameraMode m_sceneCameraMode = SceneCameraMode::SCENE_CAMERA_3D;
        GuizmoMode m_guizmoMode = GuizmoMode::WORLD;
        std::vector<std::string> m_assetPaths = {};
        Vector3 m_editorCamera3DPosition = Vector3(0.0f, 2.0f, 8.0f);
        float m_editorCamera3DYaw = -90.0f;
        float m_editorCamera3DPitch = -12.0f;
        float m_editorCamera3DFovDegrees = 60.0f;
        float m_editorCamera3DMoveSpeed = 8.0f;
        float m_editorCamera3DLookSensitivity = 0.12f;
        Vector2 m_editorCamera2DPosition = Vector2(0.0f);
        float m_editorCamera2DScale = 1.0f;
        Vector2 m_selectionMouseWorld = Vector2(0.0f);

        // asset panel
        bool m_isRenamingAsset = false;
        bool m_focusAssetRenameInput = false;
        std::string m_renamingPath;
        char m_renameBuffer[256] = {};
        std::string m_selectedAssetPath = {};
        std::string m_selectedScriptPath = {};
        bool m_openScriptCreatePopup = false;
        bool m_focusScriptCreateNameInput = false;
        char m_scriptCreateNameBuffer[128] = {};
        std::string m_scriptCreateTargetDir = {};
        std::string m_scriptCreateError = {};
        int m_scriptCreateTypeSelection = 0;
        int m_editorThemeSelection = 0;
        int m_editorFontSelection = 0;
        float m_editorUiScale = 1.0f;
        float m_editorFontScale = 1.0f;
        std::vector<std::string> m_editorFontPaths = {};
        std::vector<std::string> m_editorFontLabels = {};
        bool m_editorFontApplyQueued = false;
        bool m_editorFontApplyShouldSaveConfig = false;
        std::string m_queuedEditorFontPath = {};
        std::thread m_reloadBuildThread = {};
        std::mutex m_reloadBuildMutex = {};
        std::string m_reloadBuildCommand = {};
        std::string m_reloadBuildOutput = {};
        std::string m_reloadBuildBackupPath = {};
        bool m_reloadBuildInProgress = false;
        bool m_reloadBuildFinished = false;
        bool m_reloadBuildSucceeded = false;
        bool m_reloadBuildAwaitingFinalize = false;
        bool m_showReloadBuildPopup = false;
        bool m_openReloadBuildPopup = false;
        int m_reloadBuildExitCode = -1;
        std::vector<UUID> m_hierarchyRootOrder = {};

        unsigned int m_gameFramebuffer = 0;
        unsigned int m_gameColorTexture = 0;
        unsigned int m_gameDepthRbo = 0;
        RenderTarget m_gameViewPostProcessTarget = {};
        unsigned int m_gamePickingFramebuffer = 0;
        unsigned int m_gamePickingColorTexture = 0;
        unsigned int m_gamePickingDepthRbo = 0;
        Matrix4 m_gameRenderProjection = Matrix4(1.0f);
        int m_gameViewportWidth = 0;
        int m_gameViewportHeight = 0;
        float m_gameViewportPosX = 0.0f;
        float m_gameViewportPosY = 0.0f;
        float m_gameViewportDrawWidth = 0.0f;
        float m_gameViewportDrawHeight = 0.0f;
        unsigned int m_gameViewportId = 0;
        bool m_gameViewHovered = false;
        int m_gameTextureWidth = 0;
        int m_gameTextureHeight = 0;
        int m_gamePickingTextureWidth = 0;
        int m_gamePickingTextureHeight = 0;

        unsigned int m_playFramebuffer = 0;
        unsigned int m_playColorTexture = 0;
        unsigned int m_playDepthRbo = 0;
        RenderTarget m_playViewPostProcessTarget = {};
        Matrix4 m_playRenderProjection = Matrix4(1.0f);
        int m_playViewportWidth = 0;
        int m_playViewportHeight = 0;
        float m_playViewportPosX = 0.0f;
        float m_playViewportPosY = 0.0f;
        float m_playViewportDrawWidth = 0.0f;
        float m_playViewportDrawHeight = 0.0f;
        int m_playTextureWidth = 0;
        int m_playTextureHeight = 0;

        unsigned int m_gameInputWindowID = 0;
        std::unordered_map<std::type_index, InspectorFieldDrawer> m_inspectorFieldDrawers = {};
        int m_addComponentSelection = 0;
        std::string m_addComponentSearch = {};
        bool m_focusAddComponentSearch = false;
        bool m_stopPlayModeRequested = false;
    };
}
