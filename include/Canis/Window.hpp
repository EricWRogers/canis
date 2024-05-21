#pragma once
#include <string>
#include <glm/glm.hpp>

#include "Debug.hpp"

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

        int CreateFullScreen(std::string _windowName);
        int Create(std::string _windowName, int _screenWidth, int _screenHeight, unsigned int _currentFlags);
        void SetWindowName(std::string _windowName);

        void SwapBuffer();
        
        void CenterMouse();
        void SetMousePosition(int _x, int _y);

        void ClearColor();
        void SetClearColor(glm::vec4 _color);
        glm::vec4 GetScreenColor() { return m_clearColor; }

        void MouseLock(bool _isLocked);
        bool GetMouseLock() { return mouseLock; }

        int GetScreenWidth() { return screenWidth; }
        int GetScreenHeight() { return screenHeight; }

        void ToggleFullScreen();

        void* GetSDLWindow() { return m_sdlWindow; }
        void* GetGLContext() { return m_glContext; }

        float fps;

    private:
        void *m_sdlWindow;
        void *m_glContext;
        int screenWidth, screenHeight;
        bool m_fullscreen = false;
        bool mouseLock = false;
        glm::vec4 m_clearColor;
    };
} // end of Canis namespace
