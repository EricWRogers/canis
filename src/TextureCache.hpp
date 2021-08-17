#pragma once
#include <string>
#include <map>
#include <iostream>

#include "ImageLoader.hpp"
#include "GLTexture.h"

namespace canis
{
    class TextureCache
    {
    public:
        TextureCache();
        ~TextureCache();

        GLTexture getTexture(std::string);

    private:
        std::map<std::string, GLTexture> _textureMap;
    };
} // end of canis namespace