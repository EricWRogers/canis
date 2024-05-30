#pragma once
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#include <GLES3/gl3.h>

namespace Canis
{
	struct GLTexture
	{
		GLuint id;
		int width;
		int height;
	};
}