#pragma once
#include <glm/glm.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
	struct DirectionalLight
	{
        // Get this values from the transform
		glm::vec3 direction;
        glm::vec3 ambient = glm::vec3(0.05f, 0.05f, 0.05f);
        glm::vec3 diffuse = glm::vec3(0.8f, 0.8f, 0.8f);
        glm::vec3 specular = glm::vec3(0.5f, 0.5f, 0.5f);

        static void RegisterProperties()
		{
			REGISTER_PROPERTY(DirectionalLight, direction, glm::vec3);
			REGISTER_PROPERTY(DirectionalLight,   ambient, glm::vec3);
			REGISTER_PROPERTY(DirectionalLight,   diffuse, glm::vec3);
			REGISTER_PROPERTY(DirectionalLight,  specular, glm::vec3);
		}
	};
} // end of Canis namespace