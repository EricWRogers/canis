#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <string>

#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/CastleComponent.hpp"
#include "../Components/SlimeMovementComponent.hpp"
#include "../Components/HealthComponent.hpp"

class CastleSystem
{
public:
	entt::registry *refRegistry;
	entt::entity healthText;

	void Init()
	{
		auto castleView = refRegistry->view<TransformComponent, CastleComponent>();

		for(auto [castle_entity, castle_transform, castle_component] : castleView.each())
		{
			auto [health_rect, health_text] = refRegistry->get<RectTransformComponent, TextComponent>(healthText);
						
			*health_text.text = "Health : " + std::to_string(castle_component.health);
		}
	}

	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto castleView = registry.view<TransformComponent, CastleComponent>();
		auto slimeView = registry.view<TransformComponent, SlimeMovementComponent, HealthComponent>();

		for(auto [castle_entity, castle_transform, castle_component] : castleView.each())
		{
			for(auto [slime_entity, slime_transform, slimeMovement, slimeHealth] : slimeView.each())
			{
				float distance = glm::length(castle_transform.position - slime_transform.position);

				if (distance < 0.75f)
				{
					// set slime health to zero it will destroy itself
					slimeHealth.health = 0;
					// take damage
					castle_component.health--;

					auto [health_rect, health_text] = registry.get<RectTransformComponent, TextComponent>(healthText);
					
					*health_text.text = "Health : " + std::to_string(castle_component.health);
				}
			}
		}
	}
};