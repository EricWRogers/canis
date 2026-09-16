#pragma once
#include <Canis/VR/VRMath.hpp>
#include <array>

namespace Canis::VR
{
    // Same ordering as XrHandJointEXT, without requiring OpenXR in game code.
    enum class HandJoint : unsigned
    {
        Palm, Wrist,
        ThumbMetacarpal, ThumbProximal, ThumbDistal, ThumbTip,
        IndexMetacarpal, IndexProximal, IndexIntermediate, IndexDistal, IndexTip,
        MiddleMetacarpal, MiddleProximal, MiddleIntermediate, MiddleDistal, MiddleTip,
        RingMetacarpal, RingProximal, RingIntermediate, RingDistal, RingTip,
        LittleMetacarpal, LittleProximal, LittleIntermediate, LittleDistal, LittleTip,
        Count
    };
    struct TrackedHandJoint
    {
        Pose pose; // Tracking space; same floor offset as head/controller poses.
        float radius = 0;
        bool tracked = false; // Valid joints may be inferred by the runtime.
    };
    struct HandSkeleton
    {
        std::array<TrackedHandJoint, static_cast<unsigned>(HandJoint::Count)> joints{};
        bool active = false;
        const TrackedHandJoint& operator[](HandJoint joint) const { return joints[static_cast<unsigned>(joint)]; }
        TrackedHandJoint& operator[](HandJoint joint) { return joints[static_cast<unsigned>(joint)]; }
    };
}
