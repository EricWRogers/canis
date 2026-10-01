#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Profiler.hpp>
#include <Canis/ECS/Systems/JoltPhysics3DSystem.hpp>
#include <Canis/ECS/Systems/NavMeshSystem.hpp>
#include <iostream>
#include <stdexcept>
using namespace Canis;
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main()
{
    Scene scene;
    scene.CreateSystem<JoltPhysics3DSystem>();
    scene.CreateSystem<NavMeshSystem>();
    auto* physics = scene.GetSystem<JoltPhysics3DSystem>();
    physics->Create();
    auto box = [&](const char* name, Vector3 position, Vector3 size, float angle = 0.f) {
        auto entity = scene.CreateEntity(name);
        auto& transform = *entity.AddComponent<Transform>();
        transform.position = position;
        transform.rotation = glm::angleAxis(glm::radians(angle), Vector3(0,0,1));
        entity.AddComponent<Rigidbody>()->motionType = RigidbodyMotionType::STATIC;
        entity.AddComponent<BoxCollider>()->size = size;
        return entity;
    };
    box("30 degree ramp", Vector3(0), Vector3(12,.2f,4), 30);
    auto nav = scene.CreateEntity("Navigation");
    nav.AddComponent<Transform>();
    auto& surface = *nav.AddComponent<NavMeshSurface>();
    surface.meshCollidersOnly = false;
    surface.size = Vector3(12,10,6);
    surface.cellSize = .5f;
    surface.agentRadius = .2f;
    surface.agentHeight = 1.8f;
    surface.maxFloorDelta = .1f;
    surface.maxSlopeAngle = 35;
    Check(scene.BuildNavMesh(nav), "Ramp must bake below configured slope limit");
    auto floor = [&](float x, float z = 0.f, float ceiling = 5.f) {
        RaycastHit hit;
        Check(scene.Raycast(Vector3(x,ceiling,z),Vector3(0,-1,0),hit,10), "Fixture floor ray failed");
        return hit.point;
    };
    const auto lower = floor(-3), higher = floor(3);
    auto path = scene.FindNavMeshPath(nav, lower, higher);
    Check(path.size() > 2 && path.back().y - path.front().y > 3, "Navigation must follow ramp height");
    Check(!scene.FindNavMeshPath(nav,higher,lower).empty(), "Ramp must also support descent");
    surface.maxSlopeAngle = 20; surface.MarkDirty();
    Check(!scene.BuildNavMesh(nav), "Ramp above slope limit must not bake");
    Check(scene.FindNavMeshPath(nav,lower,higher).empty(), "Steep ramp cannot produce a route");
    surface.maxSlopeAngle = 35; surface.MarkDirty();
    Check(scene.BuildNavMesh(nav), "Slope change must permit rebaking");

    box("Lower floor", Vector3(25,-.1f,0),Vector3(8,.2f,8));
    box("Balcony", Vector3(25,2.9f,0),Vector3(8,.2f,8));
    nav.GetComponent<Transform>().position = Vector3(25,1.5f,0);
    surface.size = Vector3(8,7,8);surface.MarkDirty();
    Check(scene.BuildNavMesh(nav), "Overlapping floor layers should bake");
    Check(!scene.FindNavMeshPath(nav,Vector3(23,0,0),Vector3(27,0,0)).empty(), "Keep floor beneath balcony");
    Check(!scene.FindNavMeshPath(nav,Vector3(23,3,0),Vector3(27,3,0)).empty(), "Keep balcony floor");
    Check(scene.FindNavMeshPath(nav,Vector3(23,0,0),Vector3(23,3,0)).empty(), "Do not connect vertically through ceiling");
    box("Low ceiling",Vector3(25,1,0),Vector3(8,.2f,8));
    Check(scene.BuildNavMesh(nav), "Top floors still have headroom");
    Check(scene.FindNavMeshPath(nav,Vector3(23,0,0),Vector3(27,0,0)).empty(), "Reject paths without agent headroom");
    box("Gap left",Vector3(44,-.1f,0),Vector3(4,.2f,4));
    box("Gap right",Vector3(50,-.1f,0),Vector3(4,.2f,4));
    nav.GetComponent<Transform>().position = Vector3(47,0,0);
    surface.size = Vector3(12,4,6);surface.MarkDirty();
    Check(scene.BuildNavMesh(nav), "Gap platforms should bake individually");
    Check(scene.FindNavMeshPath(nav,Vector3(44,0,0),Vector3(50,0,0)).empty(), "Do not invent walking links across gaps");
    box("Step lower",Vector3(64,-.1f,0),Vector3(4,.2f,4));
    box("Step upper",Vector3(68,.4f,0),Vector3(4,.2f,4));
    nav.GetComponent<Transform>().position = Vector3(66,0,0);surface.MarkDirty();
    Check(scene.BuildNavMesh(nav), "Step platforms should bake");
    Check(scene.FindNavMeshPath(nav,Vector3(64,0,0),Vector3(68,.5f,0)).empty(), "Reject discontinuous rises beyond max step height");
    surface.maxFloorDelta = .6f;surface.MarkDirty();
    Check(scene.BuildNavMesh(nav), "Increasing step height permits rebuilding");
    Check(!scene.FindNavMeshPath(nav,Vector3(64,0,0),Vector3(68,.5f,0)).empty(), "Configured step height permits a climbable step");
    std::cout << "NAV_SLOPE_PASS configurable slope, height following, descending, stacked floors, headroom, gaps and step limits\n";
}
