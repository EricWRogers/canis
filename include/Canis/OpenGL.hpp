#pragma once

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#include <GLES3/gl3platform.h>
const static char* OPENGLVERSION = "#version 300 es";
#else
#include <GL/glew.h>
//#include <SDL3/SDL_opengl.h>
//#include <SDL3/SDL_opengl_glext.h>
const static char* OPENGLVERSION = "#version 330 core"; 
#endif
