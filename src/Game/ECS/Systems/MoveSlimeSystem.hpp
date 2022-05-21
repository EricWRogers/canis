#pragma once
#include <vector>
#include <map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../../../Canis/GameHelper/AStar.hpp"

#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/SlimeMovementComponent.hpp"
#include "../Components/HealthComponent.hpp"

#include "../Systems/PortalSystem.hpp"

#include "../../Scripts/Wallet.hpp"
#include "../../Scripts/ScoreSystem.hpp"

#include "../../Scripts/TileMap.hpp"

class MoveSlimeSystem
{
public:
	Canis::AStar *aStar;
	Wallet *wallet;
	ScoreSystem *scoreSystem;
	PortalSystem *portalSystem;
	
	std::vector<glm::vec3> slimePath= {
		glm::vec3(2.0f, 1.0f, 1.0f),
		glm::vec3(2.0f, 1.0f, 2.0f),
		glm::vec3(2.0f, 1.0f, 3.0f),
		glm::vec3(2.0f, 1.0f, 4.0f),
		glm::vec3(2.0f, 1.0f, 5.0f),
		glm::vec3(3.0f, 1.0f, 5.0f),
		glm::vec3(4.0f, 1.0f, 5.0f),
		glm::vec3(4.0f, 1.0f, 4.0f),
		glm::vec3(4.0f, 1.0f, 3.0f),
		glm::vec3(5.0f, 1.0f, 3.0f),
		glm::vec3(6.0f, 1.0f, 3.0f),
		glm::vec3(6.0f, 1.0f, 4.0f),
		glm::vec3(6.0f, 1.0f, 5.0f),
		glm::vec3(6.0f, 1.0f, 6.0f),
		glm::vec3(6.0f, 1.0f, 7.0f)};

	void Init()
	{
		glm::vec3 portalPosition;
		glm::vec3 castlePosition;

		for (int y = 0; y < 2; y++) // this will just get the first layer
		{
			for (int x = 0; x < 23; x++)
			{
				for (int z = 0; z < 30; z++)
				{
					if (titleMap[y][z][x] == BlockTypes::DIRT)
						aStar->AddPoint(glm::vec3(x,y,z));

					if (titleMap[y][z][x] == BlockTypes::CASTLE)
						castlePosition = glm::vec3(x,0.0f,z);

					if (titleMap[y][z][x] == BlockTypes::PORTAL)
						portalPosition = glm::vec3(x,0.0f,z);

					//all_points[_to_index(cell)] = point_id
				}
			}
		}

		for (int y = 0; y < 1; y++) // this will just get the first layer
		{
			for (int x = 0; x < 23; x++)
			{
				for (int z = 0; z < 30; z++)
				{
					//for (int yOffset = -1; yOffset < 2; yOffset++) // this will just get the first layer
					//{
					if (aStar->ValidPoint( glm::vec3(x, y, z)))
					{
						for (int xOffset = -1; xOffset < 2; xOffset++)
						{
							for (int zOffset = -1; zOffset < 2; zOffset++)
							{
								if (aStar->ValidPoint( glm::vec3(x + xOffset, y, z + zOffset)))
								{
									if (glm::vec3(x + xOffset, y, z + zOffset) == glm::vec3(0.0f, 0.0f, 0.0f))
										continue;
									
									if (xOffset == 0.0f || zOffset == 0.0f)
									{
										aStar->ConnectPoints(
											aStar->GetPointByPosition(glm::vec3(x + xOffset, y, z + zOffset)),
											aStar->GetPointByPosition(glm::vec3(x, y, z))
										);
									}
								}
							}
						}
					}
					//}
				}
			}
		}

		if (aStar->ValidPoint(portalPosition) && aStar->ValidPoint(castlePosition))
		{
			slimePath = aStar->GetPath(
				aStar->GetPointByPosition(portalPosition),
				aStar->GetPointByPosition(castlePosition)
			);
		}

		for(int i = 0; i < slimePath.size(); i++)
		{
			slimePath[i].y = 1.0f;
		}
	}

	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto view = registry.view<TransformComponent, SlimeMovementComponent, HealthComponent>();

		for(auto [entity, transform, slimeMovement, health] : view.each())
		{

			float speed = slimeMovement.speed;
			unsigned int newStatus = 0;

			if (slimeMovement.status & SlimeStatus::CHILL)
			{
				slimeMovement.chillCountDown -= delta;

				//Canis::Log("Slow");

				if (slimeMovement.chillCountDown > 0.0f)
				{
					newStatus |= SlimeStatus::CHILL;
					speed *= 0.5f;
				}
			}

			slimeMovement.status = newStatus;

			if (health.health <= 0)
			{
				registry.destroy(entity);

				wallet->Earn(10);

				scoreSystem->AddPoints(10);

				portalSystem->SlimeKilled();

				continue;
			}

			float distance = glm::length(transform.position - slimePath[slimeMovement.targetIndex]);

			slimeMovement.beginningDistance = (slimeMovement.beginningDistance < 0.0f) ?
			 	glm::length(
					glm::vec3(transform.position.x, 0.0f, transform.position.z) -
					glm::vec3(slimePath[slimeMovement.targetIndex].x, 0.0f, slimePath[slimeMovement.targetIndex].z)
				) : 
			 	slimeMovement.beginningDistance;

			float flatDistance = glm::length(
				glm::vec3(transform.position.x, 0.0f, transform.position.z) -
				glm::vec3(slimePath[slimeMovement.targetIndex].x, 0.0f, slimePath[slimeMovement.targetIndex].z)
			);

			if (flatDistance > (slimeMovement.beginningDistance * 0.75))
			{
				transform.position = lerp(
					transform.position,
					slimePath[slimeMovement.targetIndex] + glm::vec3(0,1,0),
					speed * delta
				);
			}
			else
			{
				transform.position = lerp(
					transform.position,
					slimePath[slimeMovement.targetIndex],
					speed * delta
				);
			}

			slimeMovement.targetIndex = (distance < 0.1f) ? slimeMovement.targetIndex + 1 : slimeMovement.targetIndex;

			slimeMovement.targetIndex = (slimePath.size() < slimeMovement.targetIndex) ? 0 : slimeMovement.targetIndex;
		}
	}

private:
	

	glm::vec3 lerp(glm::vec3 x, glm::vec3 y, float t)
	{
		return x * (1.f - t) + y * t;
	}
};