#pragma once

#include <glm/glm.hpp>

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

    void UpdateModelMatrix(TransformComponent &transform);

	glm::mat4 GetModelMatrix(TransformComponent &transform);
} 