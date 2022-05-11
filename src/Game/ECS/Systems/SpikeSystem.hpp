#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/SpikeComponent.hpp"
#include "../Components/SlimeMovementComponent.hpp"
#include "../Components/HealthComponent.hpp"

class SpikeSystem
{
public:
	entt::registry *refRegistry;

	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto spikeView = registry.view<TransformComponent, HealthComponent, SpikeComponent>();
		auto slimeView = registry.view<TransformComponent, HealthComponent, SlimeMovementComponent>();

		for(auto [spike_entity, spike_transform, spike_health, spike_component] : spikeView.each())
		{
			for(auto [slime_entity, slime_transform, slime_health, slimeMovement] : slimeView.each())
			{
				float distance = glm::length(spike_transform.position - slime_transform.position);

                if (spike_health.health > 0)
                {
                    if (distance < 0.5f)
                    {
                        if (slime_health.health >= spike_health.health)
                        {
                            // deal damage to slime
                            slime_health.health -= spike_health.health;
                            spike_health.health = 0;
                            spike_transform.active = false;
                        }
                        else
                        {
                            // take damage
                            spike_health.health -= slime_health.health;
                            // set slime health to zero it will destroy itself
                            slime_health.health = 0;
                        }
                    }
                }
			}
		}
	}
};