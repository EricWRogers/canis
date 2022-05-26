#pragma once

#include <glm/glm.hpp>

#include "../../../Canis/GameHelper/AStar.hpp"
#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"
#include "../../../Canis/ECS/Components/MeshComponent.hpp"

#include "../Components/BlockComponent.hpp"

#include "../../Scripts/TileMap.hpp"
#include "../../Scripts/Wallet.hpp"

#include "../Components/PlacementToolComponent.hpp"

enum ControllerMode {
	KEYBOARD,
	MOUSE,
	GAMEPAD
};

class PlacementToolSystem
{
public:
	Canis::Camera *camera;
	Canis::Window *window;
    Canis::InputManager *inputManager;
    Canis::AStar *aStar;
	Wallet *wallet;
	ControllerMode controllerMode = ControllerMode::MOUSE;

	unsigned int fireTowerVAO;
	unsigned int spikeTowerVAO;
	unsigned int goldTowerVAO;
	unsigned int iceTowerVAO;
	unsigned int whiteCubeVAO;

	int whiteCubeSize,
        fireTowerSize,
        spikeTowerSize,
        goldTowerSize,
        iceTowerSize;

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
					registry.emplace<MeshComponent>(entity,
						whiteCubeVAO,
						whiteCubeSize
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
					registry.emplace<MeshComponent>(entity,
						whiteCubeVAO,
						whiteCubeSize
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
					registry.emplace<MeshComponent>(entity,
						whiteCubeVAO,
						whiteCubeSize
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
					registry.emplace<MeshComponent>(entity,
						whiteCubeVAO,
						whiteCubeSize
					);
					registry.emplace<CastleComponent>(entity,
						20,
						20
					);
					break;
				case SPIKETOWER:
					registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y-0.5f, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1.0f, 1.0f, 1.0f) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
					);
					registry.emplace<MeshComponent>(entity,
						spikeTowerVAO,
						spikeTowerSize
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
						glm::vec3(x, y-0.5f, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1.0f, 1.0f, 1.0f) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
					);
					registry.emplace<MeshComponent>(entity,
						goldTowerVAO,
						goldTowerSize
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
						glm::vec3(x, y-0.5f, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1.0f, 1.0f, 1.0f) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
					);
					registry.emplace<MeshComponent>(entity,
						fireTowerVAO,
						fireTowerSize
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
						glm::vec3(x, y-0.5f, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1.0f, 1.0f, 1.0f) // scale
					);
					registry.emplace<ColorComponent>(entity,
						glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)
					);
					registry.emplace<MeshComponent>(entity,
						iceTowerVAO,
						iceTowerSize
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
            bool canPlace = UpdateCanPlace(transform);
			bool tryPlace = false;

            if (canPlace)
            {
                color.color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
            }
            else
            {
                color.color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
            }

			switch(controllerMode)
			{
				case ControllerMode::MOUSE:
				{
					tryPlace = inputManager->leftClick;

					auto block_view = registry.view<TransformComponent, BlockComponent>();

					Canis::Ray ray = Canis::RayFromMouse((*camera), (*window), (*inputManager));

					for(auto [block_entity, block_transform, block] : block_view.each())
					{
						if (Canis::HitSphere(block_transform.position, 0.5f, ray))//camera.Front))
						{
							transform.position = glm::vec3(
								block_transform.position.x,
								transform.position.y,
								block_transform.position.z
							);
						}
					}
					break;
				}
				case ControllerMode::KEYBOARD:
				{
					tryPlace = inputManager->justPressedKey(SDLK_RETURN);

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
					break;
				}
				case ControllerMode::GAMEPAD:
				{
					break;
				}
			}

            if (tryPlace && canPlace)
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
				