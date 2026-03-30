#pragma once

#include <cstddef>
#include <functional>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Canis/Data/Types.hpp>

namespace Canis
{
    inline constexpr float PI = 3.14159265f;
    inline constexpr float RAD2DEG = 180.0f / PI;
    inline constexpr float DEG2RAD = PI / 180.0f;

    using Vector2 = glm::vec2;
    using Vector3 = glm::vec3;
    using Vector4 = glm::vec4;
    using Matrix4 = glm::mat4;
    using Color = Vector4;

    inline const Vector2 VECTOR2_ZERO = Vector2(0.0f);

    inline size_t HashCombine(size_t _seed, size_t _value)
    {
        return _seed ^ (_value + 0x9e3779b97f4a7c15ULL + (_seed << 6) + (_seed >> 2));
    }

    inline size_t HashVector(const Vector2 &_v)
    {
        std::hash<float> h;
        size_t out = h(_v.x);
        out = HashCombine(out, h(_v.y));
        return out;
    }

    inline size_t HashVector(const Vector3 &_v)
    {
        std::hash<float> h;
        size_t out = h(_v.x);
        out = HashCombine(out, h(_v.y));
        out = HashCombine(out, h(_v.z));
        return out;
    }

    inline size_t HashVector(const Vector4 &_v)
    {
        std::hash<float> h;
        size_t out = h(_v.x);
        out = HashCombine(out, h(_v.y));
        out = HashCombine(out, h(_v.z));
        out = HashCombine(out, h(_v.w));
        return out;
    }

    inline size_t HashMatrix(const Matrix4 &_m)
    {
        std::hash<float> h;
        size_t out = 0;
        for (int col = 0; col < 4; ++col)
        {
            for (int row = 0; row < 4; ++row)
                out = HashCombine(out, h(_m[col][row]));
        }
        return out;
    }

    inline const char *MatrixToCString(const Matrix4 &_m)
    {
        static thread_local std::string s;
        s.clear();
        s.reserve(256);

        for (int row = 0; row < 4; ++row)
        {
            s += "[ ";
            s += std::to_string(_m[0][row]);
            s += " ";
            s += std::to_string(_m[1][row]);
            s += " ";
            s += std::to_string(_m[2][row]);
            s += " ";
            s += std::to_string(_m[3][row]);
            s += " ]";
            if (row < 3)
                s += "\n";
        }

        return s.c_str();
    }

    Vector2 RotatePoint(Vector2 _vector, float _radian);
    void RotatePoint(Vector2 &_point, const float &_cosAngle, const float &_sinAngle);
    void RotatePointAroundPivot(Vector2 &_point, const Vector2 &_pivot, float _radian);
    
    void Lerp(float &_value, const float &_min, const float &_max, const float &_fraction);

    void Lerp(glm::vec3 &_value, const glm::vec3 &_min, const glm::vec3 &_max, const float &_fraction);

    void Lerp(glm::vec4 &_value, const glm::vec4 &_min, const glm::vec4 &_max, const float &_fraction);

    float Lerp(float _min, float _max, float _fraction);
}

namespace std
{
    template<>
    struct hash<Canis::Vector2>
    {
        size_t operator()(const Canis::Vector2 &v) const
        {
            return Canis::HashVector(v);
        }
    };

    template<>
    struct hash<Canis::Vector3>
    {
        size_t operator()(const Canis::Vector3 &v) const
        {
            return Canis::HashVector(v);
        }
    };

    template<>
    struct hash<Canis::Vector4>
    {
        size_t operator()(const Canis::Vector4 &v) const
        {
            return Canis::HashVector(v);
        }
    };

    template<>
    struct hash<Canis::Matrix4>
    {
        size_t operator()(const Canis::Matrix4 &m) const
        {
            return Canis::HashMatrix(m);
        }
    };
}
