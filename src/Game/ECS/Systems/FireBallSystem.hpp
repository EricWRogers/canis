#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/SlimeMovementComponent.hpp"
#include "../Components/HealthComponent.hpp"
#include "../Components/FireBallComponent.hpp"

class FireBallSystem
{
public:

	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto fireBallView = registry.view<TransformComponent, FireBallComponent>();
		auto slimeView = registry.view<TransformComponent, HealthComponent, SlimeMovementComponent>();


		for(auto [fire_ball_entity, fire_ball_transform, fire_ball_component] : fireBallView.each())
		{
            //continue;
            if (fire_ball_transform.position.y < -1)
            {
                registry.destroy(fire_ball_entity);
                continue;
            }
            // move fire ball
            glm::mat4 transform = glm::mat4(1);
            transform = glm::translate(transform, fire_ball_transform.position);
            transform = glm::rotate(transform, fire_ball_transform.rotation.x, glm::vec3(1,0,0));
            transform = glm::rotate(transform, fire_ball_transform.rotation.y, glm::vec3(0,1,0));
            transform = glm::rotate(transform, fire_ball_transform.rotation.z, glm::vec3(0,0,1));
            transform = glm::translate(transform, (glm::vec3(0,0,1)*fire_ball_component.speed*delta));
            //transform = glm::scale(transform, fire_ball_transform.scale);

            glm::vec3 temp = glm::vec3(transform[3]);
            fire_ball_transform.position = temp;// + (glm::vec3(0,0,1)*fire_ball_component.speed*delta);

            // check for colision
			for(auto [slime_entity, slime_transform, slime_health, slimeMovement] : slimeView.each())
			{
				float distance = glm::length(slime_transform.position - fire_ball_transform.position);

                
                if (distance < 0.5f)
                {
                    // deal damage to slime
                    slime_health.health -= fire_ball_component.damage;
                    // remove fireball
                    registry.destroy(fire_ball_entity);
                    break;
                }
                
			}
		}
	}
};