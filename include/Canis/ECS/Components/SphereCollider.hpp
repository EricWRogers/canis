#pragma once
#include <Canis/Yaml.hpp>
#include <Canis/Data/Bit.hpp>

namespace Canis
{
	struct SphereCollider
	{
		glm::vec3 center{0.0f, 0.0f, 0.0f};
		float radius{1.0f};
		unsigned int layer{(unsigned int)BIT::ZERO};
		unsigned int mask{(unsigned int)BIT::ZERO};

		static void RegisterProperties()
		{
			REGISTER_PROPERTY(SphereCollider, center, glm::vec3);
			REGISTER_PROPERTY(SphereCollider, radius, float);
			REGISTER_PROPERTY(SphereCollider, layer, unsigned int);
			REGISTER_PROPERTY(SphereCollider, mask, unsigned int);
		}
	};
} // end of Canis namespace