#include <Canis/Canis.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/IOManager.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace Canis
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* kProjectConfigPath = "project_settings/project.canis";
        constexpr const char* kLegacyProjectConfigPath = "project.canis";
        constexpr const char* kEditorConfigPath = "user_settings/editor.conf";
        constexpr const char* kDefaultEditorConfigPath = "project_settings/editor.conf";
        bool g_editorRuntimeEnabled = true;

        int NormalizeProjectSyncMode(int _value)
        {
            if (_value == PROJECT_SYNC_ADAPTIVE ||
                _value == PROJECT_SYNC_OFF ||
                _value == PROJECT_SYNC_VSYNC)
            {
                return _value;
            }

            return PROJECT_SYNC_OFF;
        }

        int NormalizeEditorThemeMode(int _value)
        {
            if (_value == EDITOR_THEME_DARK || _value == EDITOR_THEME_LIGHT)
                return _value;

            return EDITOR_THEME_DARK;
        }

        float NormalizeEditorFontScale(float _value)
        {
            if (!std::isfinite(_value))
                return 1.0f;

            return std::clamp(_value, 0.5f, 2.5f);
        }

        float NormalizeSceneCameraPitch(float _value)
        {
            if (!std::isfinite(_value))
                return -12.0f;

            return std::clamp(_value, -89.0f, 89.0f);
        }

        float NormalizeSceneCameraFov(float _value)
        {
            if (!std::isfinite(_value))
                return 60.0f;

            return std::clamp(_value, 1.0f, 179.0f);
        }

        float NormalizeSceneCamera2DScale(float _value)
        {
            if (!std::isfinite(_value))
                return 1.0f;

            return std::clamp(_value, 0.01f, 100.0f);
        }

        Vector2 NormalizeVector2(Vector2 _value, Vector2 _fallback)
        {
            if (!std::isfinite(_value.x) || !std::isfinite(_value.y))
                return _fallback;

            return _value;
        }

        Vector3 NormalizeVector3(Vector3 _value, Vector3 _fallback)
        {
            if (!std::isfinite(_value.x) || !std::isfinite(_value.y) || !std::isfinite(_value.z))
                return _fallback;

            return _value;
        }

        bool SceneAssetHandlesMatch(const SceneAssetHandle &_left, const SceneAssetHandle &_right)
        {
            if (_left.uuid != UUID(0) && _right.uuid != UUID(0))
                return _left.uuid == _right.uuid;

            if (!_left.path.empty() && !_right.path.empty())
                return _left.path == _right.path;

            return false;
        }

        EditorSceneCameraConfig MakeSceneCameraConfigFromEditorFallback(const EditorConfig &_editorConfig)
        {
            EditorSceneCameraConfig sceneCamera = {};
            sceneCamera.scene = _editorConfig.lastEditorScene;
            sceneCamera.sceneCameraMode = (_editorConfig.sceneCameraMode == 0 || _editorConfig.sceneCameraMode == 1)
                ? _editorConfig.sceneCameraMode : 0;
            sceneCamera.sceneCamera3DPosition = NormalizeVector3(_editorConfig.sceneCamera3DPosition, Vector3(0.0f, 2.0f, 8.0f));
            sceneCamera.sceneCamera3DYaw = std::isfinite(_editorConfig.sceneCamera3DYaw) ? _editorConfig.sceneCamera3DYaw : -90.0f;
            sceneCamera.sceneCamera3DPitch = NormalizeSceneCameraPitch(_editorConfig.sceneCamera3DPitch);
            sceneCamera.sceneCamera3DFovDegrees = NormalizeSceneCameraFov(_editorConfig.sceneCamera3DFovDegrees);
            sceneCamera.sceneCamera2DPosition = NormalizeVector2(_editorConfig.sceneCamera2DPosition, Vector2(0.0f));
            sceneCamera.sceneCamera2DScale = NormalizeSceneCamera2DScale(_editorConfig.sceneCamera2DScale);
            return sceneCamera;
        }

        bool HasSceneCameraConfigForScene(const std::vector<EditorSceneCameraConfig> &_sceneCameras, const SceneAssetHandle &_scene)
        {
            for (const EditorSceneCameraConfig &sceneCamera : _sceneCameras)
            {
                if (SceneAssetHandlesMatch(sceneCamera.scene, _scene))
                    return true;
            }

            return false;
        }

        YAML::Node EncodeSceneCameraConfig(const EditorSceneCameraConfig &_sceneCamera)
        {
            YAML::Node node;
            node["scene"] = _sceneCamera.scene;
            node["sceneCameraMode"] = (_sceneCamera.sceneCameraMode == 0 || _sceneCamera.sceneCameraMode == 1)
                ? _sceneCamera.sceneCameraMode : 0;
            node["sceneCamera3DPosition"] = NormalizeVector3(_sceneCamera.sceneCamera3DPosition, Vector3(0.0f, 2.0f, 8.0f));
            node["sceneCamera3DYaw"] = std::isfinite(_sceneCamera.sceneCamera3DYaw) ? _sceneCamera.sceneCamera3DYaw : -90.0f;
            node["sceneCamera3DPitch"] = NormalizeSceneCameraPitch(_sceneCamera.sceneCamera3DPitch);
            node["sceneCamera3DFovDegrees"] = NormalizeSceneCameraFov(_sceneCamera.sceneCamera3DFovDegrees);
            node["sceneCamera2DPosition"] = NormalizeVector2(_sceneCamera.sceneCamera2DPosition, Vector2(0.0f));
            node["sceneCamera2DScale"] = NormalizeSceneCamera2DScale(_sceneCamera.sceneCamera2DScale);
            return node;
        }

        EditorSceneCameraConfig DecodeSceneCameraConfig(const YAML::Node &_node, const EditorConfig &_fallbackConfig)
        {
            EditorSceneCameraConfig sceneCamera = MakeSceneCameraConfigFromEditorFallback(_fallbackConfig);
            sceneCamera.scene = _node["scene"].as<SceneAssetHandle>(sceneCamera.scene);
            sceneCamera.sceneCameraMode = _node["sceneCameraMode"].as<int>(sceneCamera.sceneCameraMode);
            if (sceneCamera.sceneCameraMode < 0 || sceneCamera.sceneCameraMode > 1)
                sceneCamera.sceneCameraMode = 0;
            sceneCamera.sceneCamera3DPosition = NormalizeVector3(
                _node["sceneCamera3DPosition"].as<Vector3>(sceneCamera.sceneCamera3DPosition),
                Vector3(0.0f, 2.0f, 8.0f));
            sceneCamera.sceneCamera3DYaw = _node["sceneCamera3DYaw"].as<float>(sceneCamera.sceneCamera3DYaw);
            if (!std::isfinite(sceneCamera.sceneCamera3DYaw))
                sceneCamera.sceneCamera3DYaw = -90.0f;
            sceneCamera.sceneCamera3DPitch = NormalizeSceneCameraPitch(
                _node["sceneCamera3DPitch"].as<float>(sceneCamera.sceneCamera3DPitch));
            sceneCamera.sceneCamera3DFovDegrees = NormalizeSceneCameraFov(
                _node["sceneCamera3DFovDegrees"].as<float>(sceneCamera.sceneCamera3DFovDegrees));
            sceneCamera.sceneCamera2DPosition = NormalizeVector2(
                _node["sceneCamera2DPosition"].as<Vector2>(sceneCamera.sceneCamera2DPosition),
                Vector2(0.0f));
            sceneCamera.sceneCamera2DScale = NormalizeSceneCamera2DScale(
                _node["sceneCamera2DScale"].as<float>(sceneCamera.sceneCamera2DScale));
            return sceneCamera;
        }

        float NormalizeVolume(float _value)
        {
            if (!std::isfinite(_value))
                return 1.0f;

            return std::clamp(_value, 0.0f, 1.0f);
        }

        std::string GetReadableProjectConfigPath()
        {
            if (FileExists(kProjectConfigPath))
                return kProjectConfigPath;

            if (FileExists(kLegacyProjectConfigPath))
                return kLegacyProjectConfigPath;

            return kProjectConfigPath;
        }

        std::string GetReadableEditorConfigPath()
        {
            if (FileExists(kEditorConfigPath))
                return kEditorConfigPath;

            if (FileExists(kDefaultEditorConfigPath))
                return kDefaultEditorConfigPath;

            return kEditorConfigPath;
        }
    } // namespace

    ProjectConfig& GetProjectConfig()
    {
        static ProjectConfig projectConfig = {};
        return projectConfig;
    }

    EditorConfig& GetEditorConfig()
    {
        static EditorConfig editorConfig = {};
        return editorConfig;
    }

    bool IsEditorRuntimeEnabled()
    {
        return g_editorRuntimeEnabled;
    }

    void SetEditorRuntimeEnabled(bool _enabled)
    {
        g_editorRuntimeEnabled = _enabled;
    }

    bool SaveProjectConfig()
    {
        ProjectConfig projectConfig = GetProjectConfig();

        YAML::Node node;

        node["useFrameLimit"] = projectConfig.useFrameLimit;
        node["frameLimit"] = projectConfig.frameLimit;
        node["frameLimitEditor"] = projectConfig.frameLimitEditor;
        node["overrideSeed"] = projectConfig.overrideSeed;
        node["seed"] = projectConfig.seed;
        node["volume"] = NormalizeVolume(projectConfig.volume);
        node["musicVolume"] = NormalizeVolume(projectConfig.musicVolume);
        node["sfxVolume"] = NormalizeVolume(projectConfig.sfxVolume);
        node["mute"] = projectConfig.mute;
        node["editor"] = projectConfig.editor;
        node["syncMode"] = NormalizeProjectSyncMode(projectConfig.syncMode);
        node["iconUUID"] = std::to_string(projectConfig.iconUUID);
        node["launchScene"] = projectConfig.launchScene;
        node["launchExecutablePath"] = projectConfig.launchExecutablePath;
        node["launchWorkingDirectory"] = projectConfig.launchWorkingDirectory;
        node["launchArguments"] = projectConfig.launchArguments;
        node["editorWindowWidth"] = projectConfig.editorWindowWidth;
        node["editorWindowHeight"] = projectConfig.editorWindowHeight;
        node["targetGameWidth"] = projectConfig.targetGameWidth;
        node["targetGameHeight"] = projectConfig.targetGameHeight;

        std::error_code ec;
        fs::create_directories(fs::path(kProjectConfigPath).parent_path(), ec);
        if (ec)
        {
            Debug::Error("Failed to create project settings directory for %s", kProjectConfigPath);
            return false;
        }

        std::ofstream fout(kProjectConfigPath);
        if (!fout.is_open())
        {
            Debug::Error("Failed to save project config to %s", kProjectConfigPath);
            return false;
        }

        fout << node;

        return fout.good();
    }

    bool SaveEditorConfig()
    {
        const EditorConfig editorConfig = GetEditorConfig();

        YAML::Node node;
        node["lastEditorScene"] = editorConfig.lastEditorScene;
        node["lastShaderGraph"] = editorConfig.lastShaderGraph;
        node["lastAnimationClip"] = editorConfig.lastAnimationClip;
        node["theme"] = NormalizeEditorThemeMode(editorConfig.theme);
        node["fontPath"] = editorConfig.fontPath;
        node["fontScale"] = NormalizeEditorFontScale(editorConfig.fontScale);
        node["reloadBuildAutoCloseOnSuccess"] = editorConfig.reloadBuildAutoCloseOnSuccess;

        std::vector<EditorSceneCameraConfig> sceneCamerasToSave = editorConfig.sceneCameras;
        if (sceneCamerasToSave.empty() && !editorConfig.lastEditorScene.Empty())
            sceneCamerasToSave.push_back(MakeSceneCameraConfigFromEditorFallback(editorConfig));

        YAML::Node sceneCameras(YAML::NodeType::Sequence);
        for (const EditorSceneCameraConfig &sceneCamera : sceneCamerasToSave)
        {
            if (sceneCamera.scene.Empty())
                continue;

            sceneCameras.push_back(EncodeSceneCameraConfig(sceneCamera));
        }
        node["sceneCameras"] = sceneCameras;

        std::error_code ec;
        fs::create_directories(fs::path(kEditorConfigPath).parent_path(), ec);
        if (ec)
        {
            Debug::Error("Failed to create editor settings directory for %s", kEditorConfigPath);
            return false;
        }

        std::ofstream fout(kEditorConfigPath);
        if (!fout.is_open())
        {
            Debug::Error("Failed to save editor config to %s", kEditorConfigPath);
            return false;
        }

        fout << node;
        return fout.good();
    }

    int Init()
    {
        //SDL_Init(SDL_INIT_EVERYTHING);

        //SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        // load project.canis
        ProjectConfig& projectConfig = GetProjectConfig();
        EditorConfig& editorConfig = GetEditorConfig();

        YAML::Node node;
        const std::string projectConfigPath = GetReadableProjectConfigPath();
        if (FileExists(projectConfigPath.c_str()))
            node = YAML::LoadFile(projectConfigPath);

        YAML::Node editorNode;
        const std::string editorConfigPath = GetReadableEditorConfigPath();
        if (FileExists(editorConfigPath.c_str()))
            editorNode = YAML::LoadFile(editorConfigPath);

        projectConfig.useFrameLimit = node["useFrameLimit"].as<bool>(projectConfig.useFrameLimit);
        projectConfig.frameLimit = node["frameLimit"].as<float>(projectConfig.frameLimit);
        projectConfig.frameLimitEditor = node["frameLimitEditor"].as<float>(projectConfig.frameLimitEditor);
        projectConfig.overrideSeed = node["overrideSeed"].as<bool>(projectConfig.overrideSeed);
        projectConfig.seed = node["useFrameLimit"].as<unsigned int>(projectConfig.seed);
        projectConfig.volume = NormalizeVolume(node["volume"].as<float>(node["masterVolume"].as<float>(projectConfig.volume)));
        projectConfig.musicVolume = NormalizeVolume(node["musicVolume"].as<float>(projectConfig.musicVolume));
        projectConfig.sfxVolume = NormalizeVolume(node["sfxVolume"].as<float>(projectConfig.sfxVolume));
        projectConfig.mute = node["mute"].as<bool>(projectConfig.mute);
        projectConfig.editor = node["editor"].as<bool>(projectConfig.editor);
        projectConfig.syncMode = node["syncMode"].as<int>(projectConfig.syncMode);
        if (!node["syncMode"] && node["vsync"])
            projectConfig.syncMode = node["vsync"].as<bool>(false) ? PROJECT_SYNC_VSYNC : PROJECT_SYNC_OFF;
        projectConfig.syncMode = NormalizeProjectSyncMode(projectConfig.syncMode);
        projectConfig.iconUUID = node["iconUUID"].as<uint64_t>(projectConfig.iconUUID);
        projectConfig.launchScene = node["launchScene"].as<SceneAssetHandle>(projectConfig.launchScene);
        if (!node["launchScene"] && node["LaunchScene"])
            projectConfig.launchScene = node["LaunchScene"].as<SceneAssetHandle>(projectConfig.launchScene);
        projectConfig.launchExecutablePath = node["launchExecutablePath"].as<std::string>(projectConfig.launchExecutablePath);
        projectConfig.launchWorkingDirectory = node["launchWorkingDirectory"].as<std::string>(projectConfig.launchWorkingDirectory);
        projectConfig.launchArguments = node["launchArguments"].as<std::string>(projectConfig.launchArguments);
        projectConfig.editorWindowWidth = node["editorWindowWidth"].as<int>(projectConfig.editorWindowWidth);
        projectConfig.editorWindowHeight = node["editorWindowHeight"].as<int>(projectConfig.editorWindowHeight);
        projectConfig.targetGameWidth = node["targetGameWidth"].as<int>(projectConfig.targetGameWidth);
        projectConfig.targetGameHeight = node["targetGameHeight"].as<int>(projectConfig.targetGameHeight);

        SetEditorRuntimeEnabled(projectConfig.editor);

        editorConfig.lastEditorScene = editorNode["lastEditorScene"].as<SceneAssetHandle>(editorConfig.lastEditorScene);
        if (!editorNode["lastEditorScene"] && editorNode["LastEditorScene"])
            editorConfig.lastEditorScene = editorNode["LastEditorScene"].as<SceneAssetHandle>(editorConfig.lastEditorScene);
        editorConfig.lastShaderGraph = editorNode["lastShaderGraph"].as<ShaderGraphAssetHandle>(editorConfig.lastShaderGraph);
        if (!editorNode["lastShaderGraph"] && editorNode["LastShaderGraph"])
            editorConfig.lastShaderGraph = editorNode["LastShaderGraph"].as<ShaderGraphAssetHandle>(editorConfig.lastShaderGraph);
        editorConfig.lastAnimationClip = editorNode["lastAnimationClip"].as<AnimationClipAssetHandle>(editorConfig.lastAnimationClip);
        if (!editorNode["lastAnimationClip"] && editorNode["LastAnimationClip"])
            editorConfig.lastAnimationClip = editorNode["LastAnimationClip"].as<AnimationClipAssetHandle>(editorConfig.lastAnimationClip);
        editorConfig.theme = NormalizeEditorThemeMode(editorNode["theme"].as<int>(editorConfig.theme));
        editorConfig.fontPath = editorNode["fontPath"].as<std::string>(editorConfig.fontPath);
        editorConfig.fontScale = NormalizeEditorFontScale(editorNode["fontScale"].as<float>(editorConfig.fontScale));
        editorConfig.reloadBuildAutoCloseOnSuccess =
            editorNode["reloadBuildAutoCloseOnSuccess"].as<bool>(editorConfig.reloadBuildAutoCloseOnSuccess);
        editorConfig.sceneCameraMode = editorNode["sceneCameraMode"].as<int>(editorConfig.sceneCameraMode);
        if (editorConfig.sceneCameraMode < 0 || editorConfig.sceneCameraMode > 1)
            editorConfig.sceneCameraMode = 0;
        editorConfig.sceneCamera3DPosition = NormalizeVector3(
            editorNode["sceneCamera3DPosition"].as<Vector3>(editorConfig.sceneCamera3DPosition),
            Vector3(0.0f, 2.0f, 8.0f));
        editorConfig.sceneCamera3DYaw = editorNode["sceneCamera3DYaw"].as<float>(editorConfig.sceneCamera3DYaw);
        if (!std::isfinite(editorConfig.sceneCamera3DYaw))
            editorConfig.sceneCamera3DYaw = -90.0f;
        editorConfig.sceneCamera3DPitch = NormalizeSceneCameraPitch(
            editorNode["sceneCamera3DPitch"].as<float>(editorConfig.sceneCamera3DPitch));
        editorConfig.sceneCamera3DFovDegrees = NormalizeSceneCameraFov(
            editorNode["sceneCamera3DFovDegrees"].as<float>(editorConfig.sceneCamera3DFovDegrees));
        editorConfig.sceneCamera2DPosition = NormalizeVector2(
            editorNode["sceneCamera2DPosition"].as<Vector2>(editorConfig.sceneCamera2DPosition),
            Vector2(0.0f));
        editorConfig.sceneCamera2DScale = NormalizeSceneCamera2DScale(
            editorNode["sceneCamera2DScale"].as<float>(editorConfig.sceneCamera2DScale));

        editorConfig.sceneCameras.clear();
        if (YAML::Node sceneCamerasNode = editorNode["sceneCameras"])
        {
            if (sceneCamerasNode.IsSequence())
            {
                for (YAML::const_iterator it = sceneCamerasNode.begin(); it != sceneCamerasNode.end(); ++it)
                {
                    const YAML::Node sceneCameraNode = *it;
                    if (!sceneCameraNode || !sceneCameraNode.IsMap())
                        continue;

                    EditorSceneCameraConfig sceneCamera = DecodeSceneCameraConfig(sceneCameraNode, editorConfig);
                    if (!sceneCamera.scene.Empty() && !HasSceneCameraConfigForScene(editorConfig.sceneCameras, sceneCamera.scene))
                        editorConfig.sceneCameras.push_back(sceneCamera);
                }
            }
        }

        const bool hasLegacySceneCameraConfig =
            editorNode["sceneCameraMode"] ||
            editorNode["sceneCamera3DPosition"] ||
            editorNode["sceneCamera3DYaw"] ||
            editorNode["sceneCamera3DPitch"] ||
            editorNode["sceneCamera3DFovDegrees"] ||
            editorNode["sceneCamera2DPosition"] ||
            editorNode["sceneCamera2DScale"];
        if (hasLegacySceneCameraConfig &&
            !editorConfig.lastEditorScene.Empty() &&
            !HasSceneCameraConfigForScene(editorConfig.sceneCameras, editorConfig.lastEditorScene))
        {
            editorConfig.sceneCameras.push_back(MakeSceneCameraConfigFromEditorFallback(editorConfig));
        }

        // Backward compatibility with older project keys.
        if (!node["editorWindowWidth"] && node["windowWidth"])
            projectConfig.editorWindowWidth = node["windowWidth"].as<int>(projectConfig.editorWindowWidth);
        if (!node["editorWindowHeight"] && node["windowHeight"])
            projectConfig.editorWindowHeight = node["windowHeight"].as<int>(projectConfig.editorWindowHeight);
        if (!node["targetGameWidth"] && node["windowWidth"])
            projectConfig.targetGameWidth = node["windowWidth"].as<int>(projectConfig.targetGameWidth);
        if (!node["targetGameHeight"] && node["windowHeight"])
            projectConfig.targetGameHeight = node["windowHeight"].as<int>(projectConfig.targetGameHeight);

        if (projectConfig.editorWindowWidth < 320)
            projectConfig.editorWindowWidth = 320;
        if (projectConfig.editorWindowHeight < 240)
            projectConfig.editorWindowHeight = 240;
        if (projectConfig.targetGameWidth < 1)
            projectConfig.targetGameWidth = 1;
        if (projectConfig.targetGameHeight < 1)
            projectConfig.targetGameHeight = 1;
        
        
        return 0;
    }
} // end of Canis namespace
