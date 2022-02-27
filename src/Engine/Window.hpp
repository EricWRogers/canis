#pragma once
#include <SDL.h>
#include <GL/glew.h>
#include <string>

#include "Debug.hpp"

namespace Engine
{
    enum WindowFlags
    {
        FULLSCREEN = 1,
        BORDERLESS = 16
    };

    class Window
    {
    public:
        Window();
        ~Window();

        int Create(std::string _windowName, int _screenWidth, int _screenHeight, unsigned int _currentFlags);
        void SetWindowName(std::string _windowName);

        void SwapBuffer();

        int GetScreenWidth() { return screenWidth; }
        int GetScreenHeight() { return screenHeight; }

    private:
        SDL_Window *sdlWindow;
        int screenWidth, screenHeight;
    };
} // end of Engine namespace