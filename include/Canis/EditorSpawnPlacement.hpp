#pragma once
#include <Canis/Entity.hpp>

namespace Canis
{
    // Distance from the entity origin to the bottom of its primitive physics shape.
    // Match Jolt's collider priority, scaling, and minimum dimensions.
    inline float EditorSpawnGroundOffset(const Vector3 &scale, const Quaternion &rotation,
                                         const BoxCollider *box, const SphereCollider *sphere,
                                         const CapsuleCollider *capsule)
    {
        const Vector3 absoluteScale = glm::abs(scale);
        Vector3 offset(0.0f);
        float extent = 0.0f;
        if (box != nullptr && box->active)
        {
            offset = box->offset;
            const Vector3 half = glm::max(box->size * absoluteScale * 0.5f, Vector3(0.01f));
            const auto axes = glm::mat3_cast(rotation);
            extent = glm::abs(axes[0].y) * half.x + glm::abs(axes[1].y) * half.y +
                     glm::abs(axes[2].y) * half.z;
        }
        else if (sphere != nullptr && sphere->active)
        {
            offset = sphere->offset;
            extent = glm::max(0.01f, sphere->radius *
                glm::max(absoluteScale.x, glm::max(absoluteScale.y, absoluteScale.z)));
        }
        else if (capsule != nullptr && capsule->active)
        {
            offset = capsule->offset;
            const float radius = glm::max(0.01f, capsule->radius * glm::max(absoluteScale.x, absoluteScale.z));
            const float half = glm::max(0.01f, capsule->halfHeight * absoluteScale.y);
            extent = radius + half * glm::abs((rotation * Vector3(0, 1, 0)).y);
        }
        return extent - (rotation * (offset * scale)).y + 0.1f;
    }
}
