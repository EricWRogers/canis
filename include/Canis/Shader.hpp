#pragma once
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <cstddef>
//#include <glm/glm.hpp>
#include <Canis/Math.hpp>
#include <unordered_map>

namespace Canis
{
    class Shader
    {
    public:
        Shader();
        Shader(const std::string &_vertexShaderFilePath, const std::string &_fragmentShaderFilePath);
        ~Shader();

        void Compile(const std::string &_vertexShaderFilePath, const std::string &_fragmentShaderFilePath);
        void CompileSource(const std::string &vertex, const std::string &fragment, const std::string &name);
        void Link();
        // Experimental opaque stereo: one vertex submission, two geometry invocations.
        // Returns null for unsupported shader interfaces; never replaces the mono program.
        Shader* StereoVariant();
        void AddAttribute(const std::string &_attributeName);
        void Use();
        void UnUse();
        void SetBool(const std::string &_name, bool _value) const;
        void SetInt(const std::string &_name, int value) const;
        void SetFloat(const std::string &_name, float _value) const;
        void SetVec2(const std::string &_name, const Vector2 &_value) const;
        void SetVec2(const std::string &_name, float _x, float _y) const;
        void SetVec3(const std::string &_name, const Vector3 &_value) const;
        void SetVec3(const std::string &_name, float _x, float _y, float _z) const;
        void SetVec4(const std::string &_name, const Vector4 &_value) const;
        void SetVec4(const std::string &_name, float _x, float _y, float _z, float _w);
        //void SetMat2(const std::string &_name, const glm::mat2 &_mat) const;
        //void SetMat3(const std::string &_name, const glm::mat3 &_mat) const;
        void SetMat4(const std::string &_name, const Matrix4 &_mat) const;

        bool IsLinked() { return m_isLinked; }
        int GetUniformLocation(const std::string &uniformName, const size_t _valueHash) const;
        unsigned int GetUniformType(const std::string &uniformName) const;
        bool SupportsGpuSkinning() const { return m_gpuSkinning; }
        // Opt-in contract: ordinary P/V/M transforms, no vertex displacement or
        // object-space shading dependencies; instanceM is available at location 3.
        bool SupportsStaticBatching() const { return m_staticBatching; }
        int GetProgramID() { return m_programId; }

    private:
        bool m_isLinked = false;

        unsigned int m_programId = 0;
        unsigned int m_vertexShaderId = 0;
        unsigned int m_fragmentShaderId = 0;

        int m_numberOfAttributes = 0;
        std::vector<std::string> m_attributes = {};

        mutable std::unordered_map<std::string, int> m_locationsCashe = {};
        struct UniformValue
        {
            std::array<unsigned char, sizeof(Matrix4)> bytes{};
            std::size_t size = 0;
            unsigned int type = 0;
        };
        mutable std::unordered_map<int, UniformValue> m_uniformValues;
        bool UpdateUniformValue(int location, unsigned int type, const void* data, std::size_t size) const;
        std::unordered_map<std::string, unsigned int> m_uniformTypes;

        std::string m_path = "";
        std::string m_vertexSource, m_fragmentSource;
        bool m_stereoAttempted = false;
        bool m_gpuSkinning = false;
        bool m_staticBatching = false;
        unsigned int m_defaultSkinPalette = 0;
#if defined(__APPLE__)
        unsigned int m_defaultScenarioBuffer = 0;
        unsigned int m_defaultScenarioTexture = 0;
#endif
        std::unique_ptr<Shader> m_stereoVariant;

        void CompileShaderSource(std::string source, const std::string &name, unsigned int &id);
        void InitializeSkinning();
    };

} // end of Canis namespace
