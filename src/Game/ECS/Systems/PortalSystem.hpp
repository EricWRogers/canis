#pragma once
#include <string>

#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"

#include "../Components/PortalComponent.hpp"
#include "../Components/HealthComponent.hpp"

#include "../../Scripts/TileMap.hpp"

class PortalSystem
{
private:
	Wave wave;
	int currentWaveCount = -1;
	int NumSpawned = 0;
    int NumKilledInRound = 0;

public:
	entt::registry *refRegistry;

	void SlimeKilled()
	{
		NumKilledInRound++;
	}

	void UpdateCurrentWave()
    {
		currentWaveCount++;
		NumSpawned = 0;
		NumKilledInRound = 0;

		Canis::Log("Current Wave : " + std::to_string(currentWaveCount));
        //if(World.Instance.EndlessMode)
        //{
			wave.enemies.clear();

            if ((currentWaveCount + 1) < 3)
            {
                wave.delay = 1.0f;
                wave.enemies = {0,0,0,1,1,1};
            }
            else if((currentWaveCount + 1) < 5)
            {
                wave.delay = 0.8f;
                wave.enemies = {0,0,0,1,1,1,2};
            }
            else if((currentWaveCount + 1) < 8)
            {
                wave.delay = 0.7f;
                wave.enemies = {0,0,0,1,1,1,2,2,2};
            }
            else if((currentWaveCount + 1) < 10)
            {
                wave.delay = 0.6f;
                wave.enemies = {0,0,1,1,1,2,2,2,3,3};
            }
            else if((currentWaveCount + 1) < 20)
            {
                wave.delay = 0.5f;
                wave.enemies = {0,0,1,1,2,2,3,3,3,3,3,2,2,2};
            }
            else
            {
                wave.delay = 0.1f;
                wave.enemies = {2,3,3,3,2,2,3,3,3,3,3,2,2,2};
            }
        /*}
        else
        {
            CurrentWave = Waves[CurrentWaveCount];
        }*/
    }

	SlimeInfo SpawnSlime(unsigned int type)
	{
		switch(type)
		{
			case SlimeType::GREEN:
				return GreenSlimeInfo;
			case SlimeType::BLUE:
				return BlueSlimeInfo;
			case SlimeType::PURPLE:
				return PurpleSlimeInfo;
			case SlimeType::ORANGE:
				return OrangeSlimeInfo;
			default:
				return GreenSlimeInfo;
		};
	}

	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto view = registry.view<TransformComponent, PortalComponent>();

		for(auto [portal_entity, transform, portal] : view.each())
		{
			portal.currentTime -= delta;

			if (portal.currentTime <= 0.0f)
			{
				// reset time
				portal.currentTime = wave.delay;

				// check for new game
				if (currentWaveCount == -1)
					UpdateCurrentWave();

				// check end of round
				if (NumSpawned >= wave.enemies.size())
				{
					if (NumSpawned <= NumKilledInRound)
						UpdateCurrentWave();
					else
						return;
				}

				// slime config
				unsigned int slimeType = wave.enemies[NumSpawned];
				SlimeInfo slimeInfo = SpawnSlime(slimeType);
				NumSpawned++;

				// spawn slime
				const entt::entity entity = refRegistry->create();

				refRegistry->emplace<TransformComponent>(entity,
					true, // active
					transform.position, // position
					glm::vec3(0.0f, 0.0f, 0.0f), // rotation
					slimeInfo.size // scale
				);
				refRegistry->emplace<ColorComponent>(entity,
					slimeInfo.color
				);
				refRegistry->emplace<HealthComponent>(entity,
					slimeInfo.health,// health
					slimeInfo.health // max health
				);
				refRegistry->emplace<SlimeMovementComponent>(entity,
					slimeType,
					1,     // targetIndex
					0,     // startIndex
					14,    // endIndex
					slimeInfo.speed,  // speed
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