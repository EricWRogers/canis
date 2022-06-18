#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct TransformComponent
	{
		bool active;
		glm::vec3 position;
		glm::vec3 rotation;
		glm::vec3 scale;
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		bool isDirty = true;
	};
} // end of Canis namespace