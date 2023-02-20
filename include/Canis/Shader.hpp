#pragma once
#include <SDL.h>
#include <string>
#include <GL/glew.h>
#include <vector>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Debug.hpp"

namespace Canis
{
    class Shader
    {
    public:
        Shader();
        ~Shader();

        void Compile(const std::string &_vertexShaderFilePath, const std::string &_fragmentShaderFilePath);
        void Link();
        void AddAttribute(const std::string &_attributeName);
        void Use();
        void UnUse();
        void SetBool(const std::string &_name, bool _value) const;
        void SetInt(const std::string &_name, int value) const;
        void SetFloat(const std::string &_name, float _value) const;
        void SetVec2(const std::string &_name, const glm::vec2 &_value) const;
        void SetVec2(const std::string &_name, float _x, float _y) const;
        void SetVec3(const std::string &_name, const glm::vec3 &_value) const;
        void SetVec3(const std::string &_name, float _x, float _y, float _z) const;
        void SetVec4(const std::string &_name, const glm::vec4 &_value) const;
        void SetVec4(const std::string &_name, float _x, float _y, float _z, float _w);
        void SetMat2(const std::string &_name, const glm::mat2 &_mat) const;
        void SetMat3(const std::string &_name, const glm::mat3 &_mat) const;
        void SetMat4(const std::string &_name, const glm::mat4 &_mat) const;

        bool IsLinked() { return m_isLinked; }
        GLint GetUniformLocation(const std::string &uniformName);
        GLint GetProgramID() { return m_programId; }

    private:
        bool m_isLinked = false;
        GLuint m_programId = 0;
        GLuint m_vertexShaderId = 0;
        GLuint m_fragmentShaderId = 0;

        int m_numberOfAttributes = 0;

        void CompileShaderFile(const std::string &_filePath, GLuint &_id);
    };

} // end of Canis namespace