#pragma once
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/CubeMeshComponent.hpp"

#include "../Components/SpikeComponent.hpp"
#include "../Components/HealthComponent.hpp"
#include "../Components/SpikeTowerComponent.hpp"

#include "../../Scripts/TileMap.hpp"

class SpikeTowerSystem
{
public:
	entt::registry *refRegistry;

    std::unordered_map<std::string, entt::entity> spikes;
    std::unordered_map<std::string, std::vector<entt::entity>> towerSpikes;

    glm::vec3 UP = glm::vec3(0,0,1);
    glm::vec3 DOWN = glm::vec3(0,0,-1);
    glm::vec3 LEFT = glm::vec3(-1,0,0);
    glm::vec3 RIGHT = glm::vec3(1,0,0);

	void UpdateComponents(float delta, entt::registry &registry)
	{
        auto towerView = registry.view<TransformComponent, SpikeTowerComponent>();

        // check if all towers are setup
        for(auto [tower_entity, tower_transform, tower_component] : towerView.each())
        {
            if(tower_component.setup == false)
            {
                tower_component.setup = true;

                towerSpikes[glm::to_string(tower_transform.position)] = std::vector<entt::entity>();

                auto spikeView = registry.view<TransformComponent, HealthComponent, SpikeComponent>();

                // check for road tiles
                std::vector<glm::vec3> positionToCheck;
                positionToCheck.push_back(tower_transform.position + UP);
                positionToCheck.push_back(tower_transform.position + DOWN);
                positionToCheck.push_back(tower_transform.position + LEFT);
                positionToCheck.push_back(tower_transform.position + RIGHT);
                positionToCheck.push_back(tower_transform.position + LEFT + UP);
                positionToCheck.push_back(tower_transform.position + RIGHT + UP);
                positionToCheck.push_back(tower_transform.position + LEFT + DOWN);
                positionToCheck.push_back(tower_transform.position + RIGHT + DOWN);

                for(int i = 0; i < positionToCheck.size(); i++)
                {
                    if(titleMap[0][(int)positionToCheck[i].z][(int)positionToCheck[i].x] == BlockTypes::DIRT)
                    {
                        std::unordered_map<std::string, entt::entity>::iterator it = spikes.find(glm::to_string(positionToCheck[i]));
                        if(it != spikes.end()) // found
                        {
                            // add spike to tower
                            tower_component.numOfSpikes++;
                            towerSpikes[glm::to_string(tower_transform.position)].push_back(it->second);
                        }
                        else // not found
                        {
                            // make new spike
                            const entt::entity entity = refRegistry->create();

                            // update title map
                            titleMap[1][(int)positionToCheck[i].z][(int)positionToCheck[i].x] = BlockTypes::SPIKE;

                            refRegistry->emplace<TransformComponent>(entity,
                                false, // active
                                glm::vec3((int)positionToCheck[i].x, (int)positionToCheck[i].y, (int)positionToCheck[i].z), // position
                                glm::vec3(0.0f, 0.0f, 0.0f), // rotation
                                glm::vec3(0.2f, 0.2f, 0.2f) // scale
                            );
                            refRegistry->emplace<ColorComponent>(entity,
                                glm::vec4(0.337f, 0.258f, 0.082f, 1.0f) // #564215
                            );
                            refRegistry->emplace<CubeMeshComponent>(entity,
                                0u
                            );
                            refRegistry->emplace<HealthComponent>(entity,
                                0,
                                6
                            );
                            refRegistry->emplace<SpikeComponent>(entity,
                                20,
                                20
                            );

                            // add spike to tower
                            tower_component.numOfSpikes++;
                            towerSpikes[glm::to_string(tower_transform.position)].push_back(entity);

                            // add spike to map
                            spikes[glm::to_string(positionToCheck[i])] = entity;
                        }
                    }
                }
            }
        }

		for(auto [tower_entity, tower_transform, tower_component] : towerView.each())
        {
            tower_component.currentTime -= delta;

			tower_component.currentTime -= delta;

			if (tower_component.currentTime <= 0.0f)
			{
				// reset time
				tower_component.currentTime = tower_component.timeToSpawn;

                // pick a random spike add 6 to the health
                int spikeIndex = rand()%towerSpikes[glm::to_string(tower_transform.position)].size();

                auto [spike_transform, spike_health] = registry.get<TransformComponent, HealthComponent>(towerSpikes[glm::to_string(tower_transform.position)][spikeIndex]);

                spike_transform.active = true;

                spike_health.health += 2;
                
            }
        }
	}
};