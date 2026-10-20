#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Profiler.hpp>
#include <Canis/ECS/Systems/JoltPhysics3DSystem.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <thread>

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

static void ContinuousYawLocks(float dt)
{
    Scene scene;
    JoltPhysics3DSystem physics;
    physics.scene = &scene;
    physics.Create();
    constexpr float pi = 3.14159265358979323846f;
    const Vector3 up(0, 1, 0), front(0, 0, -1);
    struct Spinner
    {
        Entity entity;
        Quaternion initial;
        Vector3 previousForward;
        float direction = 1, accumulated = 0;
        bool kinematic = false;
    };
    std::vector<Spinner> spinners;
    for (bool kinematic : {false, true})
        for (float direction : {-1.f, 1.f})
            for (float heading : {0.f, pi})
                for (bool tiltedParent : {false, true})
                {
                    auto entity = scene.CreateEntity("Yaw-only turning body");
                    auto& transform = *entity.AddComponent<Transform>();
                    transform.position = {float(spinners.size()) * 4, 0, 0};
                    transform.rotation = glm::angleAxis(heading, up);
                    if (tiltedParent)
                    {
                        transform.rotation *= Quaternion(Vector3(.21f, 0, -.13f));
                        auto parent = scene.CreateEntity("Rotated body parent");
                        parent.AddComponent<Transform>()->rotation = Quaternion(Vector3(.1f, .37f, -.08f));
                        transform.SetParent(&parent); // Preserve the authored world tilt.
                    }
                    auto& body = *entity.AddComponent<Rigidbody>();
                    body.motionType = kinematic ? RigidbodyMotionType::KINEMATIC : RigidbodyMotionType::DYNAMIC;
                    body.useGravity = false;
                    body.linearDamping = body.angularDamping = 0;
                    body.allowSleeping = false;
                    body.lockRotationX = body.lockRotationZ = true;
                    entity.AddComponent<BoxCollider>()->size = {.6f, 1, .8f};
                    const auto initial = transform.GetGlobalRotation();
                    auto forward = initial * front;
                    forward.y = 0;
                    spinners.push_back({entity, initial, glm::normalize(forward), direction, 0, kinematic});
                }
    physics.Update(scene.GetRegistry(), dt);
    for (int frame = 0; frame < int(std::ceil(4.f / dt)); ++frame)
    {
        for (auto& spinner : spinners)
        {
            auto& body = spinner.entity.GetComponent<Rigidbody>();
            if (spinner.kinematic)
                body.Rotate({0, spinner.direction * 2 * dt, 0});
            else
                body.SetAngularVelocity({1.25f, spinner.direction * 2, -.75f});
        }
        physics.Update(scene.GetRegistry(), dt);
        for (auto& spinner : spinners)
        {
            const auto rotation = spinner.entity.GetComponent<Transform>().GetGlobalRotation();
            auto forward = rotation * front;
            forward.y = 0;
            forward = glm::normalize(forward);
            const float turn = std::atan2(glm::dot(up, glm::cross(spinner.previousForward, forward)),
                                           glm::dot(spinner.previousForward, forward));
            Check(turn * spinner.direction >= -.0001f,
                  "Yaw-only body reversed at an Euler branch while turning");
            Check(std::abs(turn) < 2 * std::max(dt, 1.f / 60.f) + .01f,
                  "Yaw-only body jumped while crossing a rotation branch");
            spinner.accumulated += turn;
            spinner.previousForward = forward;
            for (const auto axis : {Vector3(1, 0, 0), up, Vector3(0, 0, 1)})
                Check(std::abs((rotation * axis).y - (spinner.initial * axis).y) < .001f,
                      "Yaw-only rotation changed the authored world tilt");
            const auto velocity = spinner.entity.GetComponent<Rigidbody>().angularVelocity;
            Check(std::abs(velocity.x) < .0001f && std::abs(velocity.z) < .0001f,
                  "Solver rotation locks admitted a locked angular velocity");
        }
    }
    for (const auto& spinner : spinners)
    {
        if (std::abs(spinner.accumulated - spinner.direction * 8) > .1f)
            std::cerr << "Yaw progress dt=" << dt << " kinematic=" << spinner.kinematic
                      << " direction=" << spinner.direction << " turn=" << spinner.accumulated << '\n';
        Check(std::abs(spinner.accumulated - spinner.direction * 8) < .1f,
              "Yaw-only body failed to turn continuously through 180 and 360 degrees");
    }
}

