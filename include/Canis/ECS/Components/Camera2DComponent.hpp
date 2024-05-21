#pragma once
#include <glm/glm.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    struct Camera2DComponent
    {
        glm::vec2 position = glm::vec2(0.0f);
        float scale = 1.0f;

        static void RegisterProperties()
		{
			REGISTER_PROPERTY(Canis::Camera2DComponent, position, glm::vec2);
			REGISTER_PROPERTY(Canis::Camera2DComponent, scale, float);
		}
    };
} // end of Canis namespace