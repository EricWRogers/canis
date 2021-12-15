#include "Window.hpp"

namespace Canis
{
    Window::Window() {}

    Window::~Window() {}

    int Window::Spawn(std::string window_name, int screen_width, int screen_height, unsigned int current_flags)
    {
        Uint32 flags = SDL_WINDOW_OPENGL;

        if (current_flags & INVISIBLE)
            flags |= SDL_WINDOW_HIDDEN;
        if (current_flags & FULLSCREEN)
            flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        if (current_flags & BORDERLESS)
            flags |= SDL_WINDOW_BORDERLESS;
        
        sdl_window = SDL_CreateWindow(window_name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screen_width, screen_height, flags);
        if (sdl_window == nullptr)
            FatalError("SDL Window could not be created");
        
        SDL_GLContext gl_context = SDL_GL_CreateContext(sdl_window);
        if (gl_context == nullptr)
            FatalError("SDL_GL context could not be created!");
        
        GLenum glew_status = glewInit();
        if (glew_status != GLEW_OK)
            FatalError("Could not init GLEW");
        
        std::printf("*** OpenGL Version: %s ***", glGetString(GL_VERSION));
        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);

        // VSYNC 0 off 1 on
        SDL_GL_SetSwapInterval(0);

        // Enable alpha blending
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        return 0;
    }

    void Window::SwapBuffer()
    {
        SDL_GL_SwapWindow(sdl_window);
    }
}