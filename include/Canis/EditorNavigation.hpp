#pragma once

#include <Canis/Math.hpp>
#include <algorithm>
#include <cmath>

namespace Canis
{
    // Use distance to the surface rather than the pivot of a large model.
    inline float EditorCameraBoundsDistance(const Vector3 &_position, const Vector3 &_min, const Vector3 &_max)
    {
        return glm::length(_position - glm::clamp(_position, _min, _max));
    }

    inline float EditorCameraSelectionSpeedScale(float _distance)
    {
        if (!std::isfinite(_distance))
            return 1.0f;

        // Normal flight speed beyond eight metres, with fine control nearby.
        // Keep a nonzero minimum so the camera can leave or cross the selection.
        return std::clamp(_distance / 8.0f, 0.025f, 1.0f);
    }
}
