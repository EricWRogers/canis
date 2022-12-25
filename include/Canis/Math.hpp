#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Camera.hpp"
#include "Window.hpp"
#include "InputManager.hpp"

#include "ECS/Components/TransformComponent.hpp"

namespace Canis
{
    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    Ray RayFromMouse(Camera &camera, Window &window, InputManager &inputManager);

    bool HitSphere(glm::vec3 center, float radius, Ray ray);

    glm::quat RotationBetweenVectors(glm::vec3 start, glm::vec3 dest);

    void UpdateModelMatrix(TransformComponent &transform);

	glm::mat4 GetModelMatrix(TransformComponent &transform);

    void MoveTransformPosition(TransformComponent &transform, glm::vec3 offset);

    void SetTransformPosition(TransformComponent &transform, glm::vec3 position);

    void RotateTransformRotation(TransformComponent &transform, glm::vec3 rotate);

    void SetTransformRotation(TransformComponent &transform, glm::vec3 rotation);

    float RandomFloat(float min, float max);
} 