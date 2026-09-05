#include <Canis/EditorTransformConstraints.hpp>
#include <Canis/EditorSpawnPlacement.hpp>
#include <iostream>

int main()
{
    using namespace Canis;
    const Quaternion upright(1, 0, 0, 0);
    CapsuleCollider capsule;
    capsule.halfHeight = 0.5f;
    capsule.radius = 0.5f;
    const auto near = [](float a, float b) { return glm::abs(a - b) < 0.0001f; };
    if (!near(EditorSpawnGroundOffset(Vector3(1), upright, nullptr, nullptr, &capsule), 1.1f))
        return 1;
    capsule.offset.y = 1.0f; // Feet-origin player.
    if (!near(EditorSpawnGroundOffset(Vector3(1), upright, nullptr, nullptr, &capsule), 0.1f))
        return 1;
    capsule.offset.y = 0.0f;
    if (!near(EditorSpawnGroundOffset(Vector3(2, 3, 2), upright, nullptr, nullptr, &capsule), 2.6f) ||
        !near(EditorSpawnGroundOffset(Vector3(1), glm::angleAxis(glm::radians(90.0f), Vector3(0, 0, 1)),
                                      nullptr, nullptr, &capsule), 0.6f))
        return 1;
    BoxCollider box;
    box.size = Vector3(2, 4, 6);
    SphereCollider sphere;
    sphere.radius = 0.5f;
    if (!near(EditorSpawnGroundOffset(Vector3(1), upright, &box, &sphere, &capsule), 2.1f))
        return 1;
    box.active = false;
    if (!near(EditorSpawnGroundOffset(Vector3(2, 3, 4), upright, &box, &sphere, &capsule), 2.1f))
        return 1;
    capsule.offset.y = 0.25f;
    if (!near(EditorSpawnGroundOffset(Vector3(1, -2, 1), upright, nullptr, nullptr, &capsule), 2.1f) ||
        !near(EditorSpawnGroundOffset(Vector3(1), upright, nullptr, nullptr, nullptr), 0.1f))
        return 1;
    const Vector3 drag(2, 3, 4);
    for (int axis = 0; axis < 3; ++axis)
    {
        const Vector3 line = ConstrainEditorVector(drag, axis, false);
        const Vector3 plane = ConstrainEditorVector(drag, axis, true);
        if (line[axis] != drag[axis] || plane[axis] != 0 || line + plane != drag)
            return 1;
        const Vector3 scale = EditorScaleFactors(2, axis, true);
        for (int i = 0; i < 3; ++i)
            if (scale[i] != (i == axis ? 1.0f : 2.0f))
                return 1;
    }
    if (ConstrainEditorVector(drag, -1, false) != drag ||
        EditorScaleFactors(2, -1, false) != Vector3(2) ||
        ConstrainEditorVector(Vector3(0, 3, 0), 1, false) != Vector3(0, 3, 0) ||
        ConstrainEditorVector(drag, 2, true) != Vector3(2, 3, 0))
        return 1;
    std::cout << "Spawn clearance, axis, plane, vertical drag, and scale constraints passed.\n";
}
