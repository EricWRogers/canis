#pragma once

#include <glm/glm.hpp>
#include <Canis/External/entt.hpp>
#include <Canis/Entity.hpp>

using namespace glm;

namespace Canis
{
    class Camera;
    class Window;
    class InputManager;
    class TransformComponent;

    struct Ray
    {
        vec3 origin;
        vec3 direction;

        Ray(vec3 _origin, vec3 _direction)
        {
            origin = _origin;
            direction = _direction;
        }
    };

    struct Hit
    {
        Entity entity;
        vec3 position;
    };

    const float PI = 3.14159265f;

    const float RAD2DEG = 180.0f / PI;

    bool FindRayMeshIntersection(Entity _entity, Ray _ray, Hit &_hit);

    bool CheckRay(Entity _entity, Ray _ray, Hit &_hit);

    Ray RayFromMouse(Camera &camera, Window &window, InputManager &inputManager);

    vec2 WorldToScreenSpace(Camera &_camera, Window &_window, InputManager &_inputManager, vec3 _position);

    bool HitSphere(vec3 center, float radius, Ray ray);

    quat RotationBetweenVectors(vec3 start, vec3 dest);

    void UpdateModelMatrix(TransformComponent &_transform);

	const mat4& GetModelMatrix(TransformComponent &_transform);

    vec3 GetGlobalPosition(TransformComponent &_transform);

    vec3 GetGlobalRotation(TransformComponent &_transform);

    vec3 GetGlobalScale(TransformComponent &_transform);

    void MoveTransformPosition(TransformComponent &_transform, vec3 _offset);

    void SetTransformPosition(TransformComponent &_transform, vec3 _position);

    void Rotate(TransformComponent &_transform, vec3 _rotate);

    void SetTransformRotation(TransformComponent &_transform, vec3 _rotation);

    void SetTransformRotation(TransformComponent &_transform, quat _rotation);

    void SetTransformScale(TransformComponent &_transform, vec3 _scale);

    vec3 GetTransformForward(TransformComponent &_transform);

    vec3 GetTransformRight(TransformComponent &_transform);

    void LookAt(TransformComponent &_transform, vec3 _target, vec3 _up);

    quat RotateTowards(quat _q1, quat _q2, float _maxAngle);

    void RotateTowardsLookAt(TransformComponent &_transform, vec3 _target, vec3 _up, float _maxAngle);

    float RandomFloat(float min, float max);

    vec4 HexToRGBA(unsigned int _RRGGBBAA);

    vec4 UIntToRGBA(unsigned int _red, unsigned int _green, unsigned int _blue, unsigned int _alpha);

    void Lerp(float &_value, const float &_min, const float &_max, const float &_fraction);

    void Lerp(vec3 &_value, const vec3 &_min, const vec3 &_max, const float &_fraction);

    void Lerp(vec4 &_value, const vec4 &_min, const vec4 &_max, const float &_fraction);

    void AnimationBellCurve(float &_value, const float &_min, const float &_max, const float &_fraction);

    void Clamp(float &_value, float _min, float _max);

    void RotatePoint(vec2 &_point, const float &_cosAngle, const float &_sinAngle);

    void RotatePointAroundPivot(vec2 &_point, const vec2 &_pivot, float _radian);
} 