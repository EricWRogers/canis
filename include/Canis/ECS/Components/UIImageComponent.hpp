#pragma once
#include <glm/glm.hpp>
#include "../../Data/GLTexture.hpp"

namespace Canis
{
    struct UIImageComponent
    {
        glm::vec4 uv = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        GLTexture texture;
    };

    static void GetUIImageFromTextureAtlas(Canis::UIImageComponent &_sprite,
                                    const unsigned short int _offsetX, const unsigned short int _offsetY,
                                    const unsigned int &_row, const unsigned int &_column,
                                    const unsigned int &_spriteWidth, const unsigned int &_spriteHeight,
                                    const bool &_flipX, const bool &_flipY)
    {
        _sprite.uv.x = (_flipX) ? (((_row+1) * _spriteWidth) + _offsetX)/(float)_sprite.texture.width : (_row == 0) ? 0.0f : ((_row * _spriteWidth) + _offsetX)/(float)_sprite.texture.width;
        _sprite.uv.y = (_flipY) ? (((_column+1) * _spriteHeight) + _offsetY)/(float)_sprite.texture.height : (_column == 0) ? 0.0f : ((_column * _spriteHeight) + _offsetY)/(float)_sprite.texture.height;
        _sprite.uv.z = (_flipX) ? (_spriteWidth*-1.0f)/(float)_sprite.texture.width : _spriteWidth/(float)_sprite.texture.width;
        _sprite.uv.w = (_flipY) ? (_spriteHeight*-1.0f)/(float)_sprite.texture.height : _spriteHeight/(float)_sprite.texture.height;
    }
} // end of Canis namespace