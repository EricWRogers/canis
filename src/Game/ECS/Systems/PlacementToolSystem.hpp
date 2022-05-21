#pragma once

#include <glm/glm.hpp>

#include "../../../Canis/GameHelper/AStar.hpp"
#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../../Scripts/TileMap.hpp"
#include "../../Scripts/Wallet.hpp"

#include "../Components/PlacementToolComponent.hpp"

class PlacementToolSystem
{
public:
    Canis::InputManager *inputManager;
    Canis::AStar *aStar;
	Wallet *wallet;

    float RoundUpFloat(float number)
    {
        float whole = floorf(number);
        float dec = whole - number;

        if (dec == 0.0f)
            return whole;
        else
            return whole + 1;
    }

    bool UpdateCanPlace(TransformComponent &transform)
    {
        if (titleMap[0][(int)transform.position.z][(int)transform.position.x] == BlockTypes::DIRT)
            return false;
        if (titleMap[0][(int)transform.position.z][(int)transform.position.x] == BlockTypes::GRASS)
            if (titleMap[1][(int)transform.position.z][(int)transform.position.x] == BlockTypes::NONE)
                return true;

        return false;
    }

    void PlaceTower(entt::registry &registry, TransformComponent &transform, BlockTypes blockType)
    {
        int x = roundf(transform.position.x);
        int y = RoundUpFloat(transform.position.y);
        int z = roundf(transform.position.z);

		titleMap[y][z][x] = blockType;

        const auto entity = registry.create();
				switch (blockType) // this looks bad
				{
				case GRASS:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(0.15f, 0.52f, 0.30f, 1.0f) // #26854c
					);
					break;
				case DIRT:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(0.91f, 0.82f, 0.51f, 1.0f) // #e8d282
					);
					break;
				case PORTAL:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(0.21f, 0.77f, 0.96f, 1.0f) // #36c5f4
					);
					registry.emplace<PortalComponent>(entity,
						2.0f,
						0.1f
					);
					break;
				case CASTLE:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(0.69f, 0.65f, 0.72f, 1.0f) // #b0a7b8
					);
					registry.emplace<CastleComponent>(entity,
						20,
						20
					);
					break;
				case SPIKETOWER:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(0.5f, 0.5f, 0.5f) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(0.15f, 0.52f, 0.30f, 1.0f) // #26854c
					);
					registry.emplace<SpikeTowerComponent>(entity,
						false, // setup
						0, // numOfSpikes
						3.0f, // timeToSpawn
						0.1f // currentTime
					);
					break;
				case GEMMINETOWER:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(0.5f, 0.5f, 0.5f) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(0.972f, 0.827f, 0.207f, 1.0f) // #f8d335
					);
					registry.emplace<GemMineTowerComponent>(entity,
						20, // gems
						5.0f, // timeToSpawn
						5.0f // currentTime
					);
					break;
				case FIRETOWER:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(0.5f, 0.5f, 0.5f) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(0.909f, 0.007f, 0.007f, 1.0f) // #e80202
					);
					registry.emplace<FireTowerComponent>(entity,
						1, // damage
						20.0f,// speed
						5.0f, // range
						2.0f, // timeToSpawn
						2.0f // currentTime
					);
					break;
				case ICETOWER:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(0.5f, 0.5f, 0.5f) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(0.117f, 0.980f, 0.972f, 1.0f) // #e80202
					);
					registry.emplace<IceTowerComponent>(entity,
						1, // maxSlimesToFreeze
						2, // damageOnBreak
						0.5f,// freezeTime
						2.0f, // range
						4.0f, // timeToSpawn
						4.0f // currentTime
					);
					break;
				default:
					break;
				}
    }

    void UpdateComponents(float delta, entt::registry &registry)
	{
        auto view = registry.view<TransformComponent, ColorComponent, PlacementToolComponent>();

        for(auto [entity, transform, color, tool] : view.each())
		{
			if (inputManager->justPressedKey(SDLK_c))
			{
				registry.destroy(entity);
			}

            bool canPlace = UpdateCanPlace(transform);

            if (canPlace)
            {
                color.color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
            }
            else
            {
                color.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            }

            glm::vec3 moveDirection = glm::vec3( 0.0f, 0.0f, 0.0f);

            if (inputManager->isKeyPressed(SDLK_w))
                moveDirection += glm::vec3( 1.0f, 0.0f, 0.0f);
            if (inputManager->isKeyPressed(SDLK_a))
                moveDirection += glm::vec3( 0.0f, 0.0f, -1.0f);
            if (inputManager->isKeyPressed(SDLK_s))
                moveDirection += glm::vec3( -1.0f, 0.0f, 0.0f);
            if (inputManager->isKeyPressed(SDLK_d))
                moveDirection += glm::vec3( 0.0f, 0.0f, 1.0f);
            
            transform.position += moveDirection;

            if (inputManager->justPressedKey(SDLK_RETURN) && canPlace)
            {
                PlaceTower(registry, transform, tool.blockType);

				switch (tool.blockType)
				{
					case SPIKETOWER:
						wallet->Pay(100);
						break;
					case GEMMINETOWER:
						wallet->Pay(200);
						break;
					case FIRETOWER:
						wallet->Pay(150);
						break;
					case ICETOWER:
						wallet->Pay(175);
						break;
					default:
						break;
				}
                registry.destroy(entity);
            }
        }
    }
};