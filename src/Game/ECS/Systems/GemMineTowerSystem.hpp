#pragma once
#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"

#include "../Components/GemMineTowerComponent.hpp"

#include "../../Scripts/Wallet.hpp"

class GemMineTowerSystem
{
public:
    Wallet *wallet;


	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto view = registry.view<TransformComponent, GemMineTowerComponent>();

		for(auto [portal_entity, transform, tower] : view.each())
		{
            tower.currentTime -= delta;

			if (tower.currentTime <= 0.0f)
			{
				// reset time
				tower.currentTime = tower.timeToSpawn;
			    wallet->Earn(tower.gems);
            }
		}
	}
};