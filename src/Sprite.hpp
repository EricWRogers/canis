#pragma once
#include <string>
#include <GL/glew.h>
#include <cstddef>

#include "GLTexture.h"
#include "Vertex.h"
#include "ResourceManager.hpp"

namespace canis
{
    class Sprite
    {
    public:
        Sprite();
        ~Sprite();

        void init(float x, float y, float width, float height, std::string texturePath);

        void draw();

    private:
        float _x;
        float _y;
        float _width;
        float _height;
        GLuint _vboID;
        GLTexture _texture;
    };
} // end of canis namespace