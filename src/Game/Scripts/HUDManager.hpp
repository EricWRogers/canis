#pragma once

#include <glm/glm.hpp>

#include "../../Canis/External/entt.hpp"

#include "../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../Canis/ECS/Components/RectTransformComponent.hpp"
#include "../../Canis/ECS/Components/ColorComponent.hpp"

#include "TileMap.hpp"
#include "Wallet.hpp"

#include "../ECS/Components/PlacementToolComponent.hpp"

class HUDManager
{
private:
    bool showingHUD = false;

public:
    Canis::InputManager *inputManager;
    Canis::Window *window;
    Wallet *wallet;

    entt::entity healthText;
    entt::entity walletText;
    entt::entity scoreText;

    entt::entity rootTowerText;
    entt::entity fireTowerText;
    entt::entity iceTowerText;
    entt::entity goldTowerText;

    void SpawnPlacementTool(entt::registry &registry, BlockTypes blockType)
    {
        const auto entityPlacementTool = registry.create();
        registry.emplace<TransformComponent>(entityPlacementTool,
            true, // active
            glm::vec3(11.0f, 0.5f, 16.0f), // position
            glm::vec3(0.0f, 0.0f, 0.0f), // rotation
            glm::vec3(1.0f, 1.0f, 1.0f) // scale
        );
        registry.emplace<ColorComponent>(entityPlacementTool,
            glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)    // color #c82fb5e9
        );
        registry.emplace<PlacementToolComponent>(entityPlacementTool,
            blockType // blockType
        );
    }

    void Update(float delta, entt::registry &registry)
    {
        if (showingHUD)
        {
            BlockTypes blockToSpawn = BlockTypes::NONE;

            // grey out what you can not buy
            auto [root_tower_transform, root_tower_color] = registry.get<RectTransformComponent, ColorComponent>(rootTowerText);
            if (wallet->ICanAfford(100))
                root_tower_color.color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            else
                root_tower_color.color = glm::vec4(0.384f, 0.356f, 0.356f, 1.0f);
            
            auto [fire_tower_transform, fire_tower_color] = registry.get<RectTransformComponent, ColorComponent>(fireTowerText);
            if (wallet->ICanAfford(150))
                fire_tower_color.color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            else
                fire_tower_color.color = glm::vec4(0.384f, 0.356f, 0.356f, 1.0f);
            
            auto [ice_tower_transform, ice_tower_color] = registry.get<RectTransformComponent, ColorComponent>(iceTowerText);
            if (wallet->ICanAfford(175))
                ice_tower_color.color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            else
                ice_tower_color.color = glm::vec4(0.384f, 0.356f, 0.356f, 1.0f);
            
            auto [gold_tower_transform, gold_tower_color] = registry.get<RectTransformComponent, ColorComponent>(goldTowerText);
            if (wallet->ICanAfford(200))
                gold_tower_color.color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            else
                gold_tower_color.color = glm::vec4(0.384f, 0.356f, 0.356f, 1.0f);

            // 
            if (inputManager->isKeyPressed(SDLK_1) && wallet->ICanAfford(100)) // root tower
            {
                blockToSpawn = BlockTypes::SPIKETOWER;
            }
            if (inputManager->isKeyPressed(SDLK_2) && wallet->ICanAfford(150)) // fire tower
            {
                blockToSpawn = BlockTypes::FIRETOWER;
            }
            if (inputManager->isKeyPressed(SDLK_3) && wallet->ICanAfford(175)) // ice tower
            {
                blockToSpawn = BlockTypes::ICETOWER;
            }
            if (inputManager->isKeyPressed(SDLK_4) && wallet->ICanAfford(200)) // gold tower
            {
                blockToSpawn = BlockTypes::GEMMINETOWER;
            }
            
            if (blockToSpawn != BlockTypes::NONE)
            {
                showingHUD = false;

                root_tower_transform.active = false;
                fire_tower_transform.active = false;
                ice_tower_transform.active = false;
                gold_tower_transform.active = false;

                SpawnPlacementTool(registry, blockToSpawn);
                return;
            }
        }
        else
        {
            if (inputManager->justPressedKey(SDLK_RETURN) || inputManager->justPressedKey(SDLK_c) || inputManager->leftClick)
            {
                showingHUD = true;

                auto [root_tower_transform, root_tower_color] = registry.get<RectTransformComponent, ColorComponent>(rootTowerText);
                auto [fire_tower_transform, fire_tower_color] = registry.get<RectTransformComponent, ColorComponent>(fireTowerText);
                auto [ice_tower_transform, ice_tower_color] = registry.get<RectTransformComponent, ColorComponent>(iceTowerText);
                auto [gold_tower_transform, gold_tower_color] = registry.get<RectTransformComponent, ColorComponent>(goldTowerText);

                root_tower_transform.active = true;
                fire_tower_transform.active = true;
                ice_tower_transform.active = true;
                gold_tower_transform.active = true;
            }
        }
    }

    void Load(entt::registry &registry)
    {
        showingHUD = true;

        healthText = registry.create();
        registry.emplace<RectTransformComponent>(healthText,
            true, // active
            glm::vec2(25.0f, window->GetScreenHeight() - 65.0f), // position
            glm::vec2(0.0f, 0.0f), // rotation
            1.0f // scale
        );
        registry.emplace<ColorComponent>(healthText,
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
        );
        registry.emplace<TextComponent>(healthText,
            new std::string("Health : 0") // text
        );

        scoreText = registry.create();
        registry.emplace<RectTransformComponent>(scoreText,
            true, // active
            glm::vec2(25.0f, window->GetScreenHeight() - 130.0f), // position
            glm::vec2(0.0f, 0.0f), // rotation
            1.0f // scale
        );
        registry.emplace<ColorComponent>(scoreText,
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
        );
        registry.emplace<TextComponent>(scoreText,
            new std::string("Score : 0") // text
        );

        walletText = registry.create();
        registry.emplace<RectTransformComponent>(walletText,
            true, // active
            glm::vec2(25.0f, window->GetScreenHeight() - 195.0f), // position
            glm::vec2(0.0f, 0.0f), // rotation
            1.0f // scale
        );
        registry.emplace<ColorComponent>(walletText,
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
        );
        registry.emplace<TextComponent>(walletText,
            new std::string("Gold : 0") // text
        );

        rootTowerText = registry.create();
        registry.emplace<RectTransformComponent>(rootTowerText,
            true, // active
            glm::vec2(25.0f, 120.0f), // position
            glm::vec2(0.0f, 0.0f), // rotation
            0.5f // scale
        );
        registry.emplace<ColorComponent>(rootTowerText,
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
        );
        registry.emplace<TextComponent>(rootTowerText,
            new std::string("[1] $100 Root Tower") // text
        );

        fireTowerText = registry.create();
        registry.emplace<RectTransformComponent>(fireTowerText,
            true, // active
            glm::vec2(25.0f, 90.0f), // position
            glm::vec2(0.0f, 0.0f), // rotation
            0.5f // scale
        );
        registry.emplace<ColorComponent>(fireTowerText,
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
        );
        registry.emplace<TextComponent>(fireTowerText,
            new std::string("[2] $150 Fire Tower") // text
        );

        iceTowerText = registry.create();
        registry.emplace<RectTransformComponent>(iceTowerText,
            true, // active
            glm::vec2(25.0f, 60.0f), // position
            glm::vec2(0.0f, 0.0f), // rotation
            0.5f // scale
        );
        registry.emplace<ColorComponent>(iceTowerText,
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
        );
        registry.emplace<TextComponent>(iceTowerText,
            new std::string("[3] $175 Ice Tower") // text
        );

        goldTowerText = registry.create();
        registry.emplace<RectTransformComponent>(goldTowerText,
            true, // active
            glm::vec2(25.0f, 30.0f), // position
            glm::vec2(0.0f, 0.0f), // rotation
            0.5f // scale
        );
        registry.emplace<ColorComponent>(goldTowerText,
            glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
        );
        registry.emplace<TextComponent>(goldTowerText,
            new std::string("[4] $200 Gold Mine") // text
        );
    }
};