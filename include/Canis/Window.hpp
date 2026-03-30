#pragma once
#include <Canis/Math.hpp>

namespace Canis
{

    class Window
    {
    public:
        typedef enum Sync
        {
            IMMEDIATE = 0,
            VSYNC = 1,
            ADAPTIVE = -1
        } Sync;

        Window(const char *title, int width, int height);
        ~Window();

        // Gameplay/render surface size (current game view target).
        int GetScreenWidth() { return m_renderWidth; }
        int GetScreenHeight() { return m_renderHeight; }

        // Native SDL window pixel size.
        int GetWindowWidth() { return m_screenWidth; }
        int GetWindowHeight() { return m_screenHeight; }

        bool IsMouseLocked() { return m_mouseLock; }
        void LockMouse(bool _lock);
        void CenterMouse();
        void SetMousePosition(int _x, int _y);
        void RequestClose() { m_shouldClose = true; }
        bool ShouldClose() const { return m_shouldClose; }

        void Clear() const;
        void SetClearColor(Color _color);
        Color GetClearColor() { return m_clearColor; }
        void SwapBuffer() const;

        void SetWindowIcon(std::string _path);

        void SetWindowSize(int _width, int _height);
        void SetRenderSize(int _width, int _height);
        void SetResized(bool _resized);
        bool IsResized();

        void* GetSDLWindow() { return m_window; }
        void* GetGLContext() { return m_context; }

        Sync GetSync();
        void SetSync(Sync _type);

    private:
        void* m_window = nullptr;
        void* m_context = nullptr;

        Color m_clearColor;

        bool m_shouldClose = false;
        bool m_resized = false;

        int m_screenWidth = 0;
        int m_screenHeight = 0;
        int m_renderWidth = 0;
        int m_renderHeight = 0;

        bool m_mouseLock = false;

        void InitGL();
    };

} // namespace Canis
