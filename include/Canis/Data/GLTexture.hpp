#pragma once

#if __ANDROID__
#define GLES
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#elif __EMSCRIPTEN__
#define GLES
#include <emscripten.h>
#include <GLES3/gl3.h>
#else
#define GLAD
#include <glad/glad.h>
#endif

namespace Canis
{
	struct GLTexture
	{
		GLuint id;
		int width;
		int height;
	};
}