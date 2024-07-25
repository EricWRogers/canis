#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/Math.hpp>

#include <Canis/ECS/Systems/System.hpp>

#include <Canis/ECS/Components/TransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/MeshComponent.hpp>
#include <Canis/ECS/Components/SphereColliderComponent.hpp>
#include <Canis/ECS/Components/ParticleEmitterComponent.hpp>
#include <Canis/ECS/Components/ParticleComponent.hpp>

namespace Canis
{
class ParticleEmitterSystem : public System
{
private:
public:
    ParticleEmitterSystem() : System() { m_name = "Canis::ParticaleEmitterSystem"; }

    void Create() override {}

    void Ready() override {}

    void Update(entt::registry &_registry, float _deltaTime) override {
        auto view = _registry.view<TransformComponent, SphereColliderComponent, ParticleEmitterComponent>();
        for (auto [entity, transform, sphere, emitter] : view.each())
        {
            // check if the particles system is seen by camera

            Canis::UpdateModelMatrix(transform);

            // init particles
            if (emitter.particles.size() == 0) {
                emitter.time = emitter.lifeTime;
                // load particles
                for(int i = 0; i < emitter.numOfParticle; i++) {
                    Entity e = scene->CreateEntity();
                    TransformComponent& transformComponent = e.AddComponent<TransformComponent>();
                    transformComponent.registry = &(scene->entityRegistry);
                    
                    if (emitter.state & ParticleEmitterState::LOCAL)
                    {
                        transformComponent.position = glm::vec3(0.0f);
                        transformComponent.scale = glm::vec3(1.0f);

                        e.SetParent(entity);
                    }
                    else
                    {
                        transformComponent.position = Canis::GetGlobalPosition(transform);
                        transformComponent.scale = Canis::GetGlobalScale(transform);
                    }

                    transformComponent.active = false;
                    ParticleComponent particleComponent = {};

                    if (emitter.state & ParticleEmitterState::GRAVITY)
                        particleComponent.gravity = emitter.gravity;
                    
                    ColorComponent colorComponent = {};
                    colorComponent.color = emitter.colorStart;
                    colorComponent.emission = emitter.emission;
                    colorComponent.emissionUsingAlbedoIntesity = emitter.emissionUsingAlbedoIntesity;

                    MeshComponent meshComponent = {};
                    meshComponent.modelHandle.id = emitter.modelID;
                    meshComponent.castShadow = emitter.castShadow;
                    meshComponent.material = emitter.material;
                    meshComponent.castDepth = false;

                    SphereColliderComponent sphereColliderComponent = {};

                    // create entity
                    e.AddComponent<ColorComponent>(colorComponent);
                    e.AddComponent<MeshComponent>(meshComponent);
                    e.AddComponent<SphereColliderComponent>(sphereColliderComponent);
                    e.AddComponent<ParticleComponent>(particleComponent);

                    emitter.particles.push_back(e);
                }
            }

            if (emitter.state & ParticleEmitterState::BURST && !(emitter.state & ParticleEmitterState::PAUSED))
            {
                for(int i = 0; i < emitter.numOfParticle; i++)
                {
                    TransformComponent &t = _registry.get<TransformComponent>(emitter.particles[i]);
                    ColorComponent &c = _registry.get<ColorComponent>(emitter.particles[i]);
                    ParticleComponent &p = _registry.get<ParticleComponent>(emitter.particles[i]);

                    if (emitter.state & ParticleEmitterState::LOCAL)
                    {
                        t.position = glm::vec3(0.0f);
                        t.scale = glm::vec3(1.0f);
                    }
                    else
                    {
                        t.position = Canis::GetGlobalPosition(transform);
                        t.scale = Canis::GetGlobalScale(transform);
                    }

                    c.color = emitter.colorStart;

                    float scaleMul = RandomFloat(emitter.minScalePercentage, emitter.maxScalePercentage);
                    t.scale = transform.scale*scaleMul;
                    t.isDirty = true;
                    t.active = true;
                    
                    p.velocity.x = RandomFloat(emitter.minVelocity.x, emitter.maxVelocity.x);
                    p.velocity.y = RandomFloat(emitter.minVelocity.y, emitter.maxVelocity.y);
                    p.velocity.z = RandomFloat(emitter.minVelocity.z, emitter.maxVelocity.z);

                    p.velocity = glm::normalize(p.velocity)*emitter.speed;
                    
                    p.acceleration = glm::vec3(0.0f);
                    p.time = emitter.particleLifeTime;
                }

                emitter.state |= ParticleEmitterState::PAUSED;
            }

            // clean up
            emitter.time -= _deltaTime;

            if (!(emitter.state & ParticleEmitterState::BURST) && !(emitter.state & ParticleEmitterState::PAUSED)) {
                emitter._spawnCountDown -= _deltaTime;

                if (emitter._spawnCountDown < 0.0f) {
                    emitter._spawnCountDown = emitter.timeTillSpawn;
                    int numHaveSpawned = 0;

                    for(int i = 0; i < emitter.numOfParticle; i++) {
                        TransformComponent &t = _registry.get<TransformComponent>(emitter.particles[i]);

                        if (t.active == false) {
                            numHaveSpawned++;

                            ColorComponent &c = _registry.get<ColorComponent>(emitter.particles[i]);
                            ParticleComponent &p = _registry.get<ParticleComponent>(emitter.particles[i]);

                            float scaleMul = RandomFloat(emitter.minScalePercentage, emitter.maxScalePercentage);

                            if (emitter.state & ParticleEmitterState::LOCAL)
                            {
                                t.position = glm::vec3(0.0f);
                                t.scale = glm::vec3(scaleMul);
                            }
                            else
                            {
                                t.position = Canis::GetGlobalPosition(transform);
                                t.scale = transform.scale*scaleMul;
                            }

                            c.color = emitter.colorStart;

                            t.active = true;

                            SetTransformRotation(t,
                                glm::vec3(
                                    RandomFloat(emitter.minRotation.x,emitter.maxRotation.x),
                                    RandomFloat(emitter.minRotation.y,emitter.maxRotation.y),
                                    RandomFloat(emitter.minRotation.z,emitter.maxRotation.z)
                                )
                            );

                            t.isDirty = true;

                            p.velocity.x = RandomFloat(emitter.minVelocity.x, emitter.maxVelocity.x);
                            p.velocity.y = RandomFloat(emitter.minVelocity.y, emitter.maxVelocity.y);
                            p.velocity.z = RandomFloat(emitter.minVelocity.z, emitter.maxVelocity.z);

                            p.velocity = glm::normalize(p.velocity)*emitter.speed;
                            
                            p.acceleration = glm::vec3(0.0f);
                            p.time = emitter.particleLifeTime;

                            if(numHaveSpawned >= emitter.numToSpawn) {
                                break;
                            }
                        }
                    }
                }
            }

            if (emitter.time < 0.0f)
            {
                if (emitter.state & ParticleEmitterState::LOOPING)
                {
                    emitter.time = emitter.lifeTime;
                }
                else if (emitter.state & ParticleEmitterState::DESTROYONEND)
                {
                    Entity emitterEntity(entity, scene);
                    // the future having a pool of particels that is shared would be better
                    if ((emitter.state & ParticleEmitterState::LOCAL) == false)
                    {
                        for(entt::entity e : emitter.particles)
                        {
                            _registry.destroy(e);
                        }

                        emitter.particles.clear();
                    }

                    emitterEntity.Destroy();
                    
                    continue;
                }
            }

            // update particles
            for(int i = 0; i < emitter.numOfParticle; i++) {
                TransformComponent &t = _registry.get<TransformComponent>(emitter.particles[i]);

                if (t.active) {
                    ColorComponent &c = _registry.get<ColorComponent>(emitter.particles[i]);
                    ParticleComponent &p = _registry.get<ParticleComponent>(emitter.particles[i]);

                    p.velocity.y = p.velocity.y + (p.gravity*_deltaTime);
                    //p.velocity *= p.drag;

                    t.position += (p.velocity*_deltaTime);

                    t.isDirty = true;

                    p.time -= _deltaTime;

                    if (emitter.state & ParticleEmitterState::COLORLERP) { // get rid of div
                        Canis::Lerp(c.color, emitter.colorStart, emitter.colorEnd, 1 - (p.time/emitter.particleLifeTime));
                    }

                    if (p.time <= 0.0f) {
                        t.active = false;
                    }
                }
            }
        }
    }
};
} // end of Canis namespace