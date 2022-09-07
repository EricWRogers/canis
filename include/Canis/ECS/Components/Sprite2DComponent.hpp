#pragma once
#include <glm/glm.hpp>
#include "../../Data/GLTexture.hpp"

namespace Canis
{
    struct Sprite2DComponent
    {
        glm::vec4 uv;
        GLTexture texture;
    };
    
} // end of Canis namespace