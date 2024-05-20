#include <Canis/Window.hpp>
#include <SDL.h>
#include <GL/glew.h>

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
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        //SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        
        if (_currentFlags & WindowFlags::FULLSCREEN)
        {
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            m_fullscreen = true;
        }
        if (_currentFlags & WindowFlags::BORDERLESS)
            flags |= SDL_WINDOW_BORDERLESS;

        // Create Window
        sdlWindow = SDL_CreateWindow(_windowName.c_str(), SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0), _screenWidth, _screenHeight, flags);
        
        SDL_Surface *surface;     // Declare an SDL_Surface to be filled in with pixel data from an image file
        Uint16 pixels[16*16] = {  // ...or with raw pixel data:
            0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff,
            0x0fff, 0x0fff, 0x0fff, 0xf435, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff, 0x0fff, 0x0fff,
            0x0fff, 0x0fff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff, 0x0fff,
            0x0fff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff,
            0x0fff, 0xf435, 0xffff, 0xffff, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff,
            0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xffff, 0xf435, 0xf435,
            0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xf435,
            0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435,
            0xf435, 0xf435, 0xffff, 0xffff, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435,
            0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xffff, 0xffff, 0xf435, 0xf435,
            0xf435, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435,
            0x0fff, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff,
            0x0fff, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff,
            0x0fff, 0x0fff, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff, 0x0fff,
            0x0fff, 0x0fff, 0x0fff, 0xf435, 0xf435, 0xf435, 0xffff, 0xffff, 0xffff, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff, 0x0fff, 0x0fff,
            0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0xf435, 0x0fff, 0x0fff, 0x0fff, 0x0fff, 0x0fff,
        };
        surface = SDL_CreateRGBSurfaceFrom(pixels,16,16,16,16*2,0x0f00,0x00f0,0x000f,0xf000);

        // The icon is attached to the window pointer
        SDL_SetWindowIcon((SDL_Window*)sdlWindow, surface);

        // ...and the surface containing the icon pixel data is no longer required.
        SDL_FreeSurface(surface);

        if ((SDL_Window*)sdlWindow == nullptr) // Check for an error when creating a window
        {
            FatalError("SDL Window could not be created");
        }

        // Create OpenGL Context
        SDL_GLContext glContext = SDL_GL_CreateContext((SDL_Window*)sdlWindow);

        if (glContext == nullptr) // Check for an error when creating the OpenGL Context
        {
            FatalError("SDL_GL context could not be created!");
        }

        // Load OpenGL
        GLenum error = glewInit();

        if (error != GLEW_OK) // Check for an error loading OpenGL
        {
            FatalError("Could not init GLEW");
        }

        std::string openglVersion = (const char*)glGetString(GL_VERSION);
        Log("*** OpenGL Version: " + openglVersion + " ***");

        // before a new frame is drawn we need to clear the buffer
        // the clear color will be the new value of all of the pixels
        // in that buffer
        SetClearColor(glm::vec4(0.05f, 0.05f, 0.05f, 1.0f));

        // VSYNC 0 off 1 on
        SDL_GL_SetSwapInterval(0);

        // Enable alpha blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        return 0;
    }

    void Window::SetWindowName(std::string _windowName)
    {
        SDL_SetWindowTitle((SDL_Window*)sdlWindow,_windowName.c_str());
    }

    void Window::SwapBuffer()
    {
        // After we draw our sprite and models to a window buffer
        // We want to display the one we were drawing to and
        // get the old buffer to start drawing our next frame to
        SDL_GL_SwapWindow((SDL_Window*)sdlWindow);
    }
    
    void Window::CenterMouse()
    {
      SetMousePosition(screenWidth/2, screenHeight/2);
    }

    void Window::SetMousePosition(int _x, int _y)
    {
      SDL_WarpMouseInWindow((SDL_Window*)sdlWindow, _x, screenHeight - _y);
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

        SDL_SetWindowFullscreen((SDL_Window*)sdlWindow, m_fullscreen);
    }
} // end of Canis namespace
