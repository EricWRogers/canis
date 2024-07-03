#include <Canis/Canis.hpp>
#include <Canis/Debug.hpp>
#include <SDL.h>

#include <fstream>

namespace Canis
{
    ProjectConfig& GetProjectConfig()
    {
        static ProjectConfig projectConfig = {};
        return projectConfig;
    }

    bool SaveProjectConfig()
    {
        ProjectConfig projectConfig = GetProjectConfig();

        SDL_RWops *file = SDL_RWFromFile("assets/project.canis", "w+");

        if (file == nullptr)
        {
            Canis::Warning("Fail to open project config file for saving.");
            return false;
        }

        std::map<std::string,std::string> data = {};

        data["fullscreen"] = (projectConfig.fullscreen) ? "true" : "false";
        data["width"] = std::to_string(projectConfig.width);
        data["height"] = std::to_string(projectConfig.heigth);
        data["use_frame_limit"] = (projectConfig.useFrameLimit) ? "true" : "false";
        data["frame_limit"] = std::to_string(projectConfig.frameLimit);
        data["override_seed"] = (projectConfig.overrideSeed) ? "true" : "false";
        data["seed"] = std::to_string(projectConfig.seed);
        data["volume"] = std::to_string(projectConfig.volume);
        data["log"] = (projectConfig.log) ? "true" : "false";
        data["editor"] = (projectConfig.editor) ? "true" : "false";
        data["vsync"] = (projectConfig.vsync) ? "true" : "false";

        for(const auto& pair : data)
        {
            std::string line = pair.first + " " + pair.second + "\n";
            SDL_RWwrite(file, line.c_str(), 1, line.size());
        }

        SDL_RWclose(file);

        return true;
    }

    int Init()
    {
        SDL_Init(SDL_INIT_EVERYTHING);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

        // load project.canis
        std::ifstream file;
        file.open("assets/project.canis");

        if (!file.is_open()) return 1;

        std::string word;
        int wholeNumber = 0;
        unsigned int unsignedWholeNumber = 0;
        float fNumber = 0.0f;
        while(file >> word) {
            if (word == "fullscreen"){
                if (file >> word) {
                    if (word == "true")
                        GetProjectConfig().fullscreen = true;
                    else
                        GetProjectConfig().fullscreen = false;
                    
                    continue;
                }
            }
            if (word == "width") {
                if (file >> wholeNumber) {
                    GetProjectConfig().width = wholeNumber;
                    continue;
                }
            }
            if (word == "heigth") {
                if (file >> wholeNumber) {
                    GetProjectConfig().heigth = wholeNumber;
                    continue;
                }
            }
            if(word == "use_frame_limit")
            {
                if(file >> word)
                {
                    GetProjectConfig().useFrameLimit = (word == "true");
                    continue;
                }
            }
            if(word == "frame_limit")
            {
                if(file >> wholeNumber)
                {
                    GetProjectConfig().frameLimit = wholeNumber;
                    continue;
                }
            }
            if(word == "override_seed")
            {
                if(file >> word)
                {
                    GetProjectConfig().overrideSeed = (word == "true");
                    continue;
                }
            }
            if(word == "seed")
            {
                if(file >> unsignedWholeNumber)
                {
                    GetProjectConfig().seed = unsignedWholeNumber;
                    continue;
                }
            }
            if (word == "log")
            {
                if (file >> word)
                {
                    GetProjectConfig().log = (word == "true");
                    continue;
                }
            }
            if (word == "editor")
            {
                if (file >> word)
                {
                    GetProjectConfig().editor = (word == "true");
                    continue;
                }
            }
        }

        file.close();
        
        return 0;
    }
} // end of Canis namespace