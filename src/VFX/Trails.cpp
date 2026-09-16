#include <Canis/VFX/Trails.hpp>
#include <Canis/App.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Editor.hpp>
#include <algorithm>
#include <cmath>

namespace Canis
{
    namespace
    {
        ComponentConf trailConf;
        SystemConf trailSystemConf;
        float Safe(float value, float fallback, float low, float high)
        { return std::isfinite(value) ? std::clamp(value, low, high) : fallback; }
        bool Finite(Vector3 v)
        { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
        Vector3 Perpendicular(Vector3 tangent)
        {
            return glm::normalize(glm::cross(tangent,
                std::abs(tangent.y) < .9f ? Vector3(0,1,0) : Vector3(1,0,0)));
        }
    }

    void TrailRenderer::Advance(Vector3 position, float deltaTime, bool active)
    {
        if (!active || !Finite(position)) { Clear(); return; }
        const float duration = Safe(lifetime, .25f, .01f, 30.f);
        const float dt = Safe(deltaTime, 0, 0, 60);
        for (auto& point : points) point.age += dt;
        points.erase(std::remove_if(points.begin(), points.end(),
            [duration](const TrailPoint& p) { return p.age >= duration; }), points.end());
        if (emitting)
        {
            const float distance = points.empty() ? 0 : glm::length(position - points.back().position);
            if (distance > Safe(breakDistance, 3, .01f, 10000)) Clear();
            if (points.empty() || distance >= Safe(minDistance, .02f, .0001f, 10))
                points.push_back({position, 0});
        }
        const auto limit = static_cast<size_t>(std::clamp(maxPoints, 2, 2048));
        if (points.size() > limit) points.erase(points.begin(), points.end() - limit);
    }

    std::vector<TrailVertex> BuildTrailMesh(const TrailRenderer& trail, Vector3 cameraPosition)
    {
        std::vector<TrailVertex> result;
        const auto& points = trail.points;
        if (points.size() < 2) return result;
        const bool tube = trail.mode == 1;
        const int sides = tube ? std::clamp(trail.tubeSides, 3, 24) : 2;
        const float radius = Safe(trail.width, .025f, .0001f, 10) * .5f;
        const float duration = Safe(trail.lifetime, .25f, .01f, 30);
        std::vector<float> distances(points.size(), 0);
        for (size_t i = 1; i < points.size(); ++i)
            distances[i] = distances[i-1] + glm::length(points[i].position - points[i-1].position);
        if (distances.back() < .00001f) return result;
        std::vector<TrailVertex> rings;
        rings.reserve(points.size() * sides);
        Vector3 normal(0);
        for (size_t i = 0; i < points.size(); ++i)
        {
            Vector3 tangent = points[std::min(i+1, points.size()-1)].position - points[i ? i-1 : 0].position;
            if (glm::length(tangent) < .00001f) tangent = Vector3(0,0,-1);
            tangent = glm::normalize(tangent);
            // Transport the tube frame along bends; ribbons face the current render eye.
            Vector3 side = tube ? normal - tangent * glm::dot(normal, tangent)
                                : glm::cross(tangent, cameraPosition - points[i].position);
            if (glm::length(side) < .00001f) side = Perpendicular(tangent);
            side = glm::normalize(side);
            if (i && glm::dot(side, normal) < 0) side = -side;
            normal = side;
            const Vector3 other = glm::cross(tangent, side);
            const float taper = distances[i] / distances.back();
            Color color = trail.color;
            color.a *= std::clamp(1.f - points[i].age / duration, 0.f, 1.f);
            for (int j = 0; j < sides; ++j)
            {
                const float angle = 6.28318530718f * j / sides;
                Vector3 offset = tube ? side * std::cos(angle) + other * std::sin(angle)
                                      : side * (j ? 1.f : -1.f);
                rings.push_back({points[i].position + offset * radius * taper, color});
            }
        }
        result.reserve((points.size()-1) * (tube ? sides : 1) * 6);
        for (size_t i = 1; i < points.size(); ++i)
            for (int j = 0; j < (tube ? sides : 1); ++j)
            {
                const int next = (j+1) % sides;
                const auto a = rings[(i-1)*sides+j], b = rings[(i-1)*sides+next];
                const auto c = rings[i*sides+j], d = rings[i*sides+next];
                result.insert(result.end(), {a,b,c,b,d,c});
            }
        return result;
    }

    void TrailSystem::Update(entt::registry& registry, float deltaTime)
    {
        for (auto [handle, trail, transform] : registry.view<TrailRenderer, Transform>().each())
            trail.Advance(transform.GetGlobalPosition(), deltaTime, transform.IsActiveInHierarchy());
    }

    void RegisterTrailComponents(App& app)
    {
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, emitting);
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, mode);
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, width);
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, lifetime);
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, minDistance);
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, breakDistance);
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, maxPoints);
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, tubeSides);
        REGISTER_PROPERTY(trailConf, Canis::TrailRenderer, color);
        DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(trailConf, Canis::TrailRenderer, Canis::Transform);
        const auto decode = trailConf.Decode;
        trailConf.Decode = [decode](YAML::Node& node, Entity& entity, bool callCreate)
        {
            if (!entity.HasComponent<Transform>()) entity.AddComponent<Transform>();
            decode(node, entity, callCreate);
        };
        trailConf.DrawInspector = [](Editor& editor, Entity& entity, const ScriptConf& conf)
        {
            auto& trail = entity.GetComponent<TrailRenderer>();
            ImGui::Combo("Shape", &trail.mode, "Ribbon\0Tube\0");
            DrawInspectorField(editor, "emitting", conf.name.c_str(), trail.emitting);
            DrawInspectorField(editor, "width", conf.name.c_str(), trail.width);
            DrawInspectorField(editor, "lifetime", conf.name.c_str(), trail.lifetime);
            DrawInspectorField(editor, "color", conf.name.c_str(), trail.color);
            DrawInspectorField(editor, "minDistance", conf.name.c_str(), trail.minDistance);
            DrawInspectorField(editor, "breakDistance", conf.name.c_str(), trail.breakDistance);
            DrawInspectorField(editor, "maxPoints", conf.name.c_str(), trail.maxPoints);
            if (trail.mode == 1) DrawInspectorField(editor, "tubeSides", conf.name.c_str(), trail.tubeSides);
        };
        app.RegisterComponent(trailConf);
        DEFAULT_SYSTEM_CONFIG(trailSystemConf, Canis::TrailSystem, Canis::SystemPipeline::Update);
        app.RegisterSystem(trailSystemConf);
    }
}
