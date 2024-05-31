#pragma once

#ifdef __EMSCRIPTEN__
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <GLES3/gl3.h>
#else
#include <glad/glad.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#endif