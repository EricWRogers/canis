#include <Canis/Canis.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/IOManager.hpp>

#include <fstream>

namespace Canis
{
    namespace
    {
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
    } // namespace

    ProjectConfig& GetProjectConfig()
    {
        static ProjectConfig projectConfig = {};
        return projectConfig;
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
        node["editor"] = projectConfig.editor;
        node["syncMode"] = NormalizeProjectSyncMode(projectConfig.syncMode);
        node["iconUUID"] = std::to_string(projectConfig.iconUUID);
        node["editorWindowWidth"] = projectConfig.editorWindowWidth;
        node["editorWindowHeight"] = projectConfig.editorWindowHeight;
        node["targetGameWidth"] = projectConfig.targetGameWidth;
        node["targetGameHeight"] = projectConfig.targetGameHeight;

        std::ofstream fout("project.canis");
        fout << node;

        return true;
    }

    int Init()
    {
        //SDL_Init(SDL_INIT_EVERYTHING);

        //SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        // load project.canis
        ProjectConfig& projectConfig = GetProjectConfig();

        YAML::Node node;
        if (FileExists("project.canis"))
            node = YAML::LoadFile("project.canis");

        projectConfig.useFrameLimit = node["useFrameLimit"].as<bool>(projectConfig.useFrameLimit);
        projectConfig.frameLimit = node["frameLimit"].as<float>(projectConfig.frameLimit);
        projectConfig.frameLimitEditor = node["frameLimitEditor"].as<float>(projectConfig.frameLimitEditor);
        projectConfig.overrideSeed = node["overrideSeed"].as<bool>(projectConfig.overrideSeed);
        projectConfig.seed = node["useFrameLimit"].as<unsigned int>(projectConfig.seed);
        projectConfig.editor = node["editor"].as<bool>(projectConfig.editor);
        projectConfig.syncMode = node["syncMode"].as<int>(projectConfig.syncMode);
        if (!node["syncMode"] && node["vsync"])
            projectConfig.syncMode = node["vsync"].as<bool>(false) ? PROJECT_SYNC_VSYNC : PROJECT_SYNC_OFF;
        projectConfig.syncMode = NormalizeProjectSyncMode(projectConfig.syncMode);
        projectConfig.iconUUID = node["iconUUID"].as<uint64_t>(projectConfig.iconUUID);
        projectConfig.editorWindowWidth = node["editorWindowWidth"].as<int>(projectConfig.editorWindowWidth);
        projectConfig.editorWindowHeight = node["editorWindowHeight"].as<int>(projectConfig.editorWindowHeight);
        projectConfig.targetGameWidth = node["targetGameWidth"].as<int>(projectConfig.targetGameWidth);
        projectConfig.targetGameHeight = node["targetGameHeight"].as<int>(projectConfig.targetGameHeight);

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
