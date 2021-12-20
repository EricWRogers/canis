#pragma once
#include <string>
#include <GL/glew.h>
#include <vector>
#include <fstream>

#include "Debug.h"

namespace Canis
{

    class Shader
    {
    public:
        Shader();
        ~Shader();

        void Compile(const std::string &vertexShaderFilePath, const std::string &fragmentShaderFilePath);

        void Link();

        void AddAttribute(const std::string &attributeName);

        GLint GetUniformLocation(const std::string &uniformName);

        void Use();
        void UnUse();

    private:
        GLuint program_id;

        int number_of_attributes;

        void compile(const std::string &filePath, GLuint &id);

        GLuint vertex_shader_id;
        GLuint fragment_shader_id;
    };

} // end of Canis namespace