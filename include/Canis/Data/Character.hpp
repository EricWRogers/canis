#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct Character
    {
        glm::ivec2 size;
        glm::ivec2 bearing;
        unsigned int advance;
        glm::vec2 atlasPos;
        glm::vec2 atlasSize;
    };
} // end of Canis namespace