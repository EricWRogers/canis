#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/quaternion.hpp>

#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/SlimeFreezeComponent.hpp"
#include "../Components/IceTowerComponent.hpp"
#include "../Components/SlimeMovementComponent.hpp"
#include "../Components/HealthComponent.hpp"

#include "../../Scripts/TileMap.hpp"

class SlimeFreezeSystem
{
public:
    void UpdateComponents(float delta, entt::registry &registry)
    {
        auto view = registry.view<TransformComponent, SlimeMovementComponent, SlimeFreezeComponent, HealthComponent>();

        for (auto [entity, transform, movement, freeze, health] : view.each())
        {
            freeze.currentTime -= delta;

            movement.speed = 0.0f;

            if (freeze.currentTime <= 0.0f)
            {

                switch(movement.slimeType)
                {
                    case SlimeType::GREEN:
                        movement.speed = GreenSlimeInfo.speed;
                        break;
                    case SlimeType::BLUE:
                        movement.speed = BlueSlimeInfo.speed;
                        break;
                    case SlimeType::PURPLE:
                        movement.speed = PurpleSlimeInfo.speed;
                        break;
                    case SlimeType::ORANGE:
                        movement.speed = OrangeSlimeInfo.speed;
                        break;
                    default:
                        movement.speed = GreenSlimeInfo.speed;
                        break;
                };
                
                health.health = freeze.damageOnBreak;
                registry.remove<SlimeFreezeComponent>(entity);
            }
        }
    }
};