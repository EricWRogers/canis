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
    ParticleEmitterSystem() : System() {}

    void Create() override {}

    void Ready() override {}

    void Update(entt::registry &_registry, float _deltaTime) override {
        auto view = _registry.view<TransformComponent, SphereColliderComponent, ParticleEmitterComponent>();
        for (auto [entity, transform, sphere, emitter] : view.each())
        {
            // check if the particles system is seen by camera

            // init particles
            if (emitter.particles.size() == 0) {
                emitter.time = emitter.lifeTime;
                // load particles
                for(int i = 0; i < emitter.numOfParticle; i++) {
                    TransformComponent transformComponent = {};
                    transformComponent = transform;
                    transformComponent.active = false;
                    ParticleComponent particleComponent = {};

                    if (emitter.state & ParticleEmitterState::GRAVITY)
                        particleComponent.gravity = emitter.gravity;
                    
                    ColorComponent colorComponent = {};
                    colorComponent.color = emitter.colorStart;
                    colorComponent.emission = emitter.emission;
                    colorComponent.emissionUsingAlbedoIntesity = emitter.emissionUsingAlbedoIntesity;

                    MeshComponent meshComponent = {};
                    meshComponent.id = emitter.modelID;
                    meshComponent.castShadow = emitter.castShadow;
                    meshComponent.vao = assetManager->Get<Canis::ModelAsset>(meshComponent.id)->GetVAO();
                    meshComponent.size = assetManager->Get<Canis::ModelAsset>(meshComponent.id)->GetSize();

                    SphereColliderComponent sphereColliderComponent = {};

                    // create entity
                    entt::entity e = _registry.create();
                    _registry.emplace<TransformComponent>(e, transformComponent);
                    _registry.emplace<ColorComponent>(e, colorComponent);
                    _registry.emplace<MeshComponent>(e, meshComponent);
                    _registry.emplace<SphereColliderComponent>(e, sphereColliderComponent);
                    _registry.emplace<ParticleComponent>(e, particleComponent);

                    emitter.particles.push_back(e);
                }
            }

            if (emitter.time == emitter.lifeTime) {
                if (emitter.state & ParticleEmitterState::BURST) {
                    for(int i = 0; i < emitter.numOfParticle; i++) {
                        TransformComponent &t = _registry.get<TransformComponent>(emitter.particles[i]);
                        ColorComponent &c = _registry.get<ColorComponent>(emitter.particles[i]);
                        ParticleComponent &p = _registry.get<ParticleComponent>(emitter.particles[i]);

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
                }
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

                            t = transform;

                            c.color = emitter.colorStart;

                            t.active = true;
                            t.rotation.x = RandomFloat(emitter.minRotation.x,emitter.maxRotation.x);
                            t.rotation.y = RandomFloat(emitter.minRotation.y,emitter.maxRotation.y);
                            t.rotation.z = RandomFloat(emitter.minRotation.z,emitter.maxRotation.z);
                            float scaleMul = RandomFloat(emitter.minScalePercentage, emitter.maxScalePercentage);
                            t.scale = transform.scale*scaleMul;
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

            if (emitter.time < 0.0f) {
                if (emitter.state & ParticleEmitterState::LOOPING) {
                    emitter.time = emitter.lifeTime;
                }
                else {
                    for(entt::entity e : emitter.particles) {
                        _registry.destroy(e);
                    }

                    _registry.destroy(entity);
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