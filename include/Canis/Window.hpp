#pragma once

#if __ANDROID__
#define GLES
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#elif __EMSCRIPTEN__
#define GLES
#include <emscripten.h>
#include <GLES3/gl3.h>
#else
#define GLAD
#include <glad/glad.h>
#endif

#include <SDL.h>
#include <string>

#include "Debug.hpp"

extern PFNGLBINDVERTEXARRAYPROC bindVertexArrayOES;
extern PFNGLDELETEVERTEXARRAYSPROC deleteVertexArraysOES;
extern PFNGLGENVERTEXARRAYSPROC genVertexArraysOES;
extern PFNGLISVERTEXARRAYPROC isVertexArrayOES;

#ifdef __ANDROID__
#include <dlfcn.h>
#define BindVAO bindVertexArrayOES
#define GenVAO genVertexArraysOES
#else
#define BindVAO glBindVertexArray
#define GenVAO glGenVertexArrays
#endif

namespace Canis
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
        void MouseLock(bool _isLocked);

        int GetScreenWidth() { return screenWidth; }
        int GetScreenHeight() { return screenHeight; }

        float fps;

    private:
        SDL_Window *sdlWindow;
        int screenWidth, screenHeight;
    };
} // end of Canis namespace