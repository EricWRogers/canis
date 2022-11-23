#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct DirectionalLightComponent
	{
        // Get this values from the transform
		// glm::vec3 direction;
        glm::vec3 ambient = glm::vec3(0.05f, 0.05f, 0.05f);
        glm::vec3 diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
        glm::vec3 specular = glm::vec3(0.5f, 0.5f, 0.5f);
	};
} // end of Canis namespace