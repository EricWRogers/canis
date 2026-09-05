#pragma once
#include <Canis/Math.hpp>

namespace Canis
{
    // Vectors are expressed in the same local frame as the mesh-edit gizmo.
    inline Vector3 ConstrainEditorVector(Vector3 value, int axis, bool plane)
    {
        if (axis < 0 || axis > 2)
            return value;
        for (int i = 0; i < 3; ++i)
            if (plane ? i == axis : i != axis)
                value[i] = 0.0f;
        return value;
    }

    inline Vector3 EditorScaleFactors(float amount, int axis, bool plane)
    {
        return Vector3(1.0f) + ConstrainEditorVector(Vector3(amount - 1.0f), axis, plane);
    }
}
