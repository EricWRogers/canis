#include <Canis/Math.hpp>

#include <cmath>

namespace Canis
{
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
}
