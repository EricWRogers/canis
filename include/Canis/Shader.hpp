#pragma once
#include <string>
#include <glm/glm.hpp>

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
        int GetUniformLocation(const std::string &uniformName);
        int GetProgramID() { return m_programId; }

    private:
        bool m_isLinked = false;

        unsigned int m_programId = 0;
        unsigned int m_vertexShaderId = 0;
        unsigned int m_fragmentShaderId = 0;

        int m_numberOfAttributes = 0;

        void CompileShaderFile(const std::string &_filePath, unsigned int &_id);
    };

} // end of Canis namespace