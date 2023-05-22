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

    Ray RayFromMouse(Camera &camera, Window &window, InputManager &inputManager);

    bool HitSphere(const glm::vec3 &center, const float &radius, const Ray &ray);

    glm::quat RotationBetweenVectors(glm::vec3 start, glm::vec3 dest);

    void UpdateModelMatrix(TransformComponent &transform);

	glm::mat4 GetModelMatrix(TransformComponent &transform);

    void MoveTransformPosition(TransformComponent &transform, glm::vec3 offset);

    void SetTransformPosition(TransformComponent &transform, glm::vec3 position);

    void RotateTransformRotation(TransformComponent &transform, glm::vec3 rotate);

    void SetTransformRotation(TransformComponent &transform, glm::vec3 rotation);

    float RandomFloat(float min, float max);
} 