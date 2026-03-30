#include <Canis/Shader.hpp>
#include <Canis/Debug.hpp>
#include <Canis/OpenGL.hpp>

//#define GLM_ENABLE_EXPERIMENTAL
//#include "glm/gtx/hash.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <fstream>
#include <sstream>

namespace Canis
{
    namespace
    {
        bool HasActivePrecisionQualifier(const std::string &_source)
        {
            std::istringstream stream(_source);
            std::string line;

            while (std::getline(stream, line))
            {
                const size_t firstNonWhitespace = line.find_first_not_of(" \t\r");
                if (firstNonWhitespace == std::string::npos)
                    continue;

                if (line.compare(firstNonWhitespace, 2, "//") == 0)
                    continue;

                if (line.compare(firstNonWhitespace, 10, "precision ") == 0)
                    return true;
            }

            return false;
        }
    }

    Shader::Shader()
    {
    }

    Shader::Shader(const std::string &_vertexShaderFilePath, const std::string &_fragmentShaderFilePath)
    {
        Compile(_vertexShaderFilePath, _fragmentShaderFilePath);
        Link();
    }

    Shader::~Shader()
    {
        if (m_fragmentShaderId != 0)
            glDeleteShader(m_fragmentShaderId);
        if (m_vertexShaderId != 0)
            glDeleteShader(m_vertexShaderId);
    }

    void Shader::Compile(const std::string &_vertexShaderFilePath, const std::string &_fragmentShaderFilePath)
    {
        m_path = _vertexShaderFilePath;
        //Getting vertex shaderID
        m_vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
        if (m_vertexShaderId == 0)
            Debug::FatalError("Vertex shader failed to be created!");

        //Getting fragment shaderID
        m_fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
        if (m_fragmentShaderId == 0)
            Debug::FatalError("Fragment shader failed to be created!");

        m_programId = glCreateProgram();

        CompileShaderFile(_vertexShaderFilePath, m_vertexShaderId);
        CompileShaderFile(_fragmentShaderFilePath, m_fragmentShaderId);
    }

    void Shader::Link()
    {
        if (m_isLinked)
            return;
        
        glAttachShader(m_programId, m_vertexShaderId);
        glAttachShader(m_programId, m_fragmentShaderId);

        glLinkProgram(m_programId);

        GLuint isLinked = 0;
        glGetProgramiv(m_programId, GL_LINK_STATUS, (int *)&isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(m_programId, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(m_programId, maxLength, &maxLength, infoLog.data());

            glDeleteProgram(m_programId);

            Debug::FatalError("Shader failed to link!\nOpengl Error: %s", std::string(infoLog.begin(), infoLog.end()).c_str());
        } else {
            m_isLinked = true;
        }

        glDetachShader(m_programId, m_vertexShaderId);
        glDetachShader(m_programId, m_fragmentShaderId);
        glDeleteShader(m_vertexShaderId);
        glDeleteShader(m_fragmentShaderId);
    }

    void Shader::AddAttribute(const std::string &_attributeName)
    {
        glBindAttribLocation(m_programId, m_numberOfAttributes++, _attributeName.c_str());
    }

    GLint Shader::GetUniformLocation(const std::string &_uniformName, const size_t _valueHash) const
    {
        if (m_locationsCashe.find(_uniformName) != m_locationsCashe.end())
        {
            //Canis::Log("Cashe: " + _uniformName + " location: " + std::to_string(m_locationsCashe[_uniformName]));
            return m_locationsCashe[_uniformName];
        }
        
        GLint location = glGetUniformLocation(m_programId, _uniformName.c_str());

        //Canis::Log(_uniformName + " location: " + std::to_string(location));

        if (location == GL_INVALID_INDEX)
        {
            //Debug::FatalError("Uniform " + _uniformName + " not found in shader " + m_path + " isValid: " + std::to_string(m_isLinked));
        }
        
        if (m_lastValueCashe.find(_uniformName) != m_lastValueCashe.end())
        {
            if (m_lastValueCashe[_uniformName] == _valueHash)
            {
                return -1;
            }
        }

        m_locationsCashe[_uniformName] = location;

        
        return location;
    }

    void Shader::Use()
    {
        glUseProgram(m_programId);
        for (int i = 0; i < m_numberOfAttributes; i++)
        {
            glEnableVertexAttribArray(i);
        }
    }

    void Shader::UnUse()
    {
        glUseProgram(0);
    }

    void Shader::SetBool(const std::string &_name, bool _value) const
    {
        size_t valueHash = std::hash<bool>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform1i(location, (int)_value); 
    }
    
    void Shader::SetInt(const std::string &_name, int _value) const
    {
        size_t valueHash = std::hash<int>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform1i( location, _value); 
    }
    
    void Shader::SetFloat(const std::string &_name, float _value) const
    {
        size_t valueHash = std::hash<float>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform1f( location, _value); 
    }
    
    void Shader::SetVec2(const std::string &_name, const Vector2 &_value) const
    {
        size_t valueHash = std::hash<Vector2>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform2fv( location, 1, &_value.x); 
    }

    void Shader::SetVec2(const std::string &_name, float _x, float _y) const
    {
        SetVec2(_name, Vector2(_x, _y));
    }

    void Shader::SetVec3(const std::string &_name, const Vector3 &_value) const
    {
        size_t valueHash = std::hash<Vector3>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform3f(location, _value.x, _value.y, _value.z);
    }

    void Shader::SetVec3(const std::string &_name, float _x, float _y, float _z) const
    {
        SetVec3(_name, Vector3(_x, _y, _z));
    }

    void Shader::SetVec4(const std::string &_name, const Vector4 &_value) const
    {
        size_t valueHash = std::hash<Vector4>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform4f(location, _value.x, _value.y, _value.z, _value.w);
    }

    void Shader::SetVec4(const std::string &_name, float _x, float _y, float _z, float _w)
    {
        SetVec4(_name, Vector4(_x, _y, _z, _w));
    }

    /*
    
    void Shader::SetMat2(const std::string &_name, const glm::mat2 &_mat) const
    {
        size_t valueHash = std::hash<glm::mat2>{}(_mat);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniformMatrix2fv( location, 1, GL_FALSE, &_mat[0][0]);
    }
    
    void Shader::SetMat3(const std::string &_name, const glm::mat3 &_mat) const
    {
        size_t valueHash = std::hash<glm::mat3>{}(_mat);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniformMatrix3fv( location, 1, GL_FALSE, &_mat[0][0]);
    }*/
    
    void Shader::SetMat4(const std::string &_name, const Matrix4 &_mat) const
    {
        size_t valueHash = HashMatrix(_mat);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(_mat));
    }


