#include "ResourceManager.hpp"

namespace canis
{
    TextureCache ResourceManager::_textureCache;

    GLTexture ResourceManager::getTexture(std::string texturePath)
    {
        return _textureCache.getTexture(texturePath);
    }
} // end of canis namespace