static void FullyLockedAuthoredRotation()
{
    Scene scene;
    JoltPhysics3DSystem physics;
    physics.scene = &scene;
    physics.Create();
    for (bool kinematic : {false, true})
    {
        auto entity = scene.CreateEntity("All rotation axes locked");
        auto& transform = *entity.AddComponent<Transform>();
        transform.position = {kinematic ? 5.f : 0.f, 0, 0};
        const auto authored = glm::normalize(Quaternion(Vector3(.6f, 2.4f, -.35f)));
        transform.rotation = authored;
        auto& body = *entity.AddComponent<Rigidbody>();
        body.motionType = kinematic ? RigidbodyMotionType::KINEMATIC : RigidbodyMotionType::DYNAMIC;
        body.useGravity = false;
        body.allowSleeping = false;
        body.linearDamping = body.angularDamping = 0;
        body.lockRotationX = body.lockRotationY = body.lockRotationZ = true;
        entity.AddComponent<BoxCollider>();
        const auto start = transform.position;
        for (int frame = 0; frame < 120; ++frame)
        {
            if (kinematic)
            {
                body.Rotate({.03f, .07f, -.02f});
                body.Move({.02f, 0, 0});
            }
            else
            {
                body.SetAngularVelocity({2, 4, -3});
                body.SetLinearVelocity({1.2f, 0, 0});
            }
            physics.Update(scene.GetRegistry(), 1.f / 60.f);
            Check(1 - std::abs(glm::dot(authored, transform.GetGlobalRotation())) < .00001f,
                  "All-axis locks changed an authored nonidentity orientation");
            Check(glm::length(body.angularVelocity) < .0001f,
                  "All-axis locks retained commanded angular velocity");
        }
        Check(transform.position.x > start.x + 2.3f,
              "All rotation locks accidentally constrained translation");
    }
}

static void SubframeMotionAfterTurn()
{
    Scene scene;
    JoltPhysics3DSystem physics;physics.scene=&scene;physics.Create();
    std::vector<Entity> movers;
    for(int i=0;i<720;i++)
    {
        auto entity=scene.CreateEntity("Turned moving body");
        auto& transform=*entity.AddComponent<Transform>();
        transform.position={i*4.f,0,0};
        transform.rotation=glm::normalize(Quaternion(Vector3((i%3)*.025f,i*.00872664626f,(i%7)*.01f)));
        auto& body=*entity.AddComponent<Rigidbody>();
        body.useGravity=false;body.linearDamping=body.angularDamping=0;body.allowSleeping=false;
        entity.AddComponent<BoxCollider>();body.SetLinearVelocity({0,0,-12});
        movers.push_back(entity);
    }
    std::vector<float> previous(movers.size()),minimum(movers.size(),1),maximum(movers.size());
    for(int frame=0;frame<240;frame++)
    {
        physics.Update(scene.GetRegistry(),1.f/240);
        for(size_t i=0;i<movers.size();i++)
        {
            const float distance=-movers[i].GetComponent<Transform>().position.z;
            const float step=distance-previous[i];previous[i]=distance;
            if(frame>10){minimum[i]=std::min(minimum[i],step);maximum[i]=std::max(maximum[i],step);}
        }
    }
    for(size_t i=0;i<movers.size();i++)
    {
        if(std::abs(previous[i]-11.8f)>.03f || minimum[i]<.045f || maximum[i]>.055f)
        {
            std::cout<<"Subframe body="<<i<<" distance="<<previous[i]<<" step="<<minimum[i]<<".."<<maximum[i]<<'\n';
            Check(false,"Turned rigidbody rewound or stuttered between fixed physics steps");
        }
    }
    std::cout<<"720 headings preserve constant motion at 240 Hz rendering / 60 Hz physics\n";
}

