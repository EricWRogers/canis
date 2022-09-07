#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct Vertex
	{
		// position
        glm::vec3 position;
        // normal
        glm::vec3 normal;
        // texCoords
        glm::vec2 texCoords;
	};
}