    void Shader::CompileShaderFile(const std::string &_filePath, unsigned int &_id)
    {
        SDL_IOStream* shaderFile = SDL_IOFromFile(_filePath.c_str(), "r");

        if (shaderFile == nullptr)
            Debug::FatalError("Unable to open file \"%s\"", _filePath.c_str());
        
        size_t shaderFileLength;// = static_cast<size_t>(SDL_RWsize(shaderFile));
        void* shaderFileData = SDL_LoadFile_IO(shaderFile, &shaderFileLength, true);
        std::string shaderFileCode(static_cast<char*>(shaderFileData), shaderFileLength);

        // Replace placeholder text with actual version directive
        std::string placeholder = "[OPENGL VERSION]";
        std::string versionDirective(OPENGLVERSION);

        size_t pos = shaderFileCode.find(placeholder);
        if (pos != std::string::npos) {
            shaderFileCode.replace(pos, placeholder.length(), versionDirective);
        } else {
            Debug::Error("Add [OPENGL VERSION] to the top of your shader file: %s", _filePath.c_str());
            shaderFileCode = versionDirective + shaderFileCode;
        }

#if defined(__EMSCRIPTEN__)
        if (!HasActivePrecisionQualifier(shaderFileCode))
        {
            const size_t firstNewline = shaderFileCode.find('\n');
            const std::string precisionBlock = "\nprecision mediump float;\nprecision mediump int;\n";

            if (firstNewline == std::string::npos)
                shaderFileCode += precisionBlock;
            else
                shaderFileCode.insert(firstNewline + 1, precisionBlock);
        }
#endif

        //Canis::Log(shaderFileCode);

        const char *contentsPtr = shaderFileCode.c_str();
        glShaderSource(_id, 1, &contentsPtr, nullptr);

        glCompileShader(_id);

        int success = 0;
        glGetShaderiv(_id, GL_COMPILE_STATUS, &success);

        if (shaderFileData != nullptr)
            SDL_free(shaderFileData);

        if (success == GL_FALSE)
        {
            int maxLength = 0;
            glGetShaderiv(_id, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<char> errorLog(maxLength);
            glGetShaderInfoLog(_id, maxLength, &maxLength, errorLog.data());

            glDeleteShader(_id);

            Debug::FatalError("Shader %s failed to compile\nOpengl Error: %s", _filePath.c_str(), std::string(errorLog.begin(), errorLog.end()).c_str());
            return;
        }
    }

} // end of Canis namespace
