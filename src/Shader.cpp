#include <Canis/ProbeLightingGLSL.hpp>
#include <Canis/SkinningGLSL.hpp>
#include <Canis/DirectLightingGLSL.hpp>
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
#include <cstring>
#include <cstdlib>

namespace Canis
{
    namespace
    {
        void InitializeLightingSamplers(unsigned int program)
        {
            // Different sampler types cannot alias even when a lighting branch is disabled.
            GLint previousProgram=0;glGetIntegerv(GL_CURRENT_PROGRAM,&previousProgram);glUseProgram(program);
            for(const auto& [name,unit] : std::array<std::pair<const char*,int>,2>{{{"canisProbeArray",8},{"canisLightingScenarioData",14}}}) {
                const GLint location=glGetUniformLocation(program,name);
                if(location>=0)glUniform1i(location,unit);
            }
            for(int i=0;i<6;++i) {
                const GLint location=glGetUniformLocation(program,("canisProbeMap"+std::to_string(i)).c_str());
                if(location>=0)glUniform1i(location,8+i);
            }
            glUseProgram(previousProgram);
        }
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
#if defined(__APPLE__)
        if (m_defaultScenarioTexture) glDeleteTextures(1, &m_defaultScenarioTexture);
        if (m_defaultScenarioBuffer) glDeleteBuffers(1, &m_defaultScenarioBuffer);
#endif
        if (m_defaultSkinPalette) glDeleteBuffers(1, &m_defaultSkinPalette);
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
        const char* disabledStatic = std::getenv("CANIS_DISABLE_STATIC_SHADER_CAPABILITIES");
        m_staticBatching = vertex.find("CANIS_STATIC_BATCHING") != std::string::npos &&
            (!disabledStatic || std::string(disabledStatic) != "1");
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
        m_gpuSkinning = false;
        m_locationsCashe.clear();
        m_uniformValues.clear();
        m_uniformTypes.clear();
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

        const std::string skinMarker = "[CANIS SKINNING]";
        std::string expandedVertex = vertex;
        if (auto at = expandedVertex.find(skinMarker); at != std::string::npos)
            expandedVertex.replace(at, skinMarker.size(), SkinningGLSL);
        m_vertexSource = expandedVertex;
        CompileShaderSource(expandedVertex, name+":vertex", m_vertexShaderId);
        std::string expandedFragment=fragment;
        const std::string marker="[CANIS PROBE LIGHTING]";
        if(auto at=expandedFragment.find(marker);at!=std::string::npos)
            expandedFragment.replace(at,marker.size(),ProbeLightingGLSL);
        const std::string directMarker="[CANIS DIRECT LIGHTING]";
        if(auto at=expandedFragment.find(directMarker);at!=std::string::npos)
            expandedFragment.replace(at,directMarker.size(),DirectLightingGLSL);
        m_fragmentSource=expandedFragment;
        CompileShaderSource(expandedFragment, name+":fragment", m_fragmentShaderId);
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
            if (p != std::string::npos) source.replace(p, marker.size(), "#version 410 core\n#define CANIS_PROBE_ARRAYS 1");
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
        if(valid){variant->m_isLinked=true;variant->InitializeSkinning();InitializeLightingSamplers(variant->m_programId);variant->m_path=m_path+":stereo";m_stereoVariant=std::move(variant);}
        return m_stereoVariant.get();
#else
        return nullptr;
#endif
    }

    void Shader::InitializeSkinning()
    {
        const auto block = glGetUniformBlockIndex(m_programId, "CanisSkinPalette");
        m_gpuSkinning = block != GL_INVALID_INDEX &&
            glGetUniformLocation(m_programId, "canisUseSkinning") >= 0;
        if (m_gpuSkinning)
        {
            glUniformBlockBinding(m_programId, block, SkinningBufferBinding);
            if (!m_defaultSkinPalette) glGenBuffers(1, &m_defaultSkinPalette);
            glBindBuffer(GL_UNIFORM_BUFFER, m_defaultSkinPalette);
            glBufferData(GL_UNIFORM_BUFFER, MaxSkinningJoints * sizeof(Matrix4), nullptr, GL_STATIC_DRAW);
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
        }
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

        InitializeSkinning();
        m_uniformTypes.clear();
        GLint uniformCount = 0, maximumNameLength = 0;
        glGetProgramiv(m_programId, GL_ACTIVE_UNIFORMS, &uniformCount);
        glGetProgramiv(m_programId, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maximumNameLength);
        std::vector<GLchar> uniformName(std::max(1, maximumNameLength));
        for (GLint index = 0; index < uniformCount; ++index)
        {
            GLsizei length = 0;
            GLint size = 0;
            GLenum type = 0;
            glGetActiveUniform(m_programId, index, uniformName.size(), &length, &size, &type, uniformName.data());
            m_uniformTypes.emplace(std::string(uniformName.data(), length), type);
        }

        InitializeLightingSamplers(m_programId);
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

    unsigned int Shader::GetUniformType(const std::string &uniformName) const
    {
        auto found = m_uniformTypes.find(uniformName);
        return found == m_uniformTypes.end() ? 0 : found->second;
    }

    GLint Shader::GetUniformLocation(const std::string &name, const size_t) const
    {
        if (auto found = m_locationsCashe.find(name); found != m_locationsCashe.end())
            return found->second;
        const GLint location = glGetUniformLocation(m_programId, name.c_str());
        m_locationsCashe.emplace(name, location);
        return location;
    }

    void Shader::Use()
    {
        glUseProgram(m_programId);
#if defined(__APPLE__)
        // Apple's driver needs complete active sampler resources even when a
        // uniform disables their branch. Actual scenario draws override this.
        if (GetUniformLocation("canisLightingScenarioData", 0) >= 0)
        {
            glActiveTexture(GL_TEXTURE14);
            glBindSampler(14, 0);
            if (!m_defaultScenarioTexture)
            {
                const float white[4] = {1, 1, 1, 1};
                glGenBuffers(1, &m_defaultScenarioBuffer);
                glBindBuffer(GL_TEXTURE_BUFFER, m_defaultScenarioBuffer);
                glBufferData(GL_TEXTURE_BUFFER, sizeof(white), white, GL_STATIC_DRAW);
                glGenTextures(1, &m_defaultScenarioTexture);
                glBindTexture(GL_TEXTURE_BUFFER, m_defaultScenarioTexture);
                glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_defaultScenarioBuffer);
                glBindBuffer(GL_TEXTURE_BUFFER, 0);
            }
            else glBindTexture(GL_TEXTURE_BUFFER, m_defaultScenarioTexture);
            glActiveTexture(GL_TEXTURE0);
        }
#endif
        if (m_gpuSkinning)
        {
            glBindBufferBase(GL_UNIFORM_BUFFER, SkinningBufferBinding, m_defaultSkinPalette);
            SetBool("canisUseSkinning", false);
        }
        for (int i = 0; i < m_numberOfAttributes; i++)
        {
            glEnableVertexAttribArray(i);
        }
    }

