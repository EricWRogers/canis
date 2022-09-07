#pragma once
#include <Canis/Data/SpriteVertex.hpp>

namespace Canis
{
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