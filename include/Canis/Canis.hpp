#pragma once
#ifdef __APPLE__
#include <string>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace Canis
{
    struct ProjectConfig
    {
        bool fullscreen = false;
        bool borderless = false;
        bool resizeable = false;
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
        bool logToFile = false;
        bool editor = false;
        bool vsync = false;
    };

    ProjectConfig& GetProjectConfig();
    bool SaveProjectConfig();

    int Init();

static std::string GetResourcesPath() {
#ifdef __APPLE__
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
    char path[PATH_MAX];
    if (CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8 *)path, PATH_MAX)) {
        CFRelease(resourcesURL);
        return std::string(path) + "/";
    }
    CFRelease(resourcesURL);
    return "./"; // fallback
#else
    return "./"; // non-macOS platforms
#endif
}

} // end of Canis namespace
