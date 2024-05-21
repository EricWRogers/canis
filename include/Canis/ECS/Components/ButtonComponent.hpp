#pragma once
#include <glm/glm.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    class Entity;

    struct ButtonComponent
    {
        std::string eventName;
        glm::vec4 baseColor = glm::vec4(0.9f,0.9f,0.9f,1.0f);
        glm::vec4 hoverColor = glm::vec4(1.0f);
        unsigned int action = 0u; // 0 = just clicked, 1 = mouse released
        bool mouseOver = false;
        float scale = 1.0f;
        float hoverScale = 1.0f;
        Entity up;
        Entity down;
        Entity left;
        Entity right;
        bool defaultSelected = false;

        static void RegisterProperties()
		{
			REGISTER_PROPERTY(Canis::ButtonComponent, eventName, std::string);
			REGISTER_PROPERTY(Canis::ButtonComponent, baseColor, glm::vec4);
            REGISTER_PROPERTY(Canis::ButtonComponent, hoverColor, glm::vec4);
			REGISTER_PROPERTY(Canis::ButtonComponent, action, unsigned int);
            REGISTER_PROPERTY(Canis::ButtonComponent, mouseOver, bool);
			REGISTER_PROPERTY(Canis::ButtonComponent, scale, float);
            REGISTER_PROPERTY(Canis::ButtonComponent, hoverScale, float);
			REGISTER_PROPERTY(Canis::ButtonComponent, up, Entity);
            REGISTER_PROPERTY(Canis::ButtonComponent, down, Entity);
			REGISTER_PROPERTY(Canis::ButtonComponent, left, Entity);
            REGISTER_PROPERTY(Canis::ButtonComponent, right, Entity);
			REGISTER_PROPERTY(Canis::ButtonComponent, defaultSelected, bool);
		}
    };
} // end of Canis namespace