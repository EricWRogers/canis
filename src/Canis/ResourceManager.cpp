#include "ResourceManager.hpp"

namespace Canis
{
    TextureCache ResourceManager::_textureCache;

    GLTexture ResourceManager::getTexture(std::string texturePath)
    {
        return _textureCache.getTexture(texturePath);
    }
} // end of Canis namespace