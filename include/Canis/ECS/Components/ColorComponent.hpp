#pragma once
#include <glm/glm.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
	struct ColorComponent
	{
		glm::vec4 color = glm::vec4(1.0f);
		glm::vec3 emission = glm::vec3(0.0f);
		float emissionUsingAlbedoIntesity = 0.0f;

		static void RegisterProperties()
		{
			REGISTER_PROPERTY(ColorComponent, color, glm::vec4);
			REGISTER_PROPERTY(ColorComponent, emission, glm::vec3);
			REGISTER_PROPERTY(ColorComponent, emissionUsingAlbedoIntesity, float);
		}
	};
} // end of Canis namespace