#pragma once

#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <string>

#include "Debug.h"

namespace Canis
{
    enum WindowFlags
    {
        INVISIBLE = 0x1,
        FULLSCREEN = 0x2,
        BORDERLESS = 0x4
    };

    class Window
    {
        public:
            Window();
            ~Window();

            int Spawn(std::string window_name, int screen_width, int screen_height, unsigned int current_flags);

            void SwapBuffer();

            int GetScreenWidth() { return screen_width; }
            int GetScreenHeight() { return screen_height; }
        
        private:
            SDL_Window *sdl_window;
            int screen_width, screen_height;
    };
} // end of Canis namespace