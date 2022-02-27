#pragma once
#include <string>
#include <GL/glew.h>
#include <vector>
#include <fstream>

#include "Debug.hpp"

namespace Engine
{
    class Shader
    {
    public:
        Shader();
        ~Shader();

        void Compile(const std::string &vertexShaderFilePath, const std::string &fragmentShaderFilePath);
        void Link();
        void AddAttribute(const std::string &attributeName);
        void Use();
        void UnUse();

        GLint GetUniformLocation(const std::string &uniformName);
        GLint GetProgramID() { return program_id; }

    private:
        GLuint program_id = 0;
        GLuint vertex_shader_id = 0;
        GLuint fragment_shader_id = 0;

        int number_of_attributes = 0;

        void compile(const std::string &filePath, GLuint &id);
    };

} // end of Engine namespace