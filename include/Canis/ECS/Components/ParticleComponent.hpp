#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct ParticleComponent
    {
        glm::vec3 velocity = glm::vec3();
        glm::vec3 acceleration = glm::vec3();
        float drag = 0.9f;
        float gravity = 0.0f;
        float time = 0.5f;
    };
} // end of Canis namespace