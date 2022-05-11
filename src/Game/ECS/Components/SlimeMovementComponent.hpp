#pragma once
#include <glm/glm.hpp>

struct SlimeMovementComponent
{
	int targetIndex;
	int startIndex;
	int endIndex;
	float speed;
	float maxHeight;
	float minHeight;
	float beginningDistance;
};