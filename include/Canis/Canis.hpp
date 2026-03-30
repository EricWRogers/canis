#pragma once
#include <Canis/UUID.hpp>

namespace Canis
{
    enum ProjectSyncMode : int
    {
        PROJECT_SYNC_ADAPTIVE = -1,
        PROJECT_SYNC_OFF = 0,
        PROJECT_SYNC_VSYNC = 1,
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
        //float volume = 1.0f;
        //float musicVolume = 1.0f;
        //float sfxVolume = 1.0f;
        //bool mute = false;
        //bool log = true;
        //bool logToFile = false;
        bool editor = true;
        int syncMode = PROJECT_SYNC_OFF;
        UUID iconUUID = UUID(0);
        int editorWindowWidth = 512;
        int editorWindowHeight = 512;
        int targetGameWidth = 512;
        int targetGameHeight = 512;
    };

    ProjectConfig& GetProjectConfig();
    extern bool SaveProjectConfig();

    extern int Init();

} // end of Canis namespace
