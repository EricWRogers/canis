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
        }

        file.close();
        
        return 0;
    }
} // end of Canis namespace