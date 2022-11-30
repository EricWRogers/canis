#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct RectTransformComponent
	{
		bool active = true;
		glm::vec2 position = glm::vec2(0.0f);
		glm::vec2 size = glm::vec2(1.0f);
		glm::vec2 originOffset = glm::vec2(0.0f);
		float rotation = 0.0f;
		float scale = 1.0f;
		float depth = 1.0f;
	};
} // end of Canis namespace