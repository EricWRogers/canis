#pragma once
#include <Canis/Data/SpriteVertex.hpp>

namespace Canis
{
    enum class GlyphSortType
    {
        NONE,
        FRONT_TO_BACK,
        BACK_TO_FRONT,
        TEXTURE
    };
    
    struct Glyph
    {
        unsigned int textureId;
        float depth;
        float angle;

        SpriteVertex topLeft;
        SpriteVertex bottomLeft;
        SpriteVertex topRight;
        SpriteVertex bottomRight;
    };
} // end of Canis namespace