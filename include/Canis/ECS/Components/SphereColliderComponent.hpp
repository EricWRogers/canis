#pragma once
#include <glm/glm.hpp>

namespace Canis
{
	struct SphereColliderComponent
	{
		glm::vec3 center{0.0f, 0.0f, 0.0f};
		float radius{1.0f};
	};
} // end of Canis namespace