#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <Canis/App.hpp>

int main(int argc, char *argv[])
{
#if defined(__EMSCRIPTEN__)
    static Canis::App app;
#else
    Canis::App app;
#endif

    app.Run();

    return 0;
}
