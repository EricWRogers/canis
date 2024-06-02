#pragma once
#include <glm/glm.hpp>
#include <Canis/Data/GLTexture.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    struct Sprite2DComponent
    {
        glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        TextureHandle textureHandle;

        static void RegisterProperties()
		{
			REGISTER_PROPERTY(Canis::Sprite2DComponent, uv, glm::vec4);
			REGISTER_PROPERTY(Canis::Sprite2DComponent, textureHandle, TextureHandle);
		}
    };

    static inline void GetSpriteFromTextureAtlas(Canis::Sprite2DComponent &_sprite,
                                    const unsigned short int _offsetX, const unsigned short int _offsetY,
                                    const unsigned int &_indexX, const unsigned int &_indexY,
                                    const unsigned int &_spriteWidth, const unsigned int &_spriteHeight,
                                    const bool &_flipX, const bool &_flipY)
    {
        _sprite.uv.x = (_flipX) ? (((_indexX+1) * _spriteWidth) + _offsetX)/(float)_sprite.textureHandle.texture.width : (_indexX == 0) ? 0.0f : ((_indexX * _spriteWidth) + _offsetX)/(float)_sprite.textureHandle.texture.width;
        _sprite.uv.y = (_flipY) ? (((_indexY+1) * _spriteHeight) + _offsetY)/(float)_sprite.textureHandle.texture.height : (_indexY == 0) ? 0.0f : ((_indexY * _spriteHeight) + _offsetY)/(float)_sprite.textureHandle.texture.height;
        _sprite.uv.z = (_flipX) ? (_spriteWidth*-1.0f)/(float)_sprite.textureHandle.texture.width : _spriteWidth/(float)_sprite.textureHandle.texture.width;
        _sprite.uv.w = (_flipY) ? (_spriteHeight*-1.0f)/(float)_sprite.textureHandle.texture.height : _spriteHeight/(float)_sprite.textureHandle.texture.height;
    }

} // end of Canis namespace