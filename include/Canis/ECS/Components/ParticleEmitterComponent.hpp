#pragma once
#include <glm/glm.hpp>
#include <Canis/External/entt.hpp>

namespace Canis
{
    enum ParticleEmitterState
    {
        NONE      = 0,
        LOOPING   = 1,
        BURST     = 2,
        GRAVITY   = 3
    };

    struct ParticleEmitterComponent
    {
        unsigned int state = ParticleEmitterState::NONE;
        int numOfParticle = 100;
        std::vector<entt::entity> particles = {};
        int currentParticleIndex = 0;
        float lifeTime = 5.0f;
        float particleLifeTime = 2.5f;
        float time = 2.0f;
        float gravity = -9.8f;
        float speed = 2.0f;
        glm::vec3 minVelocity = glm::vec3(-0.5f,1.0f,-0.5f);
        glm::vec3 maxVelocity = glm::vec3(0.5f,1.0f,0.5f);
        glm::vec3 minRotation = glm::vec3(0.0f);
        glm::vec3 maxRotation = glm::vec3(360.0f);
        glm::vec4 colorStart = glm::vec4(1.0f);
        unsigned int modelID = 0;
        bool castShadow = false;
    };
} // end of Canis namespace