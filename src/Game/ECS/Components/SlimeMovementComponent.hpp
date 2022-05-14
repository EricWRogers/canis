#pragma once
#include <glm/glm.hpp>

struct SlimeMovementComponent
{
	unsigned int slimeType;
	int targetIndex;
	int startIndex;
	int endIndex;
	float speed;
	float maxHeight;
	float minHeight;
	float beginningDistance;
	unsigned int status;
	float chillCountDown;
};