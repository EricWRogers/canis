#include "GLSLProgram.hpp"

namespace Canis
{

    GLSLProgram::GLSLProgram() : _numAttributes(0), _programID(0), _vertexShaderID(0), _fragmentShaderID(0)
    {
    }

    GLSLProgram::~GLSLProgram()
    {
    }

    void GLSLProgram::compileShaders(const std::string &vertexShaderFilePath, const std::string &fragmentShaderFilePath)
    {
        //Getting vertex shaderID
        _vertexShaderID = glCreateShader(GL_VERTEX_SHADER);
        if (_vertexShaderID == 0)
            FatalError("Vertex shader failed to be created!");

        //Getting fragment shaderID
        _fragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
        if (_fragmentShaderID == 0)
            FatalError("Fragment shader failed to be created!");

        compileShader(vertexShaderFilePath, _vertexShaderID);
        compileShader(fragmentShaderFilePath, _fragmentShaderID);
    }

    void GLSLProgram::linkShaders()
    {
        glAttachShader(_programID, _vertexShaderID);
        glAttachShader(_programID, _fragmentShaderID);

        glLinkProgram(_programID);

        GLuint isLinked = 0;
        glGetProgramiv(_programID, GL_LINK_STATUS, (int *)&isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(_programID, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(_programID, maxLength, &maxLength, &infoLog[0]);

            glDeleteProgram(_programID);

            std::printf("%s\n", &(infoLog[0]));
            FatalError("Shaders failed to link!");
        }

        glDetachShader(_programID, _vertexShaderID);
        glDetachShader(_programID, _fragmentShaderID);
        glDeleteShader(_vertexShaderID);
        glDeleteShader(_fragmentShaderID);
    }

    void GLSLProgram::addAttribute(const std::string &attributeName)
    {
        glBindAttribLocation(_programID, _numAttributes++, attributeName.c_str());
    }

    GLint GLSLProgram::getUniformLocation(const std::string &uniformName)
    {
        GLint location = glGetUniformLocation(_programID, uniformName.c_str());
        if (location == GL_INVALID_INDEX)
            FatalError("Uniform " + uniformName + " not found in shader!");

        return location;
    }

    void GLSLProgram::use()
    {
        glUseProgram(_programID);
        for (int i = 0; i < _numAttributes; i++)
        {
            glEnableVertexAttribArray(i);
        }
    }

    void GLSLProgram::unUse()
    {
        glUseProgram(0);
    }

    void GLSLProgram::compileShader(const std::string &filePath, GLuint &id)
    {
        _programID = glCreateProgram();

        std::ifstream vertexFile(filePath);
        if (vertexFile.fail())
            Log("Failed to open " + filePath);

        std::string fileContents = "";
        std::string line;

        while (std::getline(vertexFile, line))
            fileContents += line + "\n";

        vertexFile.close();
        Log("VertexFile Close");

        const char *contentsPtr = fileContents.c_str();
        glShaderSource(id, 1, &contentsPtr, nullptr);

        glCompileShader(id);

        GLint success = 0;
        glGetShaderiv(id, GL_COMPILE_STATUS, &success);

        if (success == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetShaderiv(id, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<char> errorLog(maxLength);
            glGetShaderInfoLog(id, maxLength, &maxLength, &errorLog[0]);

            glDeleteShader(id);

            Log(" ");
            std::printf("%s\n", &(errorLog[0]));
            //Log(std::to_string(eLog);
            FatalError("Shader " + filePath + " failed to compile");

            return;
        }
    }

} // end of Canis namespace