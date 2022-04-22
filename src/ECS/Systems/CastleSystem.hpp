#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../Canis/External/entt.hpp"

#include "../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/CastleComponent.hpp"
#include "../Components/SlimeMovementComponent.hpp"

class CastleSystem
{
public:
	entt::registry *refRegistry;

	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto castleView = registry.view<TransformComponent, CastleComponent>();
		auto slimeView = registry.view<TransformComponent, SlimeMovementComponent>();

		for(auto [castle_entity, castle_transform, castle_component] : castleView.each())
		{
			for(auto [slime_entity, slime_transform, slimeMovement] : slimeView.each())
			{
				float distance = glm::length(castle_transform.position - slime_transform.position);

				if (distance < 0.5f)
				{
					// kill slime
					refRegistry->destroy(slime_entity);
					// take damage
					castle_component.health--;
					Canis::Log("Health : " + std::to_string(castle_component.health));
				}
			}
		}
	}
};