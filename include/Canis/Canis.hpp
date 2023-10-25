#pragma once

namespace Canis
{
    struct ProjectConfig
    {
        bool fullscreen = false;
        int width = 720;
        int heigth = 1280;
        bool overrideSeed = false;
        unsigned int seed = 0;
    };

    ProjectConfig& GetProjectConfig();

    int Init();
} // end of Canis namespace
