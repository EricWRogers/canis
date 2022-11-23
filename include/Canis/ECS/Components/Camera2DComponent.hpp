#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct Camera2DComponent
    {
        glm::vec2 position = glm::vec2(0.0f);
        float scale = 1.0f;
    };
} // end of Canis namespace