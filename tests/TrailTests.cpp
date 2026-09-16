#include <Canis/VFX/Trails.hpp>
#include <Canis/App.hpp>
#include <Canis/Editor.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <iostream>
using namespace Canis;
void Check(bool condition, const char* message)
{ if (!condition) throw std::runtime_error(message); }
int main()
{
    TrailRenderer trail;
    trail.lifetime = 1;
    trail.Advance(Vector3(0), 0, true);
    trail.Advance(Vector3(0), .1f, true);
    Check(trail.points.size() == 1, "Stationary emitter generated duplicate points");
    trail.Advance(Vector3(0,0,-1), .1f, true);
    trail.Advance(Vector3(.2f,0,-2), .1f, true);
    for (int mode : {0,1})
    {
        trail.mode = mode;
        auto mesh = BuildTrailMesh(trail, Vector3(0,0,3));
        Check(mesh.size() == static_cast<size_t>(mode ? 96 : 12), "Wrong trail topology");
        Check(glm::length(mesh.front().position - trail.points.front().position) < 1e-6f, "Tail does not taper to a point");
        Check(mesh.front().color.a < mesh.back().color.a, "Older samples must fade");
        for (auto camera : {Vector3(0,0,-1), Vector3(.032f,0,3), Vector3(-.032f,0,3)})
            for (const auto& vertex : BuildTrailMesh(trail,camera))
                Check(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) && std::isfinite(vertex.position.z), "Degenerate camera produced invalid mesh");
    }
    trail.emitting = false;
    trail.Advance(Vector3(1), .1f, true);
    Check(trail.points.size() == 3, "Stopped emitter appended a point");
    trail.Advance(Vector3(1), 1, true);
    Check(trail.points.empty(), "Stopped trail did not expire");
    trail.emitting = true;
    trail.Advance(Vector3(0), 0, true);
    trail.Advance(Vector3(100), .01f, true);
    Check(trail.points.size() == 1, "Teleport connected unrelated positions");
    trail.maxPoints = 3;
    for (int i = 0; i < 20; ++i) trail.Advance(Vector3(100 + i*.1f), .01f, true);
    Check(trail.points.size() == 3, "Point limit ignored");
    trail.Advance(Vector3(100), 0, false);
    Check(trail.points.empty(), "Inactive trail retained stale history");
    trail.Advance(Vector3(std::numeric_limits<float>::quiet_NaN()), 0, true);
    Check(trail.points.empty(), "Nonfinite position accepted");
    trail.Clear();
    Check(BuildTrailMesh(trail,Vector3(0)).empty(), "Empty trail produced geometry");
    App app;
    Editor editor;
    app.RegisterDefaults(editor);
    app.scene.app = &app;
    auto nodes = YAML::Load(R"(
- Entity: 101
  Name: Trail
  Canis::TrailRenderer:
    mode: 1
    width: 0.08
    lifetime: 0.4
    emitting: false
    tubeSides: 12
)");
    auto entities = app.scene.LoadEntityNodes(nodes);
    auto& loaded = entities[0]->GetComponent<TrailRenderer>();
    Check(entities[0]->HasComponent<Transform>(), "Trail did not add required Transform");
    Check(loaded.mode == 1 && loaded.tubeSides == 12 && !loaded.emitting, "Trail settings failed to load");
    loaded.points.push_back({Vector3(1), .1f});
    auto encoded = app.scene.EncodeEntity(*entities[0])["Canis::TrailRenderer"];
    Check(encoded["mode"].as<int>() == 1 && std::abs(encoded["width"].as<float>()-.08f) < 1e-6f,
        "Trail inspector settings failed to serialize");
    Check(!encoded["points"], "Runtime history leaked into scene serialization");
    std::cout << "Trail geometry, lifetime, reuse, and degenerate input checks passed\n";
}
