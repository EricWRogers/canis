#pragma once
#include <glm/glm.hpp>

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
    };
} // end of Canis namespace