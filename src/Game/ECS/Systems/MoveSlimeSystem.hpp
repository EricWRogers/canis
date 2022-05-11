#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/SlimeMovementComponent.hpp"
#include "../Components/HealthComponent.hpp"

#include "../../Scripts/Wallet.hpp"
#include "../../Scripts/ScoreSystem.hpp"

class MoveSlimeSystem
{
public:
	Wallet *wallet;
	ScoreSystem *scoreSystem;

	glm::vec3 slimePath[15] = {
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

	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto view = registry.view<TransformComponent, SlimeMovementComponent, HealthComponent>();

		for(auto [entity, transform, slimeMovement, health] : view.each())
		{
			if (health.health <= 0)
			{
				registry.destroy(entity);

				wallet->Earn(10);

				scoreSystem->AddPoints(10);

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
					delta
				);
			}
			else
			{
				transform.position = lerp(
					transform.position,
					slimePath[slimeMovement.targetIndex],
					delta
				);
			}

			slimeMovement.targetIndex = (distance < 0.1f) ? slimeMovement.targetIndex + 1 : slimeMovement.targetIndex;

			slimeMovement.targetIndex = (slimeMovement.endIndex < slimeMovement.targetIndex) ? slimeMovement.startIndex : slimeMovement.targetIndex;
		}
	}

private:
	glm::vec3 lerp(glm::vec3 x, glm::vec3 y, float t)
	{
		return x * (1.f - t) + y * t;
	}
};