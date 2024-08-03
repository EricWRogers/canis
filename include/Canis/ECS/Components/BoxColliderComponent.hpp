#pragma once
#include <Canis/Yaml.hpp>
#include <Canis/Data/Bit.hpp>

namespace Canis
{
	struct BoxColliderComponent
	{
        unsigned int layer{(unsigned int)BIT::ZERO};
		unsigned int mask{(unsigned int)BIT::ZERO};
		glm::vec3 center{0.0f};
		glm::vec3 size{1.0f};

		static void RegisterProperties()
		{
			REGISTER_PROPERTY(BoxColliderComponent, layer, unsigned int);
			REGISTER_PROPERTY(BoxColliderComponent, mask, unsigned int);
            REGISTER_PROPERTY(BoxColliderComponent, center, glm::vec3);
			REGISTER_PROPERTY(BoxColliderComponent, size, glm::vec3);
		}
	};
} // end of Canis namespace