#pragma once
#include <Canis/Data/GLTexture.hpp>

namespace Canis
{
    struct TextureHandle
    {
        int id = -1;
        GLTexture texture;
    };
}