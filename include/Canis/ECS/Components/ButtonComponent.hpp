#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct ButtonComponent
    {
        void (*func)(void*) = nullptr;
        void *instance = nullptr;
        glm::vec4 baseColor = glm::vec4(0.9f,0.9f,0.9f,1.0f);
        glm::vec4 hoverColor = glm::vec4(1.0f);
    };
} // end of Canis namespace