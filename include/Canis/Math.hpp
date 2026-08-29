#pragma once
#ifndef CANIS_MATH_HPP
#define CANIS_MATH_HPP

#include <cstddef>
#include <functional>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <Canis/Data/Types.hpp>

namespace Canis
{
    struct Transform;

    inline constexpr float PI = 3.14159265f;
    inline constexpr float TAU = 6.28318530718f;
    inline constexpr float RAD2DEG = 180.0f / PI;
    inline constexpr float DEG2RAD = PI / 180.0f;

    using Vector2 = glm::vec2;
    using Vector3 = glm::vec3;
    using Vector4 = glm::vec4;
    using Matrix4 = glm::mat4;
    using Quaternion = glm::quat;
    using Color = Vector4;

    inline const Vector2 VECTOR2_ZERO = Vector2(0.0f);

    size_t HashCombine(size_t _seed, size_t _value);
    size_t HashVector(const Vector2 &_v);
    size_t HashVector(const Vector3 &_v);
    size_t HashVector(const Vector4 &_v);
    size_t HashMatrix(const Matrix4 &_m);
    const char *MatrixToCString(const Matrix4 &_m);

    Vector2 RotatePoint(Vector2 _vector, float _radian);
    void RotatePoint(Vector2 &_point, const float &_cosAngle, const float &_sinAngle);
    void RotatePointAroundPivot(Vector2 &_point, const Vector2 &_pivot, float _radian);

    void Lerp(float &_value, const float &_min, const float &_max, const float &_fraction);
    void Lerp(glm::vec3 &_value, const glm::vec3 &_min, const glm::vec3 &_max, const float &_fraction);
    void Lerp(glm::vec4 &_value, const glm::vec4 &_min, const glm::vec4 &_max, const float &_fraction);

    float Lerp(float _min, float _max, float _fraction);

    float Clamp01(float _value);

    float Dot(const Vector2 &_a, const Vector2 &_b);
    float Dot(const Vector3 &_a, const Vector3 &_b);
    float Dot(const Vector4 &_a, const Vector4 &_b);

    Vector2 Normalize(const Vector2 &_value);
    Vector3 Normalize(const Vector3 &_value);
    Vector4 Normalize(const Vector4 &_value);

    Vector3 ProjectOnPlane(const Vector3 &_vector, const Vector3 &_planeNormal);

    float AngleRadians(const Vector3 &_from, const Vector3 &_to);
    float Angle(const Vector3 &_from, const Vector3 &_to);

    float InverseLerpUnclamped(float _min, float _max, float _value);
    float InverseLerp(float _min, float _max, float _value);

    float Pow(float _base, float _exponent);
    float Exp(float _value);

    Vector3 FlattenY(const Vector3 &_value);
    Vector3 SafeNormalize(const Vector3 &_value, const Vector3 &_fallback = Vector3(0.0f));

    Quaternion RotationBetweenVectors(Vector3 _start, Vector3 _destination);
    Quaternion RotateTowards(Quaternion _from, Quaternion _to, float _maxAngleRadians);

    void Rotate(Transform &_transform, const Vector3 &_eulerRadians);
    void SetTransformRotation(Transform &_transform, const Vector3 &_eulerRadians);
    void SetTransformRotation(Transform &_transform, const Quaternion &_rotation);
    Vector3 GetTransformForward(const Transform &_transform);
    Vector3 GetTransformRight(const Transform &_transform);
    void LookAt(Transform &_transform, const Vector3 &_target, const Vector3 &_up = Vector3(0.0f, 1.0f, 0.0f));
    void LookAtZeroY(Transform &_transform, const Vector3 &_target, const Vector3 &_up = Vector3(0.0f, 1.0f, 0.0f));
    void RotateTowardsLookAt(Transform &_transform, const Vector3 &_target, const Vector3 &_up, float _maxAngleRadians);
    void RotateTowardsLookAtYAxis(Transform &_transform, const Vector3 &_target, const Vector3 &_up, float _maxAngleRadians);
}

namespace std
{
    template<>
    struct hash<Canis::Vector2>
    {
        size_t operator()(const Canis::Vector2 &v) const;
    };

    template<>
    struct hash<Canis::Vector3>
    {
        size_t operator()(const Canis::Vector3 &v) const;
    };

    template<>
    struct hash<Canis::Vector4>
    {
        size_t operator()(const Canis::Vector4 &v) const;
    };

    template<>
    struct hash<Canis::Matrix4>
    {
        size_t operator()(const Canis::Matrix4 &m) const;
    };
}

#endif