static void AdaptivePhysicsJobs()
{
    Scene scene;
    JoltPhysics3DSystem physics;
    physics.scene = &scene;
    physics.Create();
    auto makeBox = [&](int index, bool dynamic) {
        auto entity = scene.CreateEntity("Job selection test");
        entity.AddComponent<Transform>()->position = {index * 3.f, 0, 0};
        auto& body = *entity.AddComponent<Rigidbody>();
        body.motionType = dynamic ? RigidbodyMotionType::DYNAMIC : RigidbodyMotionType::STATIC;
        body.useGravity = false;
        body.allowSleeping = false;
        body.linearDamping = 0;
        entity.AddComponent<BoxCollider>();
        if (dynamic) body.SetLinearVelocity({0, 0, -6});
        return entity;
    };
    // A large static level must still use inline jobs for a small awake set.
    for (int i = 0; i < 160; ++i) makeBox(i, false);
    std::vector<Entity> movers;
    movers.push_back(makeBox(200, true));
    auto& recorder = Profiler::Get();
    const bool wasRecording = recorder.IsRecording();
    recorder.SetRecording(true);
    auto step = [&](bool expectParallel) {
        { Profiler::FrameScope frame; physics.Update(scene.GetRegistry(), 1.f/60.f); }
        int inlineSteps = 0, workerSteps = 0;
        for (const auto& sample : recorder.Frames().back().samples) {
            inlineSteps += sample.name == "Jolt inline jobs";
            workerSteps += sample.name == "Jolt worker jobs";
        }
        // An explicit override intentionally bypasses automatic selection.
        if (!std::getenv("CANIS_PHYSICS_WORKERS"))
            Check(expectParallel ? workerSteps == 1 && inlineSteps == 0 : inlineSteps == 1 && workerSteps == 0,
                "Physics jobs did not follow the awake body count");
    };
    step(false);
    const float before = movers[0].GetComponent<Transform>().position.z;
    for (int i = 1; i < 128; ++i) movers.push_back(makeBox(200 + i, true));
    for (int i = 0; i < 30; ++i) step(std::thread::hardware_concurrency() > 1);
    Check(std::abs(movers[0].GetComponent<Transform>().position.z - before + 3) < .001f,
        "Switching to parallel jobs changed simulated motion");
    for (size_t i = 1; i < movers.size(); ++i) movers[i].SetActive(false);
    const float afterParallel = movers[0].GetComponent<Transform>().position.z;
    for (int i = 0; i < 30; ++i) step(false);
    Check(std::abs(movers[0].GetComponent<Transform>().position.z - afterParallel + 3) < .001f,
        "Switching back to inline jobs changed simulated motion");
    recorder.SetRecording(wasRecording);
    std::cout << "Awake-body job selection and inline/parallel transitions passed\n";
}

static void MovingSpinningBodies()
{
    Scene scene;
    JoltPhysics3DSystem physics;physics.scene=&scene;physics.Create();
    std::vector<Entity> movers;
    for (int i = 0; i < 256; ++i) {
        auto entity=scene.CreateEntity("Moving spinning body");
        auto& transform=*entity.AddComponent<Transform>();
        transform.position={i*4.f,0,0};
        transform.rotation=glm::normalize(Quaternion(Vector3(i*.013f,i*.024f,i*.037f)));
        auto& body=*entity.AddComponent<Rigidbody>();
        body.useGravity=false;body.linearDamping=body.angularDamping=0;body.allowSleeping=false;
        entity.AddComponent<BoxCollider>();
        body.SetLinearVelocity({0,0,-24});body.SetAngularVelocity({.7f,1.3f,.2f});
        movers.push_back(entity);
    }
    std::vector<float> previous(movers.size());
    int stalled=0;
    for (int frame=0;frame<480;++frame) {
        physics.Update(scene.GetRegistry(),1.f/240);
        for (size_t i=0;i<movers.size();++i) {
            const float distance=-movers[i].GetComponent<Transform>().position.z;
            const float step=distance-previous[i];previous[i]=distance;
            if (frame>10 && (step<.095f || step>.105f))++stalled;
        }
    }
    std::cout<<"Moving spinning body discontinuities: "<<stalled<<'\n';
    Check(stalled==0,"Unedited rotating bodies stalled or rewound their translation");
    for (float distance : previous)
        Check(std::abs(distance - 47.6f) < .002f, "Rotating body's simulated travel drifted");
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
    // Exercise invalidation after the unchanged-body path has been warmed.
    collider.size = Vector3(1);
    transform.position.x = 2;
    parent.GetComponent<Transform>().scale = Vector3(2);
    step();Check(hit(8) && !hit(4), "Parent scale did not update child pose/shape");
    parent.GetComponent<Transform>().rotation = glm::angleAxis(3.14159265f, Vector3(0,1,0));
    step();Check(hit(0) && !hit(8), "Parent rotation did not update static child");
    parent.GetComponent<Transform>().rotation = Quaternion(1,0,0,0);
    parent.GetComponent<Transform>().scale = Vector3(1);
    transform.SetLocalMatrixPrefix(glm::translate(Matrix4(1), Vector3(3,0,0)));
    step();Check(hit(9) && !hit(6), "Local matrix prefix was not invalidated");
    transform.ClearLocalMatrixPrefix();
    step();Check(hit(6) && !hit(9), "Cleared local matrix prefix was not invalidated");
    auto otherParent = scene.CreateEntity("Other parent");
    otherParent.AddComponent<Transform>()->position.x = 12;
    transform.SetParent(&otherParent);
    transform.position = Vector3(2,0,0);
    step();Check(hit(14) && !hit(6), "Reparented static collider kept old world pose");
    collider.active = false;
    step();Check(!hit(14), "Disabled collider stayed active");
    collider.active = true;
    step();Check(hit(14), "Reenabled collider was not rebuilt");
    auto& body = child.GetComponent<Rigidbody>();
    body.active = false;
    step();Check(!hit(14), "Disabled body stayed active");
    body.active = true;
    body.layer = 2u;
    step();Check(physics.Raycast(Vector3(14,3,0), Vector3(0,-1,0), 6.f, 2u), "Body layer edit was lost");
    Check(!physics.Raycast(Vector3(14,3,0), Vector3(0,-1,0), 6.f, 1u), "Body layer edit kept old layer");
    collider.offset.x = 2;
    step();Check(hit(16) && !hit(14), "Collider offset edit was lost");
    child.RemoveComponent<BoxCollider>();
    step();Check(!hit(16), "Removed collider stayed in physics");
    child.AddComponent<SphereCollider>()->radius = 2;
    step();Check(hit(15.5f), "Replacement sphere collider was not created");
    child.GetComponent<SphereCollider>().radius = .25f;
    step();Check(!hit(15.5f) && hit(14), "Sphere radius edit was lost");
    body.motionType = RigidbodyMotionType::DYNAMIC;
    body.useGravity = false;
    body.linearDamping = 0;
    body.SetLinearVelocity(Vector3(3,0,0));
    for (int i=0; i<60; ++i) step();
    Check(transform.GetGlobalPosition().x > 16, "Static to dynamic edit stopped motion");
    child.RemoveComponent<Rigidbody>();
    step();Check(!hit(transform.GetGlobalPosition().x), "Removed body remained in physics");
}

static void UnchangedKinematicCommands()
{
    Scene scene;
    JoltPhysics3DSystem physics;
    physics.scene = &scene; physics.Create();
    auto entity = scene.CreateEntity("Moving platform");
    auto& transform = *entity.AddComponent<Transform>();
    auto& body = *entity.AddComponent<Rigidbody>();
    entity.AddComponent<BoxCollider>();
    body.motionType = RigidbodyMotionType::KINEMATIC;
    body.useGravity = false;
    auto step = [&] { physics.Update(scene.GetRegistry(), 1.f/60.f); };
    step();
    for (int i=0; i<60; ++i) { body.Move(Vector3(.05f,0,0)); step(); }
    Check(transform.position.x > 2.8f, "Unchanged kinematic settings dropped movement commands");
    body.StopKinematicMotion(); step(); step();
    const auto stopped = transform.position;
    for (int i=0; i<10; ++i) step();
    Check(glm::distance(stopped, transform.position) < .001f, "Kinematic stop was lost");
    transform.position = Vector3(10,0,0);
    step(); step();
    Check(std::abs(transform.position.x - 10) < .001f, "Kinematic teleport was lost");
}

static void RotationRebuilds()
{
    Scene scene;
    JoltPhysics3DSystem physics;
    physics.scene = &scene; physics.Create();
    auto parent = scene.CreateEntity("Moving truck");
    parent.AddComponent<Transform>();
    auto child = scene.CreateEntity("Attached collider");
    auto& transform = *child.AddComponent<Transform>();
    transform.SetParent(&parent);
    transform.scale = Vector3(.7f, 1.3f, 2.1f);
    child.AddComponent<Rigidbody>()->motionType = RigidbodyMotionType::STATIC;
    child.AddComponent<BoxCollider>();
    auto& recorder = Profiler::Get();
    recorder.SetRecording(true);
    auto step = [&] {
        { Profiler::FrameScope frame; physics.Update(scene.GetRegistry(), 1.f/60.f); }
        int count = 0;
        for (const auto& sample : recorder.Frames().back().samples)
            count += sample.name == "Physics body rebuild";
        return count;
    };
    step();
    int rebuilds = 0;
    for (int i = 0; i < 600; ++i) {
        auto& pose = parent.GetComponent<Transform>();
        pose.rotation = glm::angleAxis(i * .007f, glm::normalize(Vector3(.1f, 1, .2f)));
        pose.position = Vector3(i * .02f, 0, 0);
        rebuilds += step();
    }
    std::cout << "Rotation-only collider rebuilds: " << rebuilds << "/600 frames\n";
    Check(rebuilds == 0, "Rigid parent motion rebuilt an unchanged collider");
    auto& pose = parent.GetComponent<Transform>();
    const auto center = transform.GetGlobalPosition();
    Check(physics.Raycast(center + Vector3(0,5,0), Vector3(0,-1,0), 10),
          "Reused collider did not follow its parent");
    pose.scale = Vector3(2);
    Check(step() == 1, "Real parent scale change did not rebuild");
    int gradualRebuilds = 0;
    for (int i = 0; i < 100; ++i) {
        pose.scale += Vector3(5.e-7f);
        gradualRebuilds += step();
    }
    Check(gradualRebuilds > 0, "Cumulative sub-tolerance scale edits were lost");
    auto& collider = child.GetComponent<BoxCollider>();
    collider.size.x += 1.e-7f;
    Check(step() == 1, "Tiny authored collider edit was ignored");
    // Child rotation under nonuniform parent scale changes effective scale/shear.
    pose.scale = Vector3(1,2,3);
    step();
    transform.rotation = glm::angleAxis(.3f, Vector3(0,1,0));
    Check(step() == 1, "Effective scale change under nonuniform parent was ignored");
    recorder.SetRecording(false);
}

int main()
{
    try {
        for (float dt : {1.f/30.f,1.f/60.f,1.f/144.f}) StepContact(dt);
        RotationRebuilds();
        RuntimeLocks();
        for (float dt : {1.f / 30.f, 1.f / 60.f, 1.f / 144.f}) ContinuousYawLocks(dt);
        FullyLockedAuthoredRotation();
        SubframeMotionAfterTurn();
        MovingSpinningBodies();
        AdaptivePhysicsJobs();
        StaticEdits();
        UnchangedKinematicCommands();
        std::cout << "Capsule step contact, runtime rotation locks and continuous yaw turns passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
