#pragma once
#include <string>
#include <GL/glew.h>
#include <vector>
#include <fstream>

#include "Debug.h"

namespace canis
{

    class GLSLProgram
    {
    public:
        GLSLProgram();
        ~GLSLProgram();

        void compileShaders(const std::string &vertexShaderFilePath, const std::string &fragmentShaderFilePath);

        void linkShaders();

        void addAttribute(const std::string &attributeName);

        GLint getUniformLocation(const std::string &uniformName);

        void use();
        void unUse();

    private:
        GLuint _programID;

        int _numAttributes;

        void compileShader(const std::string &filePath, GLuint &id);

        GLuint _vertexShaderID;
        GLuint _fragmentShaderID;
    };

} // end of canis namespace