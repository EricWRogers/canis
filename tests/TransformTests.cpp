#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <iostream>
#include <stdexcept>

using namespace Canis;
namespace
{
void Check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
bool Near(Vector3 a, Vector3 b) { return glm::length(a - b) < .0001f; }
bool Near(Quaternion a, Quaternion b)
{
    return std::abs(glm::dot(glm::normalize(a), glm::normalize(b))) > .99999f;
}
Entity Body(Scene &scene, const char *name)
{
    auto entity = scene.CreateEntity(name);
    entity.AddComponent<Transform>();
    return entity;
}
void GlobalSetters()
{
    Scene scene;
    auto root = Body(scene, "Root"), parent = Body(scene, "Parent"), child = Body(scene, "Child");
    auto &rt = root.GetComponent<Transform>();
    auto &pt = parent.GetComponent<Transform>();
    auto &ct = child.GetComponent<Transform>();
    const Vector3 target(7, -3, 11);
    const auto orientation = glm::angleAxis(.7f, Vector3(0, 1, 0));
    rt.SetGlobalPosition(target);
    rt.SetGlobalRotation(orientation * 2.f);
    Check(Near(rt.GetGlobalPosition(), target) && Near(rt.GetGlobalRotation(), orientation),
          "Root world setters changed their meaning or failed to normalize rotation");
    pt.SetParent(root.TryGet(), false);
    pt.position = Vector3(1, 2, -4);
    pt.rotation = glm::angleAxis(-.4f, Vector3(1, 0, 0));
    pt.scale = Vector3(2);
    ct.SetParent(parent.TryGet(), false);
    ct.SetGlobalPosition(target);
    ct.SetGlobalRotation(orientation);
    Check(Near(ct.GetGlobalPosition(), target) && Near(ct.GetGlobalRotation(), orientation),
          "Nested world setters treated the target as local space");
    pt.scale = Vector3(2, 3, 4);
    ct.SetGlobalPosition(target);
    Check(Near(ct.GetGlobalPosition(), target), "Nonuniform parent scale changed world placement");
    ct.SetParent(nullptr, true);
    ct.SetGlobalPosition(Vector3(-2, 5, 1));
    ct.SetGlobalRotation(orientation);
    Check(!ct.parent && Near(ct.position, Vector3(-2, 5, 1)) && Near(ct.rotation, orientation),
          "Unparented world setters retained a stale parent space");
}
void ParentingModes()
{
    Scene scene;
    auto a = Body(scene, "A"), b = Body(scene, "B"), child = Body(scene, "Child"),
         sibling = Body(scene, "Sibling");
    auto &at = a.GetComponent<Transform>();
    auto &bt = b.GetComponent<Transform>();
    auto &ct = child.GetComponent<Transform>();
    at.position = Vector3(4, 2, -3);
    at.rotation = glm::angleAxis(.6f, Vector3(0, 1, 0));
    at.scale = Vector3(2);
    bt.position = Vector3(-3, 1, 8);
    bt.rotation = glm::angleAxis(-.3f, Vector3(1, 0, 0));
    bt.scale = Vector3(.5f);
    ct.position = Vector3(.2f, -.3f, .5f);
    ct.rotation = glm::angleAxis(.2f, Vector3(0, 0, 1));
    ct.scale = Vector3(.7f, 1.1f, .9f);
    const auto localPosition = ct.position, localScale = ct.scale;
    const auto localRotation = ct.rotation;
    ct.SetParent(a.TryGet(), false);
    Check(Near(ct.position, localPosition) && Near(ct.rotation, localRotation) && Near(ct.scale, localScale),
          "Keep-local parenting disturbed authored attachment offsets");
    const auto worldPosition = ct.GetGlobalPosition(), worldScale = ct.GetGlobalScale();
    const auto worldRotation = ct.GetGlobalRotation();
    ct.SetParent(b.TryGet(), true);
    Check(Near(ct.GetGlobalPosition(), worldPosition) && Near(ct.GetGlobalRotation(), worldRotation) &&
              Near(ct.GetGlobalScale(), worldScale) && at.children.empty() && bt.children.size() == 1,
          "World-preserving reparent changed placement or hierarchy links");
    sibling.GetComponent<Transform>().SetParent(b.TryGet(), false);
    ct.SetParent(b.TryGet(), true);
    Check(bt.children.size() == 2 && bt.children[0] == child,
          "Reattaching to the same parent reordered or duplicated the child");
    const auto detachPosition = ct.position, detachScale = ct.scale;
    const auto detachRotation = ct.rotation;
    ct.SetParent(nullptr, false);
    Check(!ct.parent && Near(ct.position, detachPosition) && Near(ct.scale, detachScale) &&
              Near(ct.rotation, detachRotation) && bt.children.size() == 1,
          "Keep-local detach changed offsets or retained a hierarchy link");
    ct.SetParent(a.TryGet(), false);
    const auto before = ct.position;
    bool rejected = false;
    try
    {
        at.SetParent(child.TryGet(), false);
    }
    catch (const std::runtime_error &)
    {
        rejected = true;
    }
    Check(rejected && !at.parent && ct.parent == a && Near(ct.position, before),
          "Parenting mode bypassed hierarchy cycle protection");
}
} // namespace
int main()
{
    try
    {
        GlobalSetters();
        ParentingModes();
        std::cout << "Transform world setters, nested scale, parenting modes and hierarchy safety passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
