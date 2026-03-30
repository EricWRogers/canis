#pragma once
#include <Canis/Math.hpp>

namespace Canis
{
    struct Character
    {
        int sizeX = 0;
        int sizeY = 0;
        int bearingX = 0;
        int bearingY = 0;
        unsigned int advance = 0;
        Vector2 atlasPos = Vector2(0.0f);
        Vector2 atlasSize = Vector2(0.0f);
    };
}
