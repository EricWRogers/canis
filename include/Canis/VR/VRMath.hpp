#pragma once
#include <Canis/Math.hpp>
#include <cmath>
#include <stdexcept>

namespace Canis::VR
{
    struct Pose
    {
        Vector3 position{0.0f};
        Quaternion orientation{1.0f, 0.0f, 0.0f, 0.0f};
        bool valid = false;
    };

    inline Matrix4 PoseMatrix(const Pose& pose)
    {
        return glm::translate(Matrix4(1.0f), pose.position) * glm::mat4_cast(pose.orientation);
    }

    // OpenXR angle order; OpenGL clip depth is [-1, +1].
    inline Matrix4 Projection(float left, float right, float down, float up, float nearClip, float farClip)
    {
        if (!std::isfinite(left) || !std::isfinite(right) || !std::isfinite(down) || !std::isfinite(up) ||
            !std::isfinite(nearClip) || !std::isfinite(farClip) ||
            left <= -PI/2 || right >= PI/2 || down <= -PI/2 || up >= PI/2 ||
            !(nearClip > 0.0f && farClip > nearClip && left < right && down < up))
            throw std::invalid_argument("Invalid VR projection frustum");
        return glm::frustum(std::tan(left) * nearClip, std::tan(right) * nearClip,
            std::tan(down) * nearClip, std::tan(up) * nearClip, nearClip, farClip);
    }

    inline Pose TransformPose(const Matrix4& origin, const Pose& local)
    {
        Pose result = local;
        if (!local.valid) return result;
        result.position = Vector3(origin * Vector4(local.position, 1.0f));
        result.orientation = glm::normalize(glm::quat_cast(glm::mat3(origin)) * local.orientation);
        return result;
    }

    // Requires returning to neutral before another turn; never repeats while held.
    struct SnapTurn
    {
        bool armed = true;
        float Update(float axis, bool active, float radians = PI / 6.0f)
        {
            if (!active) { armed = false; return 0.0f; }
            if (std::abs(axis) < 0.3f) armed = true;
            if (armed && std::abs(axis) > 0.7f)
            {
                armed = false;
                return axis > 0.0f ? -radians : radians;
            }
            return 0.0f;
        }
    };
}
