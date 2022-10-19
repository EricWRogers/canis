#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct Character
    {
        unsigned int textureID; // ID handle of the glyph texture
        glm::ivec2 size;        // Size of glyph
        glm::ivec2 bearing;     // Offset from baseline to left/top of glyph
        unsigned int advance;   // Horizontal offset to advance to next glyph
    };
} // end of Canis namespace