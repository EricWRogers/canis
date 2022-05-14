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

#include "../Components/FireBallComponent.hpp"
#include "../Components/FireTowerComponent.hpp"
#include "../Components/SlimeMovementComponent.hpp"

class FireTowerSystem
{
public:
    glm::quat RotationBetweenVectors(glm::vec3 start, glm::vec3 dest){
        start = glm::normalize(start);
        dest = glm::normalize(dest);

        float cosTheta = glm::dot(start, dest);
        glm::vec3 rotationAxis;

        if (cosTheta < -1 + 0.001f){
            // special case when vectors in opposite directions:
            // there is no "ideal" rotation axis
            // So guess one; any will do as long as it's perpendicular to start
            rotationAxis = glm::cross(glm::vec3(0.0f, 0.0f, 1.0f), start);
            float normalLength2 = glm::length2(glm::normalize(rotationAxis));
            if (normalLength2 < 0.01 ) // bad luck, they were parallel, try again!
                rotationAxis = glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), start);

            rotationAxis = glm::normalize(rotationAxis);
            return glm::angleAxis(glm::radians(180.0f), rotationAxis);
        }

        rotationAxis = glm::cross(start, dest);

        float s = sqrt( (1+cosTheta)*2 );
        float invs = 1 / s;

        return glm::quat(
            s * 0.5f, 
            rotationAxis.x * invs,
            rotationAxis.y * invs,
            rotationAxis.z * invs
        );

    }

    
    void UpdateComponents(float delta, entt::registry &registry)
    {
        auto view = registry.view<TransformComponent, FireTowerComponent>();

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

                auto slimeView = registry.view<TransformComponent, ColorComponent, SlimeMovementComponent>();

                for (auto [slime_entity, slime_transform, slime_color, slime_component] : slimeView.each())
                {

                    slime_color.color = glm::vec4(0.35f, 0.71f, 0.32f, 0.8f); // #5ab552
                    float distance = glm::length(slime_transform.position - transform.position);

                    if (distance < tower.range && registry.valid(slime_entity))
                    {
                        if (!slimeFound)
                        {
                            slimeFound = true;
                            slime = slime_entity;
                            closetDistance = distance;
                            slimePosition = slime_transform.position;
                        }
                        else
                        {
                            if (distance < closetDistance)
                            {
                                slime = slime_entity;
                                closetDistance = distance;
                                slimePosition = slime_transform.position;
                            }
                        }
                    }
                }

                if (slimeFound)
                {
                    auto [slime_transform, slime_color, slime_component] = registry.get<TransformComponent, ColorComponent, SlimeMovementComponent>(slime);

                    slime_color.color = glm::vec4(1.0f);

                    glm::vec3 pos = transform.position + glm::vec3(0, 1, 0);//rtransform[3];
                    glm::vec3 rot;

                    // Find the rotation between the front of the object (that we assume towards +Z,
                    // but this depends on your model) and the desired direction
                    glm::quat rot1 = RotationBetweenVectors(glm::vec3(0.0f, 0.0f, 1.0f), glm::normalize(slimePosition - pos));

                    // Recompute desiredUp so that it's perpendicular to the direction
                    // You can skip that part if you really want to force desiredUp
                    // glm::vec3 right = glm::cross(glm::normalize(slimePosition - pos), glm::vec3(0.0f,1.0f,0.0f));
                    // glm::vec3 desiredUp = glm::cross(right, glm::normalize(slimePosition - pos));

                    // Because of the 1rst rotation, the up is probably completely screwed up.
                    // Find the rotation between the "up" of the rotated object, and the desired up
                    // glm::vec3 newUp = rot1 * glm::vec3(0.0f, 1.0f, 0.0f);
                    // glm::quat rot2 = RotationBetweenVectors(newUp, desiredUp);

                    // glm::quat targetOrientation = rot2 * rot1; // remember, in reverse order.

                    rot = glm::eulerAngles(rot1);

                    //Canis::Log("pos: " + glm::to_string(pos) + " slime pos: " + glm::to_string(slime_transform.position) + " rot: " + glm::to_string(rot));


                    //Canis::Log("fire");

                    // spawn fire ball
                    const entt::entity entity = registry.create();

                    registry.emplace<TransformComponent>(entity,
                        true,                                    // active
                        pos, // position
                        rot,             // rotation
                        glm::vec3(0.2f, 0.2f, 0.8f)              // scale
                    );
                    registry.emplace<ColorComponent>(entity,
                        glm::vec4(0.909f, 0.007f, 0.007f, 1.0f) // #e80202
                    );
                    registry.emplace<FireBallComponent>(entity,
                        tower.damage,
                        tower.fireBallSpeed
                    );
                }
            }
        }
    }
};