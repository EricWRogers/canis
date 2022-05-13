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

    glm::quat RotateTowards(glm::quat q1, glm::quat q2, float maxAngle){

        if( maxAngle < 0.001f ){
            // No rotation allowed. Prevent dividing by 0 later.
            return q1;
        }

        float cosTheta = glm::dot(q1, q2);

        // q1 and q2 are already equal.
        // Force q2 just to be sure
        if(cosTheta > 0.9999f){
            return q2;
        }

        // Avoid taking the long path around the sphere
        if (cosTheta < 0){
            q1 = q1*-1.0f;
            cosTheta *= -1.0f;
        }

        float angle = acos(cosTheta);

        // If there is only a 2&deg; difference, and we are allowed 5&deg;,
        // then we arrived.
        if (angle < maxAngle){
            return q2;
        }

        float fT = maxAngle / angle;
        angle = maxAngle;

        glm::quat res = (sin((1.0f - fT) * angle) * q1 + sin(fT * angle) * q2) / sin(angle);
        res = glm::normalize(res);
        return res;

    }

    glm::quat FromToRotation(glm::vec3 startDirection, glm::vec3 endDirection)
    {

        glm::vec3 crossProduct = glm::cross(startDirection, endDirection);

        float sineOfAngle = glm::length(crossProduct);

        float angle = asin(sineOfAngle);    
        glm::vec3 axis = crossProduct / sineOfAngle;

        glm::vec3 imaginary = sin(angle/2.0f) * axis;

        glm::quat result;
        result.w = cos(angle/2.0f);
        result.x = imaginary.x;
        result.y = imaginary.y;
        result.z = imaginary.z;

        return result;
    }

    glm::quat vec3tovec3(glm::vec3 f, glm::vec3 t)
    {
        glm::vec3 from = glm::normalize(f);
        glm::vec3 to   = glm::normalize(t);

        float cos_angle = dot(from, to);
        glm::vec3 half;
        
        if(cos_angle > -1.0005f && cos_angle < -0.9995f) {
            glm::vec3 n = glm::vec3(0, from.z, -from.y);
            if (glm::length(n) < 0.01) {
                n = glm::vec3(from.y, -from.x, 0);
            }
            n = normalize(n);
            return glm::quat(n.x,n.y,n.z, 3.14f);
        }
        else
            half = normalize(from + to);

        // http://physicsforgames.blogspot.sk/2010/03/quaternion-tricks.html
        return glm::quat(
            from.y * half.z - from.z * half.y,
            from.z * half.x - from.x * half.z,
            from.x * half.y - from.y * half.x,
            dot(from, half));
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
                    /*glm::mat4 rtransform = glm::lookAt(
                        transform.position + glm::vec3(0, 1, 0),
                        glm::normalize(slimePosition - (transform.position + glm::vec3(0, 1, 0))),
                        glm::vec3(0, 1, 0)
                    );*/

                    auto [slime_transform, slime_color, slime_component] = registry.get<TransformComponent, ColorComponent, SlimeMovementComponent>(slime);

                    slime_color.color = glm::vec4(1.0f);

                    glm::vec3 pos = transform.position + glm::vec3(0, 1, 0);//rtransform[3];
                    glm::vec3 rot;

                    //rot = glm::eulerAngles(vec3tovec3(pos,slime_transform.position));

                    //glm::mat4 bulletHoleRotationMatrix = glm::transpose(glm::lookAt(slime_transform.position, pos, glm::vec3(0.0f, 1.0f, 0.0f)));

                    //rot = glm::eulerAngles(glm::quat_cast(bulletHoleRotationMatrix));

                    

                    /*glm::quat q = FromToRotation(
                        glm::normalize(pos),
                        glm::normalize(slimePosition - pos)
                    );

                    rot = glm::eulerAngles(q);*/

                    /*glm::quat q = RotateTowards(
                        glm::normalize(pos),
                        glm::normalize(slimePosition - pos),
                        3.14f
                    );

                    rot = glm::eulerAngles(q);*/

                    /*glm::vec3 dir = glm::normalize(slimePosition - pos);
                    glm::quat lookRotation = glm::quatLookAt(-dir, glm::vec3(0.0f,1.0f,0.0f));
                    rot = glm::eulerAngles(lookRotation);*/

                    // Find the rotation between the front of the object (that we assume towards +Z,
                    // but this depends on your model) and the desired direction
                    glm::quat rot1 = RotationBetweenVectors(glm::vec3(0.0f, 0.0f, 1.0f), glm::normalize(slimePosition - pos));

                    // Recompute desiredUp so that it's perpendicular to the direction
                    // You can skip that part if you really want to force desiredUp
                    glm::vec3 right = glm::cross(glm::normalize(slimePosition - pos), glm::vec3(0.0f,1.0f,0.0f));
                    glm::vec3 desiredUp = glm::cross(right, glm::normalize(slimePosition - pos));

                    // Because of the 1rst rotation, the up is probably completely screwed up.
                    // Find the rotation between the "up" of the rotated object, and the desired up
                    glm::vec3 newUp = rot1 * glm::vec3(0.0f, 1.0f, 0.0f);
                    glm::quat rot2 = RotationBetweenVectors(newUp, desiredUp);

                    glm::quat targetOrientation = rot2 * rot1; // remember, in reverse order.

                    rot = glm::eulerAngles(rot1);

                    /*glm::mat4 fireTransform = glm::mat4(1.0f);
                    fireTransform = glm::translate(fireTransform, pos);
                    fireTransform = glm::rotate(fireTransform, 0.0f, glm::vec3(1,0,0));
                    fireTransform = glm::rotate(fireTransform, 0.0f, glm::vec3(0,1,0));
                    fireTransform = glm::rotate(fireTransform, 0.0f, glm::vec3(0,0,1));
                    fireTransform = glm::scale(fireTransform, glm::vec3(0.2f, 0.2f, 0.8f));

                    glm::quat fireQuat = glm::quat_cast(fireTransform);

                    glm::mat4 slimeTransform = glm::mat4(1.0f);
                    slimeTransform = glm::translate(slimeTransform, slime_transform.position);
                    slimeTransform = glm::rotate(slimeTransform, 0.0f, glm::vec3(1,0,0));
                    slimeTransform = glm::rotate(slimeTransform, 0.0f, glm::vec3(0,1,0));
                    slimeTransform = glm::rotate(slimeTransform, 0.0f, glm::vec3(0,0,1));
                    slimeTransform = glm::scale(slimeTransform, glm::vec3(1.0f));

                    glm::quat slimeQuat = glm::quat_cast(slimeTransform);

                    float cosTheta = dot(fireQuat, slimeQuat);

                    // Avoid taking the long path around the sphere
                    if (cosTheta < 0){
                        fireQuat = fireQuat*-1.0f;
                        cosTheta *= -1.0f;
                    }

                    float angle = acos(cosTheta);

                    float fT = 1.0f;

                    glm::quat res = (angle * fireQuat + sin(angle) * slimeQuat) / sin(angle);
	                res = glm::normalize(res);

                    rot = glm::eulerAngles(res);

                    rot *= 0.0174533;*/

                    /*glm::vec3 zaxis = glm::normalize(slimePosition - pos);
                    glm::vec3 xaxis = glm::normalize(glm::cross(glm::vec3(0,1,0), zaxis));
                    glm::vec3 yaxis = glm::cross(zaxis, xaxis);

                    glm::mat4 rtransform = glm::mat4(
                        xaxis.x, xaxis.y, xaxis.z, 0.0f,
                        yaxis.x, yaxis.y, yaxis.z, 0.0f,
                        zaxis.x, zaxis.y, zaxis.z, 0.0f,
                           0.0f,    0.0f,    0.0f, 1.0f
                    );*/

                    /*glm::vec3 forward = slimePosition - pos;
                    forward = glm::normalize(forward);
                    glm::vec3 up = glm::vec3(0, 1.0, 0);
                    glm::vec3 tangent0 = glm::cross(forward, up);
                    if (glm::length(tangent0) < 0.001f)
                    {
                        up = glm::vec3(0.0f, 1.0f, 0.0f);
                        tangent0 = glm::cross(forward, up);
                    }
                    tangent0 = glm::normalize(tangent0);
                    up = glm::cross(forward, tangent0);

                    glm::mat4 rtransform = glm::mat4(
                        forward.x, forward.y, forward.z, 0.0f,
                        up.x, up.y, up.z, 0.0f,
                        tangent0.x, tangent0.y, tangent0.z, 0.0f,
                           0.0f,    0.0f,    0.0f, 1.0f
                    );*/

                    /*glm::mat4 rtransform = glm::mat4(1.0f);

                    rtransform = glm::lookAt(
                        glm::vec3(0,0,0), 
                        -glm::normalize(slimePosition - pos),
                        glm::vec3(0,1,0)
                    );

                    rtransform = glm::inverse(rtransform);*/

                    /*
                    glm::mat4 rtransform = glm::mat4(
                        xaxis.x, yaxis.x, zaxis.x, 0.0f,
                        xaxis.y, yaxis.y, zaxis.y, 0.0f,
                        xaxis.z, yaxis.z, zaxis.z, 0.0f,
                        -glm::dot(xaxis,pos), -glm::dot(yaxis,pos), -glm::dot(zaxis,pos), 1.0f
                    );
                    */
                    //rot = glm::eulerAngles(glm::quat_cast(rtransform));

                    /*glm::vec3 pos = transform.position + glm::vec3(0, 1, 0);

                    glm::vec3 rot;

                    glm::vec3 d = slimePosition - pos;

                    rot.x = atan2( d.y, d.z );
                    if (d.z >= 0) {
                        rot.y = -atan2( d.x * cos(rot.x), d.z );
                    }else{
                        rot.y = atan2( d.x * cos(rot.x), -d.z );
                    }
                    rot.z = atan2( cos(rot.x), sin(rot.x) * sin(rot.y) );

                    rot *= 0.0174533;*/
                    
                    //glm::vec3 d = glm::normalize(slimePosition - pos);
                    //glm::vec3 rot = glm::cross(glm::vec3(0,1,0), d) * ;

                    Canis::Log("pos: " + glm::to_string(pos) + " slime pos: " + glm::to_string(slime_transform.position) + " rot: " + glm::to_string(rot));


                    Canis::Log("fire");

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