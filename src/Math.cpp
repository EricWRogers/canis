#include <Canis/Math.hpp>

namespace Canis
{
    Ray RayFromMouse(Camera &camera, Window &window, InputManager &inputManager)
    {
        glm::vec4 lRayStart_NDC(
            ((float)inputManager.mouse.x/(float)window.GetScreenWidth()  - 0.5f) * 2.0f,
            -((float)inputManager.mouse.y/(float)window.GetScreenHeight()  - 0.5f) * 2.0f,
            -1.0, // The near plane maps to Z=-1 in Normalized Device Coordinates
            1.0f
        );
        glm::vec4 lRayEnd_NDC(
            ((float)inputManager.mouse.x/(float)window.GetScreenWidth()  - 0.5f) * 2.0f,
            -((float)inputManager.mouse.y/(float)window.GetScreenHeight()  - 0.5f) * 2.0f,
            0.0,
            1.0f
        );

        glm::mat4 projection = glm::perspective(camera.FOV, (float)window.GetScreenWidth() / (float)window.GetScreenHeight(), 0.1f, 100.0f);

        glm::mat4 M = glm::inverse(projection * camera.GetViewMatrix());
        glm::vec4 lRayStart_world = M * lRayStart_NDC; lRayStart_world/=lRayStart_world.w;
        glm::vec4 lRayEnd_world   = M * lRayEnd_NDC  ; lRayEnd_world  /=lRayEnd_world.w;

        glm::vec3 lRayDir_world(lRayEnd_world - lRayStart_world);
        lRayDir_world = glm::normalize(lRayDir_world);

        return Ray { lRayStart_world, lRayDir_world };
    }

    bool HitSphere(glm::vec3 center, float radius, Ray ray)
    {
        glm::vec3 oc = ray.origin - center;
        float a = glm::dot(ray.direction, ray.direction);
        float b = 2.0f * glm::dot(oc, ray.direction);
        float c = glm::dot(oc,oc) - radius*radius;
        float discriminant = b*b - 4*a*c;
        return (discriminant>0);
    }
}