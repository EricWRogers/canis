#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct TransformComponent
	{
		bool active = true;
		glm::vec3 position = glm::vec3(0.0f);
		glm::vec3 rotation = glm::vec3(0.0f);
		glm::vec3 scale = glm::vec3(1.0f);
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		bool isDirty = true;
		bool inHierarchy = false;
	};
} // end of Canis namespace