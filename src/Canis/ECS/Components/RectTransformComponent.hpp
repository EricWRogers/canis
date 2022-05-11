#pragma once
#include <glm/glm.hpp>

struct RectTransformComponent
{
	bool active;
	glm::vec2 position;
	glm::vec2 rotation;
	float scale;
};