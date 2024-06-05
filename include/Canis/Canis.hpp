#pragma once

namespace Canis
{
    struct ProjectConfig
    {
        bool fullscreen = false;
        int width = 1280;
        int heigth = 720;
        bool useFrameLimit = false;
        int frameLimit = 60;
        bool overrideSeed = false;
        unsigned int seed = 0;
        float volume = 1.0f;
        float musicVolume = 1.0f;
        float sfxVolume = 1.0f;
        bool mute = false;
        bool log = true;
        bool editor = false;
    };

    ProjectConfig& GetProjectConfig();

    int Init();
} // end of Canis namespace
