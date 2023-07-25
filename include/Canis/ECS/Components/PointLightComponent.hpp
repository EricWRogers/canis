#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct PointLightComponent
	{
        glm::vec3 offset = glm::vec3(0.0f);
        glm::vec3 color = glm::vec3(1.0f);
        
        float linear = 0.7f;
        float quadratic = 1.8f;
        float radius;
	};
} // end of Canis namespace