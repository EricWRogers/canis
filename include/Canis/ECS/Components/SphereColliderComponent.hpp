#pragma once
#include <glm/glm.hpp>

#include <Canis/Data/Bit.hpp>

namespace Canis
{
	struct SphereColliderComponent
	{
		glm::vec3 center{0.0f, 0.0f, 0.0f};
		float radius{1.0f};
		unsigned int layer{BIT::ZERO};
		unsigned int mask{BIT::ZERO};
	};
} // end of Canis namespace