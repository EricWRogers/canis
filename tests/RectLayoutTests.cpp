#include <Canis/App.hpp>
#include <Canis/Components.hpp>
#include <Canis/Editor.hpp>
#include <Canis/RectLayoutSerialization.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Yaml.hpp>
#include <iostream>
#include <stdexcept>

using namespace Canis;
template<class T> concept OwnsPosition = requires(T t) { t.position; };
template<class T> concept OwnsHierarchy = requires(T t) { t.parent; t.children; };
static_assert(!OwnsPosition<RectLayout> && !OwnsHierarchy<RectLayout>);
static void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
static bool Near(Vector3 a, Vector3 b) { return glm::length(a - b) < .0001f; }

static void SharedSpatialAndHierarchy()
{
    Scene scene;
    auto canvas = scene.CreateEntity("World canvas");
    auto& plane = *canvas.AddComponent<Transform>(); plane.position = {8, 3, -2};
    plane.rotation = Quaternion(Vector3(.2f, .4f, .1f)); plane.scale = Vector3(.001f);
    canvas.AddComponent<Canvas>()->renderMode = CanvasRenderMode::WORLD_SPACE;
    canvas.AddComponent<RectLayout>()->size = {640, 480};
    auto group = scene.CreateEntity("Ordinary Transform group");
    auto& groupTransform = *group.AddComponent<Transform>(); groupTransform.SetParent(canvas.TryGet(), false);
    groupTransform.position = {40, 20, 0};
    auto child = scene.CreateEntity("UI element"); auto& rect = *child.AddComponent<RectLayout>();
    Check(child.HasComponent<Transform>(), "RectLayout did not require shared Transform");
    rect.Spatial().SetParent(group.TryGet(), false); rect.SetLocalPosition({20, 10}); rect.SetLocalDepth(2);
    Check(rect.GetParent() == group && groupTransform.children.size() == 1, "UI did not join the common hierarchy");
    const auto expected = Vector3(plane.GetModelMatrix() * Vector4(60, 30, 2, 1));
    Check(Near(rect.Spatial().GetGlobalPosition(), expected), "UI and Transform disagree about world position");
    Check(glm::length(rect.GetPosition() - Vector2(60, 30)) < .001f, "Transform-only UI grouping was ignored");
    rect.SetPosition({80, 25});
    Check(glm::length(rect.GetPosition() - Vector2(80, 25)) < .001f, "Small world-space canvas could not move anchored UI");
    auto other = scene.CreateEntity("Other group"); other.AddComponent<Transform>()->position = {1, 0, 0};
    const auto before = rect.Spatial().GetGlobalPosition();
    rect.Spatial().SetParent(other.TryGet());
    Check(Near(before, rect.Spatial().GetGlobalPosition()) && groupTransform.children.empty(), "Reparenting UI lost world placement or old link");
    bool cycle = false;
    try { other.GetComponent<Transform>().SetParent(child.TryGet()); } catch (const std::runtime_error&) { cycle = true; }
    Check(cycle, "Mixed UI/3D cycle was accepted");
    child.RemoveComponent<Transform>();
    Check(!child.HasComponent<RectLayout>() && other.GetComponent<Transform>().children.empty(), "Removing Transform left an invalid layout or hierarchy link");
    auto doomed = scene.CreateEntity("Doomed UI"); doomed.AddComponent<RectLayout>()->Spatial().SetParent(other.TryGet(), false);
    other.Destroy(); Check(!doomed, "Common Transform hierarchy did not destroy UI descendants");
}

static void SerializationAndMigration()
{
    App app; Editor editor; app.scene.app = &app; app.RegisterDefaults(editor);
    auto node = YAML::Load(R"(
Entity: 9001
Name: Legacy canvas
Active: true
Canis::Canvas: {renderMode: 2, screenSize: [640, 480]}
Canis::Transform:
  position: [4, 5, 6]
  rotation: [0, 0, 0.5]
  scale: [0.001, 0.001, 0.001]
  parent: 0
  children: [9002]
Canis::RectTransform:
  active: true
  position: [10, 20]
  rotation: 0.25
  scale: [2, 3]
  size: [640, 480]
  depth: 2
  parent: 0
  children: [9002]
)");
    Matrix4 expected = glm::translate(Matrix4(1), Vector3(4, 5, 6)) * glm::mat4_cast(Quaternion(Vector3(0, 0, .5f))) *
        glm::scale(Matrix4(1), Vector3(.001f)) * glm::translate(Matrix4(1), Vector3(10, 20, 2)) *
        glm::mat4_cast(Quaternion(Vector3(0, 0, .25f))) * glm::scale(Matrix4(1), Vector3(2, 3, 1));
    Check(MigrateRectLayoutEntity(node) && !MigrateRectLayoutEntity(node), "Migration is not idempotent");
    Check(!node["Canis::RectTransform"] && node["Canis::Transform"]["children"].size() == 1, "Migration kept duplicate hierarchy or old type");
    auto child = YAML::Load(R"(
Entity: 9002
Name: Legacy label
Canis::RectTransform: {position: [12, 8], size: [80, 40], parent: 9001, children: []}
)");
    YAML::Node nodes(YAML::NodeType::Sequence); nodes.push_back(node); nodes.push_back(child);
    auto loaded = app.scene.LoadEntityNodes(nodes, true);
    Check(loaded.size() == 2 && loaded[1]->GetComponent<Transform>().parent == loaded[0], "Legacy load did not remap into common hierarchy");
    const Matrix4 actual = loaded[0]->GetComponent<Transform>().GetModelMatrix();
    for (int i = 0; i < 4; ++i) Check(glm::length(actual[i] - expected[i]) < .00001f, "World canvas migration changed its physical pose");
    auto saved = app.scene.EncodeEntity(*loaded[1]);
    Check(saved["Canis::RectLayout"] && saved["Canis::Transform"] && !saved["Canis::RectTransform"], "Save did not emit new layout schema");
    for (const char* field : {"position", "rotation", "scale", "depth", "parent", "children"})
        Check(!saved["Canis::RectLayout"][field], "Spatial field remained serialized in RectLayout");
}
int main()
{
    try { SharedSpatialAndHierarchy(); SerializationAndMigration(); std::cout << "RectLayout composition and migration checks passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