    void Shader::UnUse()
    {
        glUseProgram(0);
    }

    bool Shader::UpdateUniformValue(int location, unsigned int type, const void* data, std::size_t size) const
    {
        if (location < 0) return false;
        static const bool enabled = [] {
            const char* disabled = std::getenv("CANIS_DISABLE_UNIFORM_CACHE");
            return !disabled || std::string(disabled) != "1";
        }();
        if (!enabled) return true;
        auto& previous = m_uniformValues[location];
        if (previous.type == type && previous.size == size &&
            std::memcmp(previous.bytes.data(), data, size) == 0)
            return false;
        previous.type = type;
        previous.size = size;
        std::memcpy(previous.bytes.data(), data, size);
        return true;
    }

    void Shader::SetBool(const std::string &name, bool value) const
    {
        SetInt(name, value ? 1 : 0);
    }

    void Shader::SetInt(const std::string &name, int value) const
    {
        const int location = GetUniformLocation(name, 0);
        if (UpdateUniformValue(location, GL_INT, &value, sizeof(value)))
            glUniform1i(location, value);
    }

    void Shader::SetFloat(const std::string &name, float value) const
    {
        const int location = GetUniformLocation(name, 0);
        if (UpdateUniformValue(location, GL_FLOAT, &value, sizeof(value)))
            glUniform1f(location, value);
    }

    void Shader::SetVec2(const std::string &name, const Vector2 &value) const
    {
        const int location = GetUniformLocation(name, 0);
        if (UpdateUniformValue(location, GL_FLOAT_VEC2, &value.x, 2 * sizeof(float)))
            glUniform2fv(location, 1, &value.x);
    }

    void Shader::SetVec2(const std::string &name, float x, float y) const { SetVec2(name, Vector2(x, y)); }

    void Shader::SetVec3(const std::string &name, const Vector3 &value) const
    {
        const int location = GetUniformLocation(name, 0);
        if (UpdateUniformValue(location, GL_FLOAT_VEC3, &value.x, 3 * sizeof(float)))
            glUniform3fv(location, 1, &value.x);
    }

    void Shader::SetVec3(const std::string &name, float x, float y, float z) const { SetVec3(name, Vector3(x, y, z)); }

    void Shader::SetVec4(const std::string &name, const Vector4 &value) const
    {
        const int location = GetUniformLocation(name, 0);
        if (UpdateUniformValue(location, GL_FLOAT_VEC4, &value.x, 4 * sizeof(float)))
            glUniform4fv(location, 1, &value.x);
    }

    void Shader::SetVec4(const std::string &name, float x, float y, float z, float w) { SetVec4(name, Vector4(x, y, z, w)); }

    void Shader::SetMat4(const std::string &name, const Matrix4 &value) const
    {
        const int location = GetUniformLocation(name, 0);
        if (UpdateUniformValue(location, GL_FLOAT_MAT4, glm::value_ptr(value), 16 * sizeof(float)))
            glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
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

#if !defined(__EMSCRIPTEN__)
        // Probe arrays preserve one sampler binding while blending whole lighting sets.
        // The normal material pipeline otherwise keeps its GLSL 330 contract.
        if (shaderFileCode.find("samplerCubeArray") != std::string::npos)
        {
            const size_t newline=shaderFileCode.find('\n');
            if(newline!=std::string::npos && (GLEW_VERSION_4_0 || GLEW_ARB_texture_cube_map_array))
                shaderFileCode.insert(newline+1,"#extension GL_ARB_texture_cube_map_array : require\n#define CANIS_PROBE_ARRAYS 1\n");
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
