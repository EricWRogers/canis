#include <Canis/Math.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

namespace Canis
{
    size_t HashCombine(size_t _seed, size_t _value)
    {
        return _seed ^ (_value + 0x9e3779b97f4a7c15ULL + (_seed << 6) + (_seed >> 2));
    }

    size_t HashVector(const Vector2 &_v)
    {
        std::hash<float> h;
        size_t out = h(_v.x);
        out = HashCombine(out, h(_v.y));
        return out;
    }

    size_t HashVector(const Vector3 &_v)
    {
        std::hash<float> h;
        size_t out = h(_v.x);
        out = HashCombine(out, h(_v.y));
        out = HashCombine(out, h(_v.z));
        return out;
    }

    size_t HashVector(const Vector4 &_v)
    {
        std::hash<float> h;
        size_t out = h(_v.x);
        out = HashCombine(out, h(_v.y));
        out = HashCombine(out, h(_v.z));
        out = HashCombine(out, h(_v.w));
        return out;
    }

    size_t HashMatrix(const Matrix4 &_m)
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

    const char *MatrixToCString(const Matrix4 &_m)
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

    Vector2 RotatePoint(Vector2 _vector, float _radian)
    {
        const float c = std::cos(_radian);
        const float s = std::sin(_radian);
        return Vector2(
            _vector.x * c - _vector.y * s,
            _vector.x * s + _vector.y * c
        );
    }

    void RotatePoint(Vector2 &_point, const float &_cosAngle, const float &_sinAngle)
    {
        const float x = _point.x;
        const float y = _point.y;
        _point.x = x * _cosAngle - y * _sinAngle;
        _point.y = x * _sinAngle + y * _cosAngle;
    }

    void RotatePointAroundPivot(Vector2 &_point, const Vector2 &_pivot, float _radian)
    {
        const float s = std::sin(-_radian);
        const float c = std::cos(-_radian);
        const Vector2 holder = _point - _pivot;
        _point.x = holder.x * c - holder.y * s;
        _point.y = holder.x * s + holder.y * c;
        _point += _pivot;
    }

    void Lerp(float &_value, const float &_min, const float &_max, const float &_fraction)
    {
        _value = _min + _fraction * (_max - _min);
    }

    void Lerp(glm::vec3 &_value, const glm::vec3 &_min, const glm::vec3 &_max, const float &_fraction)
    {
        _value = _min + _fraction * (_max - _min);
    }

    void Lerp(glm::vec4 &_value, const glm::vec4 &_min, const glm::vec4 &_max, const float &_fraction)
    {
        _value = _min + _fraction * (_max - _min);
    }

    float Lerp(float _min, float _max, float _fraction)
    {
        return _min + _fraction * (_max - _min);
    }

    float Clamp01(float _value)
    {
        if (_value < 0.0f)
            return 0.0f;

        if (_value > 1.0f)
            return 1.0f;

        return _value;
    }

    float Dot(const Vector2 &_a, const Vector2 &_b)
    {
        return glm::dot(_a, _b);
    }

    float Dot(const Vector3 &_a, const Vector3 &_b)
    {
        return glm::dot(_a, _b);
    }

    float Dot(const Vector4 &_a, const Vector4 &_b)
    {
        return glm::dot(_a, _b);
    }

    Vector2 Normalize(const Vector2 &_value)
    {
        const float lengthSquared = glm::dot(_value, _value);
        if (lengthSquared <= 0.000001f)
            return Vector2(0.0f);

        return _value * glm::inversesqrt(lengthSquared);
    }

    Vector3 Normalize(const Vector3 &_value)
    {
        const float lengthSquared = glm::dot(_value, _value);
        if (lengthSquared <= 0.000001f)
            return Vector3(0.0f);

        return _value * glm::inversesqrt(lengthSquared);
    }

    Vector4 Normalize(const Vector4 &_value)
    {
        const float lengthSquared = glm::dot(_value, _value);
        if (lengthSquared <= 0.000001f)
            return Vector4(0.0f);

        return _value * glm::inversesqrt(lengthSquared);
    }

    Vector3 ProjectOnPlane(const Vector3 &_vector, const Vector3 &_planeNormal)
    {
        const float normalLengthSquared = glm::dot(_planeNormal, _planeNormal);
        if (normalLengthSquared <= 0.000001f)
            return _vector;

        return _vector - _planeNormal * (glm::dot(_vector, _planeNormal) / normalLengthSquared);
    }

    float AngleRadians(const Vector3 &_from, const Vector3 &_to)
    {
        const float fromLengthSquared = glm::dot(_from, _from);
        const float toLengthSquared = glm::dot(_to, _to);
        if (fromLengthSquared <= 0.000001f || toLengthSquared <= 0.000001f)
            return 0.0f;

        const float denominator = std::sqrt(fromLengthSquared * toLengthSquared);
        const float dot = std::clamp(glm::dot(_from, _to) / denominator, -1.0f, 1.0f);
        return std::acos(dot);
    }

    float Angle(const Vector3 &_from, const Vector3 &_to)
    {
        return AngleRadians(_from, _to) * RAD2DEG;
    }

    float InverseLerpUnclamped(float _min, float _max, float _value)
    {
        const float range = _max - _min;
        if (std::fabs(range) <= 0.000001f)
            return 0.0f;

        return (_value - _min) / range;
    }

    float InverseLerp(float _min, float _max, float _value)
    {
        return Clamp01(InverseLerpUnclamped(_min, _max, _value));
    }

    float Pow(float _base, float _exponent)
    {
        return std::pow(_base, _exponent);
    }

    float Exp(float _value)
    {
        return std::exp(_value);
    }

    Vector3 FlattenY(const Vector3 &_value)
    {
        return Vector3(_value.x, 0.0f, _value.z);
    }

    Vector3 SafeNormalize(const Vector3 &_value, const Vector3 &_fallback)
    {
        const float lengthSquared = glm::dot(_value, _value);
        if (lengthSquared <= 0.000001f)
            return _fallback;

        return _value * glm::inversesqrt(lengthSquared);
    }
}

namespace std
{
    size_t hash<Canis::Vector2>::operator()(const Canis::Vector2 &v) const
    {
        return Canis::HashVector(v);
    }

    size_t hash<Canis::Vector3>::operator()(const Canis::Vector3 &v) const
    {
        return Canis::HashVector(v);
    }

    size_t hash<Canis::Vector4>::operator()(const Canis::Vector4 &v) const
    {
        return Canis::HashVector(v);
    }

    size_t hash<Canis::Matrix4>::operator()(const Canis::Matrix4 &m) const
    {
        return Canis::HashMatrix(m);
    }
}
