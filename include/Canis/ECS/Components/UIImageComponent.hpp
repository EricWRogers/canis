#pragma once
#include <glm/glm.hpp>
#include "../../Data/GLTexture.hpp"

namespace Canis
{
    struct UIImageComponent
    {
        glm::vec4 uv;
        GLTexture texture;
    };
} // end of Canis namespace