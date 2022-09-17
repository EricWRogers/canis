#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct ButtonComponent
    {
        float x;
        float y;
        float w;
        float h;
        void (*func)(void*);
        void *instance;
        glm::vec4 baseColor;
        glm::vec4 hoverColor;
    };
} // end of Canis namespace