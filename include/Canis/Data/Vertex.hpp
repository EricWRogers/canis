#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct Vertex
	{
        glm::vec3   position;
        glm::vec3   normal;
        glm::vec2   texCoords;
        glm::ivec4  boneIDs;
        glm::vec4   weights;
	};
}