#include "TextureCache.hpp"

namespace Canis
{
    TextureCache::TextureCache()
    {
    }

    TextureCache::~TextureCache()
    {
    }

    GLTexture TextureCache::getTexture(std::string texturePath)
    {
        // lookup the texture and see if its in the map
        std::map<std::string, GLTexture>::iterator mit = _textureMap.find(texturePath);
        // you can use auto instead of everthing infront of mit
        //
        // check if its not in the map
        if (mit == _textureMap.end())
        {
            // load the texture
            GLTexture newTexture = ImageLoader::loadPNG(texturePath);
            // insert it into the map
            _textureMap.insert(make_pair(texturePath, newTexture));
            // std::cout << "Loaded Texture!\n";
            return newTexture;
        }
        // std::cout << "Loaded Cached Texture!\n";
        return mit->second;
    }
} // end of Canis namespace