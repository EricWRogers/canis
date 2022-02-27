#include "Shader.hpp"

namespace Engine
{
    Shader::Shader()
    {
    }

    Shader::~Shader()
    {
    }

    void Shader::Compile(const std::string &vertexShaderFilePath, const std::string &fragmentShaderFilePath)
    {
        //Getting vertex shaderID
        vertex_shader_id = glCreateShader(GL_VERTEX_SHADER);
        if (vertex_shader_id == 0)
            FatalError("Vertex shader failed to be created!");

        //Getting fragment shaderID
        fragment_shader_id = glCreateShader(GL_FRAGMENT_SHADER);
        if (fragment_shader_id == 0)
            FatalError("Fragment shader failed to be created!");

        compile(vertexShaderFilePath, vertex_shader_id);
        compile(fragmentShaderFilePath, fragment_shader_id);
    }

    void Shader::Link()
    {
        glAttachShader(program_id, vertex_shader_id);
        glAttachShader(program_id, fragment_shader_id);

        glLinkProgram(program_id);

        GLuint isLinked = 0;
        glGetProgramiv(program_id, GL_LINK_STATUS, (int *)&isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program_id, maxLength, &maxLength, &infoLog[0]);

            glDeleteProgram(program_id);

            std::printf("%s\n", &(infoLog[0]));
            FatalError("Shaders failed to link!");
        }

        glDetachShader(program_id, vertex_shader_id);
        glDetachShader(program_id, fragment_shader_id);
        glDeleteShader(vertex_shader_id);
        glDeleteShader(fragment_shader_id);
    }

    void Shader::AddAttribute(const std::string &attributeName)
    {
        glBindAttribLocation(program_id, number_of_attributes++, attributeName.c_str());
    }

    GLint Shader::GetUniformLocation(const std::string &uniformName)
    {
        GLint location = glGetUniformLocation(program_id, uniformName.c_str());
        if (location == GL_INVALID_INDEX)
            FatalError("Uniform " + uniformName + " not found in shader!");

        return location;
    }

    void Shader::Use()
    {
        glUseProgram(program_id);
        for (int i = 0; i < number_of_attributes; i++)
        {
            glEnableVertexAttribArray(i);
        }
    }

    void Shader::UnUse()
    {
        glUseProgram(0);
    }

    void Shader::compile(const std::string &filePath, GLuint &id)
    {
        program_id = glCreateProgram();

        std::ifstream vertexFile(filePath);
        if (vertexFile.fail())
            Log("Failed to open " + filePath);

        std::string fileContents = "";
        std::string line;

        while (std::getline(vertexFile, line))
            fileContents += line + "\n";

        vertexFile.close();
        Log("File Close: " + filePath);

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

} // end of Engine namespace