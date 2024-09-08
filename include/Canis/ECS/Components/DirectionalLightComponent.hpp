#pragma once
#include <glm/glm.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
	struct DirectionalLightComponent
	{
        // Get this values from the transform
		glm::vec3 direction;
        glm::vec3 ambient = glm::vec3(0.05f, 0.05f, 0.05f);
        glm::vec3 diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
        glm::vec3 specular = glm::vec3(0.5f, 0.5f, 0.5f);

        static void RegisterProperties()
		{
			REGISTER_PROPERTY(DirectionalLightComponent, direction, glm::vec3);
			REGISTER_PROPERTY(DirectionalLightComponent,   ambient, glm::vec3);
			REGISTER_PROPERTY(DirectionalLightComponent,   diffuse, glm::vec3);
			REGISTER_PROPERTY(DirectionalLightComponent,  specular, glm::vec3);
		}
	};
} // end of Canis namespace