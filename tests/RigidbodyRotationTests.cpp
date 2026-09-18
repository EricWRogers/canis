#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <Canis/ECS/Systems/JoltPhysics3DSystem.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace Canis;
static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static void StepContact(float dt)
{
    Scene scene;
    JoltPhysics3DSystem physics;
    physics.scene = &scene;
    physics.Create();
    auto box = [&](Vector3 position, Vector3 size) {
        auto e = scene.CreateEntity("Step");
        e.AddComponent<Transform>()->position = position;
        e.AddComponent<Rigidbody>()->motionType = RigidbodyMotionType::STATIC;
        e.AddComponent<BoxCollider>()->size = size;
    };
    box(Vector3(0,-.5f,0), Vector3(12,1,12));
    box(Vector3(0,.2f,0), Vector3(4,.4f,.5f));
    auto player = scene.CreateEntity("Capsule");
    auto& t = *player.AddComponent<Transform>();
    t.position = Vector3(0,.95f,1);
    auto& body = *player.AddComponent<Rigidbody>();
    body.mass = 75;
    body.friction = 0;
    body.allowSleeping = false;
    body.lockRotationX = body.lockRotationY = body.lockRotationZ = true;
    auto& capsule = *player.AddComponent<CapsuleCollider>();
    capsule.halfHeight = .55f;
    capsule.radius = .3f;
    for (int i = 0; i < int(20.f/dt); ++i) {
        body.SetLinearVelocity(Vector3(0,body.linearVelocity.y,-3.5f));
        physics.Update(scene.GetRegistry(), dt);
        Check(glm::length(t.rotation * Vector3(0,1,0) - Vector3(0,1,0)) < .0001f,
              "Locked capsule tipped during repeated step contact");
        Check(glm::length(body.angularVelocity) < .0001f, "Locked capsule gained angular velocity");
    }
    Check(t.position.z < .7f, "Rotation locks prevented translation");
}

static void RuntimeLocks()
{
    Scene scene;
    JoltPhysics3DSystem physics;
    physics.scene = &scene;
    physics.Create();
    auto e = scene.CreateEntity("Spinner");
    auto& t = *e.AddComponent<Transform>();
    auto& body = *e.AddComponent<Rigidbody>();
    e.AddComponent<BoxCollider>();
    body.useGravity = false;
    body.angularDamping = 0;
    body.allowSleeping = false;
    for (int axis = 0; axis < 3; ++axis) {
        body.lockRotationX = body.lockRotationY = body.lockRotationZ = false;
        Vector3 spin(0); spin[axis] = 2.f;
        auto before = t.rotation;
        body.SetAngularVelocity(spin);
        for (int i=0; i<30; ++i) physics.Update(scene.GetRegistry(), 1.f/60.f);
        Check(std::abs(glm::dot(before,t.rotation)) < .99f, "Unlocked rotation did not move");
        if (axis == 0) body.lockRotationX = true;
        if (axis == 1) body.lockRotationY = true;
        if (axis == 2) body.lockRotationZ = true;
        body.SetAngularVelocity(Vector3(0));
        for (int i=0; i<3; ++i) physics.Update(scene.GetRegistry(), 1.f/60.f);
        before = t.rotation;
        for (int i=0; i<120; ++i) {
            body.SetAngularVelocity(spin);
            physics.Update(scene.GetRegistry(), 1.f/60.f);
            Check(1.f-std::abs(glm::dot(before,t.rotation)) < .00001f,
                  "Runtime rotation lock failed to constrain its axis");
        }
    }
}

static void StaticEdits()
{
    Scene scene;
    JoltPhysics3DSystem physics;
    physics.scene=&scene;physics.Create();
    auto parent=scene.CreateEntity("Parent");
    parent.AddComponent<Transform>();
    auto child=scene.CreateEntity("Static child");
    auto& transform=*child.AddComponent<Transform>();
    transform.SetParent(&parent);
    child.AddComponent<Rigidbody>()->motionType=RigidbodyMotionType::STATIC;
    auto& collider=*child.AddComponent<BoxCollider>();
    collider.size=Vector3(1);
    auto step=[&] { physics.Update(scene.GetRegistry(),1.f/60.f); };
    auto hit=[&](float x) { return physics.Raycast(Vector3(x,3,0),Vector3(0,-1,0),6.f); };
    step();Check(hit(0),"Static collider missing");
    for(int i=0;i<10;++i)step();
    Check(hit(0),"Unchanged static collider lost");
    parent.GetComponent<Transform>().position.x=4;
    step();Check(!hit(0) && hit(4),"Parent move did not move static collider");
    collider.size.x=4;
    step();Check(hit(5.5f),"Static collider resize not applied");
    child.SetActive(false);
    step();Check(!hit(4),"Inactive static collider remained in physics");
    child.SetActive(true);
    step();Check(hit(4),"Reactivated static collider missing");
}

int main()
{
    try {
        for (float dt : {1.f/30.f,1.f/60.f,1.f/144.f}) StepContact(dt);
        RuntimeLocks();
        StaticEdits();
        std::cout << "Capsule step contact and runtime rotation locks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
