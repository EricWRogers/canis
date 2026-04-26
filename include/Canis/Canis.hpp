#pragma once
#include <Canis/UUID.hpp>
#include <Canis/AssetHandle.hpp>
#include <string>

namespace Canis
{
    enum ProjectSyncMode : int
    {
        PROJECT_SYNC_ADAPTIVE = -1,
        PROJECT_SYNC_OFF = 0,
        PROJECT_SYNC_VSYNC = 1,
    };

    enum EditorThemeMode : int
    {
        EDITOR_THEME_DARK = 0,
        EDITOR_THEME_LIGHT = 1,
    };

    struct ProjectConfig
    {
        //bool fullscreen = false;
        //bool borderless = false;
        //bool resizeable = false;
        //int width = 1280;
        //int heigth = 720;
        bool useFrameLimit = false;
        int frameLimit = 120.0f;
        int frameLimitEditor = 120.0f;
        bool overrideSeed = false;
        unsigned int seed = 0;
        float volume = 1.0f;
        float musicVolume = 1.0f;
        float sfxVolume = 1.0f;
        bool mute = false;
        //bool log = true;
        //bool logToFile = false;
        bool editor = true;
        int syncMode = PROJECT_SYNC_OFF;
        UUID iconUUID = UUID(0);
        SceneAssetHandle launchScene = {};
        std::string launchExecutablePath = "";
        std::string launchWorkingDirectory = "";
        std::string launchArguments = "";
        int editorWindowWidth = 512;
        int editorWindowHeight = 512;
        int targetGameWidth = 512;
        int targetGameHeight = 512;
    };

    struct EditorConfig
    {
        SceneAssetHandle lastEditorScene = {};
        ShaderGraphAssetHandle lastShaderGraph = {};
        AnimationClipAssetHandle lastAnimationClip = {};
        int theme = EDITOR_THEME_DARK;
        std::string fontPath = "";
        float fontScale = 1.0f;
        bool reloadBuildAutoCloseOnSuccess = false;
    };

    ProjectConfig& GetProjectConfig();
    EditorConfig& GetEditorConfig();
    bool IsEditorRuntimeEnabled();
    void SetEditorRuntimeEnabled(bool _enabled);
    extern bool SaveProjectConfig();
    extern bool SaveEditorConfig();

    extern int Init();

} // end of Canis namespace
