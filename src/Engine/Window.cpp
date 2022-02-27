#include "Window.hpp"

namespace Engine
{
    Window::Window()
    {
    }

    Window::~Window()
    {
    }

    int Window::Create(std::string _windowName, int _screenWidth, int _screenHeight, unsigned int _currentFlags)
    {
        // if you wanted you application to suport multiple rendering apis 
        // you would not want to hard code it here
        Uint32 flags = SDL_WINDOW_OPENGL;

        screenWidth = _screenWidth;
        screenHeight = _screenHeight;
        
        if (_currentFlags & WindowFlags::FULLSCREEN)
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (_currentFlags & WindowFlags::BORDERLESS)
            flags |= SDL_WINDOW_BORDERLESS;

        // Create Window
        sdlWindow = SDL_CreateWindow(_windowName.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, _screenWidth, _screenHeight, flags);
        
        if (sdlWindow == nullptr) // Check for an error when creating a window
        {
            FatalError("SDL Window could not be created");
        }

        // Create OpenGL Context
        SDL_GLContext glContext = SDL_GL_CreateContext(sdlWindow);

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
        glClearColor(0.223f, 0.203f, 0.341f, 1.0f);

        // VSYNC 0 off 1 on
        SDL_GL_SetSwapInterval(0);

        // Enable alpha blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        return 0;
    }

    void Window::SetWindowName(std::string _windowName)
    {
        SDL_SetWindowTitle(sdlWindow,_windowName.c_str());
    }

    void Window::SwapBuffer()
    {
        // After we draw our sprite and models to a window buffer
        // We want to display the one we were drawing to and
        // get the old buffer to start drawing our next frame to
        SDL_GL_SwapWindow(sdlWindow);
    }
} // end of Engine namespace