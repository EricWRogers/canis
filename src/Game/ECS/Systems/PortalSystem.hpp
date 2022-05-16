#pragma once
#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"

#include "../Components/PortalComponent.hpp"
#include "../Components/HealthComponent.hpp"

#include "../../Scripts/TileMap.hpp"

class PortalSystem
{
public:
	entt::registry *refRegistry;

	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto view = registry.view<TransformComponent, PortalComponent>();

		for(auto [portal_entity, transform, portal] : view.each())
		{
			portal.currentTime -= delta;

			if (portal.currentTime <= 0.0f)
			{
				// reset time
				portal.currentTime = portal.timeToSpawn;
				// spawn slime
				const entt::entity entity = refRegistry->create();

				refRegistry->emplace<TransformComponent>(entity,
					true, // active
					transform.position, // position
					glm::vec3(0.0f, 0.0f, 0.0f), // rotation
					GreenSlimeInfo.size // scale
				);
				refRegistry->emplace<ColorComponent>(entity,
					GreenSlimeInfo.color
				);
				refRegistry->emplace<HealthComponent>(entity,
					GreenSlimeInfo.health,// health
					GreenSlimeInfo.health // max health
				);
				refRegistry->emplace<SlimeMovementComponent>(entity,
					0u,
					1,     // targetIndex
					0,     // startIndex
					14,    // endIndex
					GreenSlimeInfo.speed,  // speed
					1.5f,  // maxHeight
					0.5f,  // minHeight
					-1.0f, // beginningDistance
					0u,     // status
					0.0f   // chillCountDown
				);
			}
		}
	}
};