#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct SpotLightComponent
	{
        // Get these values from the transform
		// glm::vec3 position;
        // glm::vec3 direction;
        float cutOff;
        float outerCutOff;
    
        float constant;
        float linear;
        float quadratic;
    
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular; 
	};
} // end of Canis namespace