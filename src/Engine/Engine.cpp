#include "Engine.hpp"

namespace Engine
{
    int Init()
    {
        SDL_Init(SDL_INIT_EVERYTHING);

        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        
        return 0;
    }
} // end of Engine namespace