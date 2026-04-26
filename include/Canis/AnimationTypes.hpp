#pragma once

#include <algorithm>
#include <cmath>

#include <Canis/Math.hpp>

namespace Canis
{
    enum class AnimationValueType
    {
        NONE = 0,
        FLOAT,
        INT,
        BOOL,
        VEC2,
        VEC3,
        VEC4,
    };

    enum class AnimationInterpolation
    {
        LINEAR = 0,
        STEP,
    };

    struct AnimationValue
    {
        AnimationValueType type = AnimationValueType::NONE;
        Vector4 vector = Vector4(0.0f);
        int intValue = 0;
        bool boolValue = false;

        static AnimationValue Float(float _value)
        {
            AnimationValue value = {};
            value.type = AnimationValueType::FLOAT;
            value.vector.x = _value;
            return value;
        }

        static AnimationValue Int(int _value)
        {
            AnimationValue value = {};
            value.type = AnimationValueType::INT;
            value.intValue = _value;
            value.vector.x = static_cast<float>(_value);
            return value;
        }

        static AnimationValue Bool(bool _value)
        {
            AnimationValue value = {};
            value.type = AnimationValueType::BOOL;
            value.boolValue = _value;
            value.vector.x = _value ? 1.0f : 0.0f;
            return value;
        }

        static AnimationValue Vec2(const Vector2 &_value)
        {
            AnimationValue value = {};
            value.type = AnimationValueType::VEC2;
            value.vector = Vector4(_value, 0.0f, 0.0f);
            return value;
        }

        static AnimationValue Vec3(const Vector3 &_value)
        {
            AnimationValue value = {};
            value.type = AnimationValueType::VEC3;
            value.vector = Vector4(_value, 0.0f);
            return value;
        }

        static AnimationValue Vec4(const Vector4 &_value)
        {
            AnimationValue value = {};
            value.type = AnimationValueType::VEC4;
            value.vector = _value;
            return value;
        }

        float AsFloat(float _default = 0.0f) const
        {
            switch (type)
            {
                case AnimationValueType::FLOAT: return vector.x;
                case AnimationValueType::INT: return static_cast<float>(intValue);
                case AnimationValueType::BOOL: return boolValue ? 1.0f : 0.0f;
                default: return _default;
            }
        }

        int AsInt(int _default = 0) const
        {
            switch (type)
            {
                case AnimationValueType::INT: return intValue;
                case AnimationValueType::FLOAT: return static_cast<int>(std::round(vector.x));
                case AnimationValueType::BOOL: return boolValue ? 1 : 0;
                default: return _default;
            }
        }

        bool AsBool(bool _default = false) const
        {
            switch (type)
            {
                case AnimationValueType::BOOL: return boolValue;
                case AnimationValueType::INT: return intValue != 0;
                case AnimationValueType::FLOAT: return std::fabs(vector.x) > 0.5f;
                default: return _default;
            }
        }

        Vector2 AsVec2(const Vector2 &_default = Vector2(0.0f)) const
        {
            return (type == AnimationValueType::VEC2 || type == AnimationValueType::VEC3 || type == AnimationValueType::VEC4)
                ? Vector2(vector.x, vector.y)
                : _default;
        }

        Vector3 AsVec3(const Vector3 &_default = Vector3(0.0f)) const
        {
            return (type == AnimationValueType::VEC3 || type == AnimationValueType::VEC4)
                ? Vector3(vector.x, vector.y, vector.z)
                : _default;
        }

        Vector4 AsVec4(const Vector4 &_default = Vector4(0.0f)) const
        {
            return type == AnimationValueType::VEC4 ? vector : _default;
        }
    };

    inline int GetAnimationValueComponentCount(AnimationValueType _type)
    {
        switch (_type)
        {
            case AnimationValueType::FLOAT:
            case AnimationValueType::INT:
            case AnimationValueType::BOOL:
                return 1;
            case AnimationValueType::VEC2:
                return 2;
            case AnimationValueType::VEC3:
                return 3;
            case AnimationValueType::VEC4:
                return 4;
            default:
                return 0;
        }
    }

    inline bool AnimationValueIsDiscrete(AnimationValueType _type)
    {
        return _type == AnimationValueType::INT || _type == AnimationValueType::BOOL;
    }

    inline AnimationInterpolation GetDefaultAnimationInterpolation(AnimationValueType _type)
    {
        return AnimationValueIsDiscrete(_type) ? AnimationInterpolation::STEP : AnimationInterpolation::LINEAR;
    }

    inline AnimationValue LerpAnimationValue(const AnimationValue &_a, const AnimationValue &_b, float _t)
    {
        const float t = std::clamp(_t, 0.0f, 1.0f);

        if (_a.type != _b.type)
            return (t < 0.5f) ? _a : _b;

        switch (_a.type)
        {
            case AnimationValueType::FLOAT:
                return AnimationValue::Float(glm::mix(_a.AsFloat(), _b.AsFloat(), t));
            case AnimationValueType::VEC2:
                return AnimationValue::Vec2(glm::mix(_a.AsVec2(), _b.AsVec2(), t));
            case AnimationValueType::VEC3:
                return AnimationValue::Vec3(glm::mix(_a.AsVec3(), _b.AsVec3(), t));
            case AnimationValueType::VEC4:
                return AnimationValue::Vec4(glm::mix(_a.AsVec4(), _b.AsVec4(), t));
            case AnimationValueType::INT:
            case AnimationValueType::BOOL:
            case AnimationValueType::NONE:
            default:
                return (t < 1.0f) ? _a : _b;
        }
    }
}
