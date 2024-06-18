#pragma once

#ifdef __EMSCRIPTEN__
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#include <GLES3/gl3.h>
const static char* OPENGLVERSION = "#version 300 es"; 
#else
#include <GL/glew.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
const static char* OPENGLVERSION = "#version 330 core"; 
#endif

