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
    ParticleEmitterSystem() : System("ParticleEmitterSystem") {}

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

                    // burst
                    if (emitter.state & ParticleEmitterState::BURST) {
                        transformComponent.active = true;
                        particleComponent.velocity.x = RandomFloat(emitter.minVelocity.x, emitter.maxVelocity.x);
                        particleComponent.velocity.y = RandomFloat(emitter.minVelocity.y, emitter.maxVelocity.y);
                        particleComponent.velocity.z = RandomFloat(emitter.minVelocity.z, emitter.maxVelocity.z);

                        particleComponent.velocity = glm::normalize(particleComponent.velocity)*emitter.speed;
                        
                        particleComponent.acceleration = glm::vec3(0.0f);
                        particleComponent.time = 0.5f;
                    }

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

            // clean up
            emitter.time -= _deltaTime;

            if (emitter.time < 0.0f) {
                for(entt::entity e : emitter.particles) {
                    _registry.destroy(e);
                }

                _registry.destroy(entity);
                continue;
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
                }
            }
        }
    }
};
} // end of Canis namespace