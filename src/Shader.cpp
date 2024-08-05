#include <Canis/Shader.hpp>
#include <Canis/Debug.hpp>
#include <Canis/External/OpenGl.hpp>

#include "glm/gtx/hash.hpp"

#include <SDL.h>

#include <vector>
#include <fstream>

namespace Canis
{
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
            FatalError("Vertex shader failed to be created!");

        //Getting fragment shaderID
        m_fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
        if (m_fragmentShaderId == 0)
            FatalError("Fragment shader failed to be created!");

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

            FatalError("Shader failed to link!\nOpengl Error: " + std::string(infoLog.begin(), infoLog.end()));
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
            //FatalError("Uniform " + _uniformName + " not found in shader " + m_path + " isValid: " + std::to_string(m_isLinked));
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
    
    void Shader::SetVec2(const std::string &_name, const glm::vec2 &_value) const
    {
        size_t valueHash = std::hash<glm::vec2>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform2fv( location, 1, &_value[0]); 
    }

    void Shader::SetVec2(const std::string &_name, float _x, float _y) const
    {
        size_t valueHash = std::hash<glm::vec2>{}(glm::vec2(_x,_y));
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform2f( location, _x, _y); 
    }
    
    void Shader::SetVec3(const std::string &_name, const glm::vec3 &_value) const
    {
        size_t valueHash = std::hash<glm::vec3>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform3fv( location, 1, &_value[0]); 
    }

    void Shader::SetVec3(const std::string &_name, float _x, float _y, float _z) const
    {
        size_t valueHash = std::hash<glm::vec3>{}(glm::vec3(_x,_y,_z));
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform3f( location, _x, _y, _z); 
    }
    
    void Shader::SetVec4(const std::string &_name, const glm::vec4 &_value) const
    {
        size_t valueHash = std::hash<glm::vec4>{}(_value);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform4fv( location, 1, &_value[0]); 
    }

    void Shader::SetVec4(const std::string &_name, float _x, float _y, float _z, float _w) 
    {
        size_t valueHash = std::hash<glm::vec4>{}(glm::vec4(_x,_y,_z,_w));
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniform4f( location, _x, _y, _z, _w); 
    }
    
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
    }
    
    void Shader::SetMat4(const std::string &_name, const glm::mat4 &_mat) const
    {
        size_t valueHash = std::hash<glm::mat4>{}(_mat);
        int location = GetUniformLocation(_name, valueHash);
        if (location > -1)
            glUniformMatrix4fv(location, 1, GL_FALSE, &_mat[0][0]);
    }


    void Shader::CompileShaderFile(const std::string &_filePath, unsigned int &_id)
    {
        SDL_RWops* shaderFile = SDL_RWFromFile(_filePath.c_str(), "r");

        if (shaderFile == nullptr)
            FatalError("Unable to open file \"" + _filePath + "\"");
        
        size_t shaderFileLength;// = static_cast<size_t>(SDL_RWsize(shaderFile));
        void* shaderFileData = SDL_LoadFile_RW(shaderFile, &shaderFileLength, true);
        std::string shaderFileCode(static_cast<char*>(shaderFileData), shaderFileLength);

        // Replace placeholder text with actual version directive
        std::string placeholder = "[OPENGL VERSION]";
        std::string versionDirective(OPENGLVERSION);

        size_t pos = shaderFileCode.find(placeholder);
        if (pos != std::string::npos) {
            shaderFileCode.replace(pos, placeholder.length(), versionDirective);
        } else {
            Error("Add [OPENGL VERSION] to the top of your shader file: " + _filePath);
            shaderFileCode = versionDirective + shaderFileCode;
        }

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

            FatalError("Shader " + _filePath + " failed to compile\nOpengl Error: " + std::string(errorLog.begin(), errorLog.end()));
            return;
        }
    }

} // end of Canis namespace