#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct RectTransformComponent
	{
		bool active;
		glm::vec2 position;
		glm::vec2 rotation;
		float scale;
	};
} // end of Canis namespace