#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct PointLightComponent
	{
        // Get this values from the transform
		// glm::vec3 position;
    
        float constant;
        float linear;
        float quadratic;
        
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
	};
} // end of Canis namespace