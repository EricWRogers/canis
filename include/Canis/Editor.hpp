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
        void RebuildPrefabInstance(Canis::Entity* _entity);
        void RebuildAllPrefabInstances();
        void ApplyPrefabInstanceOverrides(Canis::Entity* _entity);
        void NotifyAnimationPropertyEdited(
            Canis::Entity& _entity,
            const std::string &_componentName,
            const std::string &_propertyName,
            AnimationValueType _type,
            AnimationInterpolation _interpolation,
            const AnimationValue &_value);

        // inspector variables
        void InputEntity(const std::string& _name, Canis::Entity* &_variable);
        void InputEntity(const std::string& _name, const char* _idSuffix, Canis::Entity* &_variable);
        void InputAudioAsset(const std::string& _name, Canis::AudioAssetHandle &_variable);
        void InputAudioAsset(const std::string& _name, const char* _idSuffix, Canis::AudioAssetHandle &_variable);
        void InputAnimationClip(const std::string& _name, Canis::AnimationClip2DID &_variable);
        void InputAnimationClip(const std::string& _name, const char* _idSuffix, Canis::AnimationClip2DID &_variable);
        void InputAnimationClipAsset(const std::string& _name, Canis::AnimationClipAssetHandle &_variable);
        void InputAnimationClipAsset(const std::string& _name, const char* _idSuffix, Canis::AnimationClipAssetHandle &_variable);
        void InputAnimatorControllerAsset(const std::string& _name, Canis::AnimatorControllerAssetHandle &_variable);
        void InputAnimatorControllerAsset(const std::string& _name, const char* _idSuffix, Canis::AnimatorControllerAssetHandle &_variable);
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

        struct AnimationRestoreBinding
        {
            std::string path = "";
            std::string component = "";
            std::string property = "";
            AnimationValue value = {};
        };

        struct SceneHistoryState
        {
            std::string sceneYaml = "";
            UUID selectedEntityUUID = UUID(0);
            std::vector<UUID> hierarchyRootOrder = {};
        };

        struct PendingHotReloadAsset
        {
            std::filesystem::file_time_type writeTime = {};
            float debounceSeconds = 0.0f;
        };

        struct ModelMaterialExportDialogState
        {
            std::mutex mutex = {};
            std::string modelPath = {};
            std::string defaultLocation = {};
            std::string selectedFolder = {};
            std::string error = {};
            bool active = false;
            bool pending = false;
            bool canceled = false;
        };

        void DrawMainDockspace();
        void ApplyInternalSceneCamera(float _deltaTime);
        void DrawSceneView();
        void DrawGameView();
        void DrawSceneViewGizmo();
        void DrawEditorWindowMenu();
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
        bool DrawAssetPreviewCard(
            const std::string &_assetPath,
            const std::string &_displayName,
            MetaFileAsset *_meta,
            bool _selected,
            float _cardWidth,
            float _cardHeight);
        bool GetMaterialPreviewTexture(const std::string &_materialPath, unsigned int &_textureId);
        bool GetScenePreviewTexture(
            const std::string &_scenePath,
            unsigned int &_textureId,
            int &_width,
            int &_height);
        void DestroyAssetPreviewCache();
        void CacheSceneCameraFrameIfNeeded();
        void DrawScriptsPanel();
        void DrawScriptDirectoryRecursive(const std::filesystem::path &_includeRoot, const std::filesystem::path &_currentDir, const std::filesystem::path &_sourceRoot);
        void CommitAssetRename();
        bool DrawModelAssetInspector(const std::string &_modelPath);
        bool DrawMaterialAssetInspector(const std::string &_materialPath);
        bool DrawSkyboxAssetInspector(const std::string &_skyboxPath);
        bool DrawPostProcessAssetInspector(const std::string &_postProcessPath);
        bool DrawAnimationClipAssetInspector(const std::string &_animationClipPath);
        bool DrawAnimatorControllerAssetInspector(const std::string &_animatorControllerPath);
        bool DrawShaderGraphAssetInspector(const std::string &_shaderGraphPath);
        void DrawAnimationWindow(float _deltaTime);
        void DrawAnimatorWindow();
        std::string ResolveRememberedAnimationClipPath() const;
        void RememberLastAnimationClipAssetPath(const std::string &_path);
        void ClearRememberedAnimationClipAssetPathIfMatches(const std::string &_path);
        void DrawShaderGraphWindow();
        void DrawProjectSettings();
        void DrawSystemPanel();
        void DrawConsolePanel();
        void DrawEditorPanel();
        std::string ResolveRememberedShaderGraphPath() const;
        void RememberLastShaderGraphAssetPath(const std::string &_path);
        void ClearRememberedShaderGraphAssetPathIfMatches(const std::string &_path);
        void ApplyEditorTheme(int _theme);
        void RefreshEditorFontOptions();
        void ApplyEditorFont(const std::string &_fontPath);
        void QueueEditorFontApply(const std::string &_fontPath, bool _saveConfig);
        void DrawReloadBuildPopup();
        void FinalizeReloadBuildIfReady();
        void FrameEntityInScene(Canis::Entity *_entity);
        Canis::Entity* RebuildPrefabInstanceNow(Canis::Entity* _entity, bool _focusSelection);
        void ProcessQueuedPrefabRebuilds();
        void RequestHierarchyReveal(Canis::Entity *_entity);
        void ResetSceneHistory();
        void BeginSceneHistoryFrame();
        void EndSceneHistoryFrame();
        void FlushSceneHistoryPendingChange();
        void CommitSceneHistoryPendingChange();
        void CommitSceneHistoryImmediateChange(const SceneHistoryState &_beforeState);
        void UndoSceneEdit();
        void RedoSceneEdit();
        bool CanUndoSceneEdit() const;
        bool CanRedoSceneEdit() const;
        bool HandleSceneHistoryShortcuts(float &_hotKeyCoolDown, float _hotKeyReset);
        bool CanTrackSceneHistory() const;
        bool IsSceneHistoryEditInProgress() const;
        SceneHistoryState CaptureSceneHistoryState() const;
        bool SceneHistoryContentEquals(const SceneHistoryState &_left, const SceneHistoryState &_right) const;
        void PushSceneUndoState(const SceneHistoryState &_state);
        void RestoreSceneHistoryState(const SceneHistoryState &_state);
        void ReleasePlayMouseCapture();
        void UpdatePlayMouseCapture();
        void PrimeAssetHotReloadState();
        void PollAssetHotReload(float _deltaTime);
        void RequestModelMaterialExport(const std::string &_modelPath);
        void ProcessModelMaterialExportDialog();
        void ResetVertexSnapDrag();
        bool TryApplyVertexSnap(Canis::Entity *_selected, const Matrix4 &_selectedWorldMatrix, Vector3 &_worldPosition, const Vector3 &_dragDelta);

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
        std::string m_assetSearch = {};
        struct MaterialPreviewCacheEntry
        {
            RenderTarget renderTarget = {};
            std::filesystem::file_time_type writeTime = std::filesystem::file_time_type::min();
            double lastRenderSeconds = -1.0;
            double lastSourceCheckSeconds = -1.0;
            bool diskCacheChecked = false;
        };
        std::unordered_map<std::string, MaterialPreviewCacheEntry> m_materialPreviewCache = {};
        struct ScenePreviewCacheEntry
        {
            GLTexture texture = {};
            std::filesystem::file_time_type writeTime = std::filesystem::file_time_type::min();
        };
        std::unordered_map<std::string, ScenePreviewCacheEntry> m_scenePreviewCache = {};
        double m_lastSceneCameraCacheSeconds = -1.0;
        std::string m_lastSceneCameraCachePath = {};
        std::string m_selectedScriptPath = {};
        std::string m_animationClipStatePath = {};
        std::string m_animatorStatePath = {};
        int m_animationSelectedTrack = -1;
        int m_animationSelectedEvent = -1;
        int m_animatorSelectedState = -1;
        int m_animatorSelectedTransition = -1;
        int m_animationFirstFrame = 0;
        bool m_animationExpanded = true;
        bool m_animationPreviewEnabled = true;
        bool m_animationRecordEnabled = false;
        bool m_animationPlaying = false;
        float m_animationTime = 0.0f;
        UUID m_animationTargetUUID = UUID(0);
        UUID m_animationPreviewTargetUUID = UUID(0);
        std::string m_animationPreviewClipPath = {};
        std::vector<AnimationRestoreBinding> m_animationRestoreBindings = {};
        std::string m_animationAddPropertySearch = {};
        std::string m_shaderGraphStatePath = {};
        int m_shaderGraphSelectedNodeId = -1;
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
        bool m_showScenePanel = true;
        bool m_showGamePanel = true;
        bool m_showHierarchyPanel = true;
        bool m_showInspectorPanel = true;
        bool m_showEnvironmentPanel = true;
        bool m_showSystemsPanel = true;
        bool m_showAssetsPanel = true;
        bool m_showScriptsPanel = true;
        bool m_showAnimationPanel = true;
        bool m_showAnimatorPanel = true;
        bool m_showShaderGraphPanel = true;
        bool m_showProjectSettingsPanel = true;
        bool m_showConsolePanel = true;
        bool m_playViewHovered = false;
        bool m_playMouseCaptured = false;
        bool m_sceneViewClicked = false;
        bool m_vertexSnappingEnabled = false;
        bool m_vertexSnapDragActive = false;
        int m_vertexSnapAxesMask = 0;
        Vector3 m_vertexSnapDragDirection = Vector3(0.0f);
        float m_assetHotReloadPollTimer = 0.0f;
        bool m_hotReloadAssets = true;
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
        bool m_reloadBuildAutoCloseOnSuccess = false;
        int m_reloadBuildExitCode = -1;
        std::vector<UUID> m_hierarchyRootOrder = {};
        std::vector<UUID> m_queuedPrefabInstanceRebuilds = {};
        bool m_rebuildAllPrefabInstancesRequested = false;
        UUID m_hierarchyRevealTargetUUID = UUID(0);
        std::vector<UUID> m_hierarchyRevealPath = {};
        std::vector<SceneHistoryState> m_sceneUndoStack = {};
        std::vector<SceneHistoryState> m_sceneRedoStack = {};
        SceneHistoryState m_sceneHistoryCurrentState = {};
        SceneHistoryState m_sceneHistoryPendingBeforeState = {};
        bool m_hasSceneHistoryCurrentState = false;
        bool m_hasSceneHistoryPendingBeforeState = false;
        bool m_sceneHistoryEditWasActive = false;
        bool m_sceneHistoryRestoring = false;
        std::unordered_map<std::string, std::filesystem::file_time_type> m_assetHotReloadWriteTimes = {};
        std::unordered_map<std::string, PendingHotReloadAsset> m_pendingHotReloadAssets = {};
        ModelMaterialExportDialogState m_modelMaterialExportDialog = {};
        std::string m_consoleSearch = {};
        bool m_consoleShowLogs = true;
        bool m_consoleShowWarnings = true;
        bool m_consoleShowErrors = true;
        bool m_consoleShowFatal = true;
        bool m_consoleAutoScroll = true;
        bool m_consoleCollapseDuplicates = false;
        uint64_t m_consoleLastEntryId = 0u;

        unsigned int m_gameFramebuffer = 0;
        unsigned int m_gameColorTexture = 0;
        unsigned int m_gameDepthRbo = 0;
        RenderTarget m_gameViewPostProcessTarget = {};
        RenderTarget m_sceneThumbnailTarget = {};
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

        unsigned int m_mainDockspaceID = 0;
        unsigned int m_gameInputWindowID = 0;
        std::unordered_map<std::type_index, InspectorFieldDrawer> m_inspectorFieldDrawers = {};
        int m_addComponentSelection = 0;
        std::string m_addComponentSearch = {};
        bool m_focusAddComponentSearch = false;
        bool m_stopPlayModeRequested = false;
    };
}
