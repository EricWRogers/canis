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
    GreenSlime gs;
    void UpdateComponents(float delta, entt::registry &registry)
    {
        auto view = registry.view<TransformComponent, SlimeMovementComponent, SlimeFreezeComponent, HealthComponent>();

        for (auto [entity, transform, movement, freeze, health] : view.each())
        {
            freeze.currentTime -= delta;

            movement.speed = 0.0f;

            if (freeze.currentTime <= 0.0f)
            {
                movement.speed = gs.speed;
                health.health = freeze.damageOnBreak;
                registry.remove<SlimeFreezeComponent>(entity);
            }
        }
    }
};