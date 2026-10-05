#include <Canis/Shader.hpp>
#include <Canis/Debug.hpp>
#include <Canis/OpenGL.hpp>

//#define GLM_ENABLE_EXPERIMENTAL
//#include "glm/gtx/hash.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <vector>
#include <fstream>
#include <sstream>
#include <regex>

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
        if (m_programId != 0)
            glDeleteProgram(m_programId);
        if (m_fragmentShaderId != 0)
            glDeleteShader(m_fragmentShaderId);
        if (m_vertexShaderId != 0)
            glDeleteShader(m_vertexShaderId);
    }

    void Shader::Compile(const std::string &_vertexShaderFilePath, const std::string &_fragmentShaderFilePath)
    {
        auto read = [](const std::string &path) {
            size_t size=0;void* bytes=SDL_LoadFile(path.c_str(),&size);
            if(!bytes)Debug::FatalError("Unable to open shader %s",path.c_str());
            std::string source(static_cast<char*>(bytes),size);SDL_free(bytes);return source;
        };
        CompileSource(read(_vertexShaderFilePath),read(_fragmentShaderFilePath),_vertexShaderFilePath);
    }

    void Shader::CompileSource(const std::string &vertex, const std::string &fragment, const std::string &name)
    {
        m_path = name;
        m_vertexSource = vertex; m_fragmentSource = fragment;
        m_stereoVariant.reset(); m_stereoAttempted = false;

        if (m_programId != 0)
        {
            glDeleteProgram(m_programId);
            m_programId = 0;
        }

        if (m_fragmentShaderId != 0)
        {
            glDeleteShader(m_fragmentShaderId);
            m_fragmentShaderId = 0;
        }

        if (m_vertexShaderId != 0)
        {
            glDeleteShader(m_vertexShaderId);
            m_vertexShaderId = 0;
        }

        m_isLinked = false;
        m_locationsCashe.clear();
        m_lastValueCashe.clear();
        m_numberOfAttributes = static_cast<int>(m_attributes.size());

        //Getting vertex shaderID
        m_vertexShaderId = glCreateShader(GL_VERTEX_SHADER);
        if (m_vertexShaderId == 0)
            Debug::FatalError("Vertex shader failed to be created!");

        //Getting fragment shaderID
        m_fragmentShaderId = glCreateShader(GL_FRAGMENT_SHADER);
        if (m_fragmentShaderId == 0)
            Debug::FatalError("Fragment shader failed to be created!");

        m_programId = glCreateProgram();
        if (m_programId == 0)
            Debug::FatalError("Shader program failed to be created!");

        for (size_t i = 0; i < m_attributes.size(); ++i)
            glBindAttribLocation(m_programId, static_cast<GLuint>(i), m_attributes[i].c_str());

        CompileShaderSource(vertex, name+":vertex", m_vertexShaderId);
        CompileShaderSource(fragment, name+":fragment", m_fragmentShaderId);
    }


    Shader* Shader::StereoVariant()
    {
#ifndef __EMSCRIPTEN__
        if (m_stereoAttempted) return m_stereoVariant.get();
        m_stereoAttempted = true;
        if (!GLEW_VERSION_4_1) return nullptr;
        // Restrict this experiment to ordinary world-mesh shaders. Camera-facing
        // vertex deformation, interface blocks and explicit clip distances need
        // a separately authored stereo implementation.
        std::string vertex = std::regex_replace(m_vertexSource, std::regex(R"(/\*[\s\S]*?\*/|//[^\n]*)"), "");
        std::string fragment = m_fragmentSource;
        if (vertex.find("cameraPosition") != std::string::npos ||
            vertex.find("gl_ClipDistance") != std::string::npos ||
            vertex.find("gl_Layer") != std::string::npos ||
            vertex.find("gl_ViewportIndex") != std::string::npos ||
            vertex.find("gl_PointSize") != std::string::npos) return nullptr;
        auto countToken = [&](const char* token) {
            const std::regex pattern(std::string("\\b")+token+"\\b");
            return std::distance(std::sregex_iterator(vertex.begin(),vertex.end(),pattern),std::sregex_iterator());
        };
        // Only the usual P*V world-position transform may depend on the view.
        if(countToken("P")!=2 || countToken("V")!=2 || countToken("gl_Position")!=1 ||
           fragment.find("gl_PrimitiveID")!=std::string::npos) return nullptr;
        const std::regex output(R"(\b(?:(flat|smooth|noperspective)\s+)?out\s+(float|vec[234]|mat[234]|int|ivec[234]|uint|uvec[234])\s+(\w+)\s*;)");
        struct Varying { std::string qualifier, type, name; };
        std::vector<Varying> varyings;
        for (std::sregex_iterator i(vertex.begin(), vertex.end(), output), end; i != end; ++i)
            varyings.push_back({(*i)[1], (*i)[2], (*i)[3]});
        if (varyings.empty()) return nullptr;
        std::string geometry = "#version 410 core\nlayout(triangles, invocations=2) in;\nlayout(triangle_strip,max_vertices=3) out;\nuniform mat4 stereoView[2],stereoProjection[2];\nuniform vec3 stereoEyes[2];\n";
        for (const auto& v : varyings)
        {
            const std::string q = v.qualifier.empty() ? "" : v.qualifier + " ";
            geometry += q+"in "+v.type+" stereo_"+v.name+"[];\n"+q+"out "+v.type+" "+v.name+";\n";
            vertex = std::regex_replace(vertex, std::regex("\\b"+v.name+"\\b"), "stereo_"+v.name);
        }
        const std::regex eyeUniform(R"(uniform\s+vec3\s+cameraPosition\s*;)");
        const bool fragmentEye = std::regex_search(fragment, eyeUniform);
        if (!fragmentEye && fragment.find("cameraPosition") != std::string::npos) return nullptr;
        if (fragmentEye)
        {
            fragment = std::regex_replace(fragment, eyeUniform, "flat in vec3 cameraPosition;");
            geometry += "flat out vec3 cameraPosition;\n";
        }
        std::string position;
        // Preserve world-space precision at large scene coordinates. Reversing a
        // projected position loses depth precision and changes coplanar surfaces.
        for(const auto& v:varyings)
            if(v.type=="vec3" && (v.name=="worldPosition" || v.name=="fragmentWorldPos"))
                position="stereoProjection[eye]*stereoView[eye]*vec4(stereo_"+v.name+"[i],1)";
        if(position.empty()) return nullptr;
        geometry += "void main(){int eye=gl_InvocationID;for(int i=0;i<3;i++){gl_ViewportIndex=eye;gl_Position="+position+";";
        if (fragmentEye) geometry += "cameraPosition=stereoEyes[eye];";
        for (const auto& v : varyings) geometry += v.name+"=stereo_"+v.name+"[i];";
        geometry += "EmitVertex();}EndPrimitive();}\n";
        auto version = [](std::string source) {
            const std::string marker = "[OPENGL VERSION]";
            auto p = source.find(marker);
            if (p != std::string::npos) source.replace(p, marker.size(), "#version 410 core");
            return source;
        };
        vertex=version(vertex); fragment=version(fragment);
        GLuint shaders[3] = {glCreateShader(GL_VERTEX_SHADER),glCreateShader(GL_GEOMETRY_SHADER),glCreateShader(GL_FRAGMENT_SHADER)};
        const std::string sources[] = {vertex,geometry,fragment};
        bool valid=true;
        for (int i=0;i<3;++i)
        {
            const char* source=sources[i].c_str();glShaderSource(shaders[i],1,&source,nullptr);glCompileShader(shaders[i]);
            GLint status=0;glGetShaderiv(shaders[i],GL_COMPILE_STATUS,&status);
            if (!status) {
                char log[2048];glGetShaderInfoLog(shaders[i],sizeof(log),nullptr,log);
                Debug::Warning("Stereo variant unavailable for %s: %s",m_path.c_str(),log);valid=false;break;
            }
        }
        auto variant=std::make_unique<Shader>();
        if (valid) {
            variant->m_programId=glCreateProgram();
            for (size_t i=0;i<m_attributes.size();++i)glBindAttribLocation(variant->m_programId,i,m_attributes[i].c_str());
            for (auto shader:shaders)glAttachShader(variant->m_programId,shader);
            glLinkProgram(variant->m_programId);
            GLint status=0;glGetProgramiv(variant->m_programId,GL_LINK_STATUS,&status);valid=status!=0;
            if(!valid){char log[2048];glGetProgramInfoLog(variant->m_programId,sizeof(log),nullptr,log);Debug::Warning("Stereo link unavailable for %s: %s",m_path.c_str(),log);}
            for(auto shader:shaders)glDetachShader(variant->m_programId,shader);
        }
        for(auto shader:shaders)glDeleteShader(shader);
        if(valid){variant->m_isLinked=true;variant->m_path=m_path+":stereo";m_stereoVariant=std::move(variant);}
        return m_stereoVariant.get();
#else
        return nullptr;
#endif
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
            m_programId = 0;

            Debug::FatalError("Shader failed to link!\nOpengl Error: %s", std::string(infoLog.begin(), infoLog.end()).c_str());
        } else {
            m_isLinked = true;
        }

        glDetachShader(m_programId, m_vertexShaderId);
        glDetachShader(m_programId, m_fragmentShaderId);
        glDeleteShader(m_vertexShaderId);
        glDeleteShader(m_fragmentShaderId);
        m_vertexShaderId = 0;
        m_fragmentShaderId = 0;
    }

    void Shader::AddAttribute(const std::string &_attributeName)
    {
        if (std::find(m_attributes.begin(), m_attributes.end(), _attributeName) != m_attributes.end())
            return;

        const GLuint attributeIndex = static_cast<GLuint>(m_attributes.size());
        m_attributes.push_back(_attributeName);
        m_numberOfAttributes = static_cast<int>(m_attributes.size());

        if (m_programId != 0)
            glBindAttribLocation(m_programId, attributeIndex, _attributeName.c_str());
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


    void Shader::CompileShaderSource(std::string shaderFileCode, const std::string &_filePath, unsigned int &_id)
    {

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
