#pragma once

namespace Canis
{
    struct ProjectConfig
    {
        bool fullscreen = false;
        int width = 1280;
        int heigth = 800;
        bool overrideSeed = false;
        unsigned int seed = 0;
    };

    ProjectConfig& GetProjectConfig();

    int Init();
} // end of Canis namespace
