#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct ColorComponent
	{
		glm::vec4 color = glm::vec4(1.0f);
		glm::vec3 emission = glm::vec3(0.0f);
		float emissionUsingAlbedoIntesity = 0.0f;
	};
} // end of Canis namespace