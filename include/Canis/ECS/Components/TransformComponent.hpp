#pragma once
#include <glm/glm.hpp>

struct TransformComponent
{
	bool active;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
};