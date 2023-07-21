#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct DamageText3DComponent
    {
        float lifeTime = 0.5f;
        float _timeAlive = 0.0f;
        glm::vec3 velocity = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec4 startColor = glm::vec4(1.0f,1.0f,1.0f,1.0f);
        glm::vec4 endColor = glm::vec4(1.0f,1.0f,1.0f,0.2f);
    };
} // end of Canis namespace