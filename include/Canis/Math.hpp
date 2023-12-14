#pragma once

#include <glm/glm.hpp>

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

    void UpdateModelMatrix(TransformComponent &transform);

	glm::mat4 GetModelMatrix(TransformComponent &transform);

    glm::vec3 GetGlobalPosition(TransformComponent &_transform);

    void MoveTransformPosition(TransformComponent &transform, glm::vec3 offset);

    void SetTransformPosition(TransformComponent &transform, glm::vec3 position);

    void RotateTransformRotation(TransformComponent &transform, glm::vec3 rotate);

    void SetTransformRotation(TransformComponent &transform, glm::vec3 rotation);

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