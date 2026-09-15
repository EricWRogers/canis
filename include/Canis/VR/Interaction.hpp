#pragma once
#include <Canis/VR/VRMath.hpp>
#include <optional>
#include <array>
#include <cstdint>

namespace Canis::VR
{
    // Owns only IDs; callers validate scene entity lifetime before dereferencing.
    class GrabOwnership
    {
        std::array<std::optional<uint64_t>, 2> held{};
        std::array<bool, 2> armed{true, true};
    public:
        std::optional<uint64_t> Held(unsigned hand) const { return held.at(hand); }
        bool IsHeld(uint64_t id) const { return held[0] == id || held[1] == id; }
        void Release(unsigned hand) { held.at(hand).reset(); armed.at(hand) = false; }
        // Returns true only on acquiring a new object. Tracking loss releases and
        // requires neutral before the next grab. A held grip never steals objects.
        bool Update(unsigned hand, float squeeze, bool tracked, std::optional<uint64_t> candidate)
        {
            if (!tracked) { Release(hand); return false; }
            if (squeeze < 0.3f) { held.at(hand).reset(); armed.at(hand) = true; }
            if (squeeze > 0.7f && armed.at(hand))
            {
                armed.at(hand) = false;
                if (candidate && !IsHeld(*candidate)) { held.at(hand) = candidate; return true; }
            }
            return false;
        }
    };

    // Axis-aligned placement for the demo counter: require the whole item to fit
    // on the surface and be near its top, rather than snapping from any altitude.
    inline std::optional<Vector3> SurfacePlacement(const Vector3& item, const Vector3& halfItem,
        const Vector3& surface, const Vector3& halfSurface, float tolerance = 0.14f)
    {
        const Vector3 delta = item-surface;
        const float height = halfSurface.y + halfItem.y;
        if (!std::isfinite(delta.x) || !std::isfinite(delta.y) || !std::isfinite(delta.z) ||
            std::abs(delta.x)+halfItem.x > halfSurface.x || std::abs(delta.z)+halfItem.z > halfSurface.z ||
            std::abs(delta.y-height) > tolerance) return {};
        return Vector3(item.x,surface.y+height,item.z);
    }

    // Demo flat-floor teleport: production levels should supply navmesh/collider
    // validation and clearance checks. The demo supplies its own comfort fade.
    // Reject near-horizontal/backward rays.
    inline std::optional<Vector3> FloorTeleport(const Pose& aim, const Vector3& head,
        const Vector2& minXZ, const Vector2& maxXZ, float maxDistance = 3.0f)
    {
        if (!aim.valid) return {};
        auto direction = aim.orientation * Vector3(0, 0, -1);
        if (direction.y >= -0.1f || aim.position.y <= 0) return {};
        auto point = aim.position + direction * (-aim.position.y / direction.y);
        if (point.x < minXZ.x || point.x > maxXZ.x || point.z < minXZ.y || point.z > maxXZ.y) return {};
        auto delta = point - Vector3(head.x, 0, head.z);
        if (glm::length(delta) > maxDistance) return {};
        point.y = 0;
        return point;
    }
}
