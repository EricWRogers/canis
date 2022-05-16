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

#include "../../Scripts/TileMap.hpp"

class IceTowerSystem
{
public:
    void UpdateComponents(float delta, entt::registry &registry)
    {
        auto view = registry.view<TransformComponent, IceTowerComponent>();
        auto slimeView = registry.view<TransformComponent, SlimeMovementComponent>();

        // SLOW DOWN
        for (auto [tower_entity, transform, tower] : view.each())
        {
            for (auto [slime_entity, slime_transform, slime_component] : slimeView.each())
            {
                float distance = glm::length(slime_transform.position - transform.position);

                if (distance < tower.range)
                {
                    if (!(slime_component.status & SlimeStatus::CHILL))
			        {
                        slime_component.status |= SlimeStatus::CHILL;
                        slime_component.chillCountDown = 1.0f;
                    }
                }
            }
        }


        // FREEZE
        for (auto [tower_entity, transform, tower] : view.each())
        {
            tower.currentTime -= delta;

            if (tower.currentTime <= 0.0f)
            {
                // reset time
                tower.currentTime = tower.timeToSpawn;

                // find target
                bool slimeFound = false;
                entt::entity slime;
                float closetDistance;
                glm::vec3 slimePosition;

                int slimesFroze = 0;

                auto singleSlimeView = registry.view<TransformComponent, ColorComponent, SlimeMovementComponent>();

                for (auto [slime_entity, slime_transform, slime_color, slime_component] : singleSlimeView.each())
                {

                    switch(slime_component.slimeType)
                    {
                        case SlimeType::GREEN:
                            slime_color.color = GreenSlimeInfo.color;
                            break;
                        case SlimeType::BLUE:
                            slime_color.color = BlueSlimeInfo.color;
                            break;
                        case SlimeType::PURPLE:
                            slime_color.color = PurpleSlimeInfo.color;
                            break;
                        case SlimeType::ORANGE:
                            slime_color.color = OrangeSlimeInfo.color;
                            break;
                        default:
                            slime_color.color = GreenSlimeInfo.color;
                            break;
                    };

                    float distance = glm::length(slime_transform.position - transform.position);

                    if (distance < tower.range && slime_component.speed != 0.0f)
                    {
                        slimesFroze++;

                        registry.emplace<SlimeFreezeComponent>(slime_entity,
                            tower.damageOnBreak,// damageOnBreak
                            tower.freezeTime,// timeToBreak
                            tower.freezeTime// currentTime
                        );

                        if (slimesFroze >= tower.maxSlimesToFreeze)
                            return;
                    }
                }
            }
        }
    }
};