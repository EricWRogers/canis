#pragma once
#include <glm/glm.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
	struct Color
	{
		glm::vec4 color = glm::vec4(1.0f);
		glm::vec3 emission = glm::vec3(0.0f);
		float emissionUsingAlbedoIntesity = 0.0f;

		static void RegisterProperties()
		{
			REGISTER_PROPERTY(Color, color, glm::vec4);
			REGISTER_PROPERTY(Color, emission, glm::vec3);
			REGISTER_PROPERTY(Color, emissionUsingAlbedoIntesity, float);
		}
	};
} // end of Canis namespace