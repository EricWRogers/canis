#pragma once

#include <glm/glm.hpp>
#include <Canis/External/entt.hpp>

namespace Canis
{
    class Camera;
    class Window;
    class InputManager;
    class TransformComponent;
    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    const float PI = 3.14159265f;

    const float RAD2DEG = 180.0f / PI;

    Ray RayFromMouse(Camera &camera, Window &window, InputManager &inputManager);

    glm::vec2 WorldToScreenSpace(Camera &_camera, Window &_window, InputManager &_inputManager, glm::vec3 _position);

    bool HitSphere(glm::vec3 center, float radius, Ray ray);

    glm::quat RotationBetweenVectors(glm::vec3 start, glm::vec3 dest);

    void UpdateModelMatrix(TransformComponent &_transform);

	const glm::mat4& GetModelMatrix(TransformComponent &_transform);

    glm::vec3 GetGlobalPosition(TransformComponent &_transform);

    void MoveTransformPosition(TransformComponent &_transform, glm::vec3 _offset);

    void SetTransformPosition(TransformComponent &_transform, glm::vec3 _position);

    void Rotate(TransformComponent &_transform, glm::vec3 _rotate);

    void SetTransformRotation(TransformComponent &_transform, glm::vec3 _rotation);

    glm::vec3 GetTransformForward(TransformComponent &_transform);

    void LookAt(TransformComponent &_transform, glm::vec3 _target, glm::vec3 _forward);

    glm::vec3 RotateTowardsTarget(TransformComponent &_transform, const glm::vec3& _target, float _rotationSpeed, float _deltaTime);

    glm::vec3 AngleBetweenPositions( const glm::vec3 &_position, const glm::vec3 &_targetPosition, const glm::vec3 &_forward);

    float RandomFloat(float min, float max);

    glm::vec4 HexToRGBA(unsigned int _RRGGBBAA);

    glm::vec4 UIntToRGBA(unsigned int _red, unsigned int _green, unsigned int _blue, unsigned int _alpha);

    void Lerp(float &_value, const float &_min, const float &_max, const float &_fraction);

    void Lerp(glm::vec3 &_value, const glm::vec3 &_min, const glm::vec3 &_max, const float &_fraction);

    void Lerp(glm::vec4 &_value, const glm::vec4 &_min, const glm::vec4 &_max, const float &_fraction);

    void Clamp(float &_value, float _min, float _max);

    void RotatePoint(glm::vec2 &_point, const float &_cosAngle, const float &_sinAngle);

    void RotatePointAroundPivot(glm::vec2 &_point, const glm::vec2 &_pivot, float _radian);
} 