#pragma once
#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>
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
        void SetBool(const std::string &name, bool value) const;
        void SetInt(const std::string &name, int value) const;
        void SetFloat(const std::string &name, float value) const;
        void SetVec2(const std::string &name, const glm::vec2 &value) const;
        void SetVec2(const std::string &name, float x, float y) const;
        void SetVec3(const std::string &name, const glm::vec3 &value) const;
        void SetVec3(const std::string &name, float x, float y, float z) const;
        void SetVec4(const std::string &name, const glm::vec4 &value) const;
        void SetVec4(const std::string &name, float x, float y, float z, float w);
        void SetMat2(const std::string &name, const glm::mat2 &mat) const;
        void SetMat3(const std::string &name, const glm::mat3 &mat) const;
        void SetMat4(const std::string &name, const glm::mat4 &mat) const;

        GLuint program_id;


    private:
        int number_of_attributes;

        void compile(const std::string &filePath, GLuint &id);

        GLuint vertex_shader_id;
        GLuint fragment_shader_id;
    };

} // end of Canis namespace