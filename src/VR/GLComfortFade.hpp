#pragma once
#include <Canis/OpenGL.hpp>
#include <algorithm>
#include <stdexcept>
namespace Canis::VR
{
    class GLComfortFade
    {
        GLuint program = 0, vao = 0;
        GLuint Compile(GLenum kind, const char* body)
        {
            GLuint shader = glCreateShader(kind);
            const char* source[] = {OPENGLVERSION, body};
            glShaderSource(shader,2,source,nullptr); glCompileShader(shader);
            GLint success = 0; glGetShaderiv(shader,GL_COMPILE_STATUS,&success);
            if (!success) { glDeleteShader(shader); throw std::runtime_error("VR comfort fade shader compilation failed"); }
            return shader;
        }
        void Initialize()
        {
            if (program) return;
            GLuint vertex = Compile(GL_VERTEX_SHADER,"\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0.0,1.0);}\n");
            GLuint fragment = 0;
            try { fragment = Compile(GL_FRAGMENT_SHADER,"\nprecision highp float; uniform float opacity; out vec4 color; void main(){color=vec4(0.0,0.0,0.0,opacity);}\n"); }
            catch (...) { glDeleteShader(vertex); throw; }
            program = glCreateProgram(); glAttachShader(program,vertex); glAttachShader(program,fragment);
            glLinkProgram(program); glDeleteShader(vertex); glDeleteShader(fragment);
            GLint success = 0; glGetProgramiv(program,GL_LINK_STATUS,&success);
            if (!success) { glDeleteProgram(program); program=0; throw std::runtime_error("VR comfort fade shader link failed"); }
            glGenVertexArrays(1,&vao);
        }
    public:
        ~GLComfortFade() { Reset(); }
        void Reset() { if (vao) glDeleteVertexArrays(1,&vao); if (program) glDeleteProgram(program); vao=program=0; }
        void Draw(float alpha)
        {
            if (alpha <= 0) return;
            Initialize();
            GLint oldProgram, oldVao, srcRGB, dstRGB, srcAlpha, dstAlpha, eqRGB, eqAlpha;
            GLboolean depthMask, colorMask[4];
            glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram); glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&oldVao);
            glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB); glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);
            glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
            glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRGB); glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);
            glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask); glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);
            bool depth=glIsEnabled(GL_DEPTH_TEST), blend=glIsEnabled(GL_BLEND), cull=glIsEnabled(GL_CULL_FACE), stencil=glIsEnabled(GL_STENCIL_TEST);
            glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_STENCIL_TEST); glDepthMask(GL_FALSE);
            glEnable(GL_BLEND); glBlendEquation(GL_FUNC_ADD); glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ZERO,GL_ONE);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            glUseProgram(program); glBindVertexArray(vao); glUniform1f(glGetUniformLocation(program,"opacity"),std::clamp(alpha,0.0f,1.0f));
            glDrawArrays(GL_TRIANGLES,0,3);
            glUseProgram(oldProgram); glBindVertexArray(oldVao);
            glBlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha); glBlendEquationSeparate(eqRGB,eqAlpha);
            glDepthMask(depthMask); glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
            if (depth) glEnable(GL_DEPTH_TEST);
            if (!blend) glDisable(GL_BLEND);
            if (cull) glEnable(GL_CULL_FACE);
            if (stencil) glEnable(GL_STENCIL_TEST);
        }
    };
}
