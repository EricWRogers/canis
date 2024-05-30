#include <Canis/Window.hpp>
#include <SDL.h>
#include <SDL_opengl.h>
#include <GLES3/gl3.h>

namespace Canis
{
    Window::Window()
    {
    }

    Window::~Window()
    {
    }

    int Window::CreateFullScreen(std::string _windowName) {

        SDL_DisplayMode DM;
        SDL_GetCurrentDisplayMode(0, &DM);

        unsigned int windowFlags = 0;
	    windowFlags |= WindowFlags::FULLSCREEN;

        m_fullscreen = true;

        return Create(_windowName, DM.w, DM.h, windowFlags);
    }

    int Window::Create(std::string _windowName, int _screenWidth, int _screenHeight, unsigned int _currentFlags)
    {
        // if you wanted you application to support multiple rendering apis 
        // you would not want to hard code it here
        Uint32 flags = SDL_WINDOW_OPENGL;

        screenWidth = _screenWidth;
        screenHeight = _screenHeight;

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        //SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        
        if (_currentFlags & WindowFlags::FULLSCREEN)
        {
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            m_fullscreen = true;
        }
        if (_currentFlags & WindowFlags::BORDERLESS)
            flags |= SDL_WINDOW_BORDERLESS;

        // Create Window
        m_sdlWindow = SDL_CreateWindow(_windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, _screenWidth, _screenHeight, flags);
    

        Log("About to check SDL");

        if ((SDL_Window*)m_sdlWindow == nullptr) // Check for an error when creating a window
        {
            FatalError("SDL Window could not be created");
        }

        Log("About to check GL Context");

        // Create OpenGL Context
        //m_glContext = {SDL_GL_CreateContext((SDL_Window*)m_sdlWindow)};
        m_glContext = (void*)SDL_GL_CreateContext((SDL_Window*)m_sdlWindow);

        if (m_glContext == nullptr) // Check for an error when creating the OpenGL Context
        {
            FatalError("SDL_GL context could not be created!");
        }

        // Display OpenGL version
        int major, minor;
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
        SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
        std::cout << "OpenGLES version loaded: " << major << "." << minor << std::endl;

        Log("Set Clear Color");

        // before a new frame is drawn we need to clear the buffer
        // the clear color will be the new value of all of the pixels
        // in that buffer
        SetClearColor(glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));

        Log("Set Swap");

        // VSYNC 0 off 1 on
        SDL_GL_SetSwapInterval(0);

        Log("glEnable");

        // Enable alpha blending
        glEnable(GL_BLEND);
        Log("glBlendFunc");
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        Log("End of create");

        return 0;
    }

    void Window::SetWindowName(std::string _windowName)
    {
        SDL_SetWindowTitle((SDL_Window*)m_sdlWindow,_windowName.c_str());
    }

    void Window::SwapBuffer()
    {
        // After we draw our sprite and models to a window buffer
        // We want to display the one we were drawing to and
        // get the old buffer to start drawing our next frame to
        SDL_GL_SwapWindow((SDL_Window*)m_sdlWindow);
    }
    
    void Window::CenterMouse()
    {
      SetMousePosition(screenWidth/2, screenHeight/2);
    }

    void Window::SetMousePosition(int _x, int _y)
    {
      SDL_WarpMouseInWindow((SDL_Window*)m_sdlWindow, _x, screenHeight - _y);
    }

    void Window::ClearColor()
    {
        glClearColor(m_clearColor.r, m_clearColor.g, m_clearColor.b, m_clearColor.a);
    }

    void Window::SetClearColor(glm::vec4 _color)
    {
        m_clearColor = _color;
    }

    void Window::MouseLock(bool _isLocked)
    {
        mouseLock = _isLocked;
        if (_isLocked)
        {
            SDL_CaptureMouse(SDL_TRUE);
            SDL_SetRelativeMouseMode(SDL_TRUE);
        }
        else
        {
            SDL_CaptureMouse(SDL_FALSE);
            SDL_SetRelativeMouseMode(SDL_FALSE);
        }
    }

    void Window::ToggleFullScreen()
    {
        m_fullscreen = !m_fullscreen;

        SDL_SetWindowFullscreen((SDL_Window*)m_sdlWindow, m_fullscreen);
    }
} // end of Canis namespace
