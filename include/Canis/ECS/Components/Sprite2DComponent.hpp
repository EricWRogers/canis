#pragma once
#include <glm/glm.hpp>
#include "../../Data/GLTexture.hpp"

namespace Canis
{
    struct Sprite2DComponent
    {
        glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        GLTexture texture;
    };
    
} // end of Canis namespace