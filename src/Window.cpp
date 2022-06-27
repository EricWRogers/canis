#include <Canis/Window.hpp>

PFNGLBINDVERTEXARRAYPROC bindVertexArrayOES;
PFNGLDELETEVERTEXARRAYSPROC deleteVertexArraysOES;
PFNGLGENVERTEXARRAYSPROC genVertexArraysOES;
PFNGLISVERTEXARRAYPROC isVertexArrayOES;

namespace Canis
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

        #ifdef GLAD
        
        if (_currentFlags & WindowFlags::FULLSCREEN)
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (_currentFlags & WindowFlags::BORDERLESS)
            flags |= SDL_WINDOW_BORDERLESS;

        // Create Window
        sdlWindow = SDL_CreateWindow(_windowName.c_str(), SDL_WINDOWPOS_CENTERED_DISPLAY(0), SDL_WINDOWPOS_CENTERED_DISPLAY(0), _screenWidth, _screenHeight, flags);
        
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
        #endif 

        #ifdef __ANDROID__
        SDL_GLContext glContext;
        sdlWindow = {SDL_CreateWindow(
            "A Simple Triangle",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            screenWidth, screenHeight,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI)};
        glContext = {SDL_GL_CreateContext(sdlWindow)};
        //SDL_GL_GetDrawableSize(sdlWindow, &screenWidth, &screenHeight);
        #endif

        #ifdef __ANDROID__
            void *libhandle = dlopen("libGLESv3.so", RTLD_LAZY);

            bindVertexArrayOES = (PFNGLBINDVERTEXARRAYPROC) dlsym(libhandle,
                                                                    "glBindVertexArray");
            deleteVertexArraysOES = (PFNGLDELETEVERTEXARRAYSPROC) dlsym(libhandle,
                                                                        "glDeleteVertexArrays");
            genVertexArraysOES = (PFNGLGENVERTEXARRAYSPROC)dlsym(libhandle,
                                                                    "glGenVertexArrays");
            isVertexArrayOES = (PFNGLISVERTEXARRAYPROC)dlsym(libhandle,
                                                                "glIsVertexArray");
        #endif
        #ifdef GLAD
            if (!gladLoadGLLoader(SDL_GL_GetProcAddress))
            {
                FatalError("Could not init load glad");
            } 
        #endif 

        //std::string openglVersion = (const char*)glGetString(GL_VERSION);
        //Log("*** OpenGL Version: " + openglVersion + " ***");

        Log("Help");

        // before a new frame is drawn we need to clear the buffer
        // the clear color will be the new value of all of the pixels
        // in that buffer
        glClearColor(0.37f, 0.36f, 0.55f, 1.0f);

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

    void Window::MouseLock(bool _isLocked)
    {
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
} // end of Canis namespace