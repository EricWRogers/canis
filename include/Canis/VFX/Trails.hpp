#pragma once
#include <Canis/Components.hpp>
#include <Canis/System.hpp>
#include <vector>

namespace Canis
{
    struct TrailPoint { Vector3 position; float age = 0; };
    struct TrailVertex { Vector3 position; Color color; };

    struct TrailRenderer
    {
        static constexpr const char* ScriptName = "Canis::TrailRenderer";
        TrailRenderer() = default;
        explicit TrailRenderer(Entity& owner) : entity(&owner) {}
        Entity* entity = nullptr;
        void Create() {}
        void Destroy() { Clear(); }
        bool emitting = true;
        int mode = 0; // Ribbon, Tube
        float width = .025f;
        float lifetime = .25f;
        float minDistance = .02f;
        float breakDistance = 3.f;
        int maxPoints = 128;
        int tubeSides = 8;
        Color color = Color(1.f, .8f, .35f, .8f);
        std::vector<TrailPoint> points;
        void Clear() { points.clear(); }
        void Advance(Vector3 position, float deltaTime, bool active);
    };

    std::vector<TrailVertex> BuildTrailMesh(const TrailRenderer& trail, Vector3 cameraPosition);
    class TrailSystem : public System
    {
    public:
        TrailSystem() { m_name = "Canis::TrailSystem"; }
        bool UpdateAfterScripts() const override { return true; }
        void Update(entt::registry& registry, float deltaTime) override;
    };
    void RegisterTrailComponents(App& app);
}
