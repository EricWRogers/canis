#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct Camera2DComponent
    {
        glm::vec2 position;
        float scale;
    };
} // end of Canis namespace