#pragma once
#include <Canis/VR/PlayerSettings.hpp>
#include <Canis/VR/VRMath.hpp>
#include <optional>
#include <algorithm>

namespace Canis::VR
{
    struct RigPose { Vector3 position{0.0f}; float yaw = 0.0f; };
    inline RigPose TurnAroundHead(const RigPose& rig, const Pose& trackingHead, float radians)
    {
        const auto head = rig.position + glm::angleAxis(rig.yaw, Vector3(0,1,0)) * trackingHead.position;
        const float yaw = rig.yaw + radians;
        return {head - glm::angleAxis(yaw, Vector3(0,1,0)) * trackingHead.position, yaw};
    }
    inline RigPose TeleportHeadTo(const RigPose& rig, const Pose& trackingHead, const Vector3& floorPoint)
    {
        const auto head = rig.position + glm::angleAxis(rig.yaw, Vector3(0,1,0)) * trackingHead.position;
        return {rig.position + Vector3(floorPoint.x-head.x,0,floorPoint.z-head.z), rig.yaw};
    }
    class TeleportGesture
    {
        bool armed = true, aiming = false;
    public:
        bool Aiming() const { return aiming; }
        bool Update(float trigger, bool enabled)
        {
            if (!enabled || !std::isfinite(trigger)) { armed = aiming = false; return false; }
            if (trigger < 0.3f)
            {
                const bool released = aiming;
                aiming = false; armed = true;
                return released;
            }
            if (armed && trigger > 0.75f) { aiming = true; armed = false; }
            return false;
        }
    };
    class ComfortMotion
    {
        enum class Phase { Idle, FadeOut, Black, FadeIn } phase = Phase::Idle;
        float elapsed = 0, alpha = 0;
        RigPose destination;
    public:
        bool Busy() const { return phase != Phase::Idle; }
        float Alpha() const { return alpha; }
        bool Queue(const RigPose& pose)
        {
            if (Busy() || !std::isfinite(pose.yaw) || !std::isfinite(pose.position.x) ||
                !std::isfinite(pose.position.y) || !std::isfinite(pose.position.z)) return false;
            destination = pose; elapsed = 0; phase = Phase::FadeOut; return true;
        }
        void Cancel() { phase = Phase::Idle; elapsed = alpha = 0; }
        // Never consume multiple phases in one update. Even a long frame renders
        // black once before fading back in; the move is emitted exactly once.
        std::optional<RigPose> Update(float dt, bool focusedAndTracked, const PlayerSettings& settings)
        {
            if (!focusedAndTracked) { Cancel(); return {}; }
            if (!std::isfinite(dt) || dt < 0 || phase == Phase::Idle) return {};
            elapsed += dt;
            if (phase == Phase::FadeOut)
            {
                alpha = std::clamp(elapsed/settings.fadeOutSeconds,0.0f,1.0f);
                if (alpha == 1) { phase = Phase::Black; elapsed = 0; return destination; }
            }
            else if (phase == Phase::Black)
            {
                alpha = 1;
                if (elapsed >= settings.blackSeconds) { phase = Phase::FadeIn; elapsed = 0; }
            }
            else
            {
                alpha = 1-std::clamp(elapsed/settings.fadeInSeconds,0.0f,1.0f);
                if (alpha == 0) Cancel();
            }
            return {};
        }
    };
}
