#pragma once
#include <Canis/Yaml.hpp>
#include <Canis/Data/Bit.hpp>

namespace Canis
{
	struct SphereColliderComponent
	{
		glm::vec3 center{0.0f, 0.0f, 0.0f};
		float radius{1.0f};
		unsigned int layer{(unsigned int)BIT::ZERO};
		unsigned int mask{(unsigned int)BIT::ZERO};

		static void RegisterProperties()
		{
			REGISTER_PROPERTY(SphereColliderComponent, center, glm::vec3);
			REGISTER_PROPERTY(SphereColliderComponent, radius, float);
			REGISTER_PROPERTY(SphereColliderComponent, layer, unsigned int);
			REGISTER_PROPERTY(SphereColliderComponent, mask, unsigned int);
		}
	};
} // end of Canis namespace