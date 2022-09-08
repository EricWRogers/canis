#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct DirectionalLightComponent
	{
        // Get this values from the transform
		// glm::vec3 direction;
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
	};
} // end of Canis namespace