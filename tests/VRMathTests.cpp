#include <Canis/VR/VRMath.hpp>
#include <Canis/VR/Interaction.hpp>
#include <Canis/VR/Foveation.hpp>
#include <cstdlib>
#include <iostream>
using namespace Canis;
using namespace Canis::VR;
void Require(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }
bool Near(float a, float b) { return std::abs(a-b) < 0.0001f; }
int main()
{
    auto projection = Projection(-0.7f, 0.9f, -0.6f, 0.8f, 0.05f, 100.0f);
    auto ndc = [&](Vector3 p) { auto clip = projection * Vector4(p, 1); return Vector3(clip) / clip.w; };
    Require(Near(ndc({0, 0, -0.05f}).z, -1), "Near plane must map to OpenGL -1");
    Require(Near(ndc({0, 0, -100}).z, 1), "Far plane must map to OpenGL +1");
    Require(Near(ndc({std::tan(-0.7f)*2, 0, -2}).x, -1), "Asymmetric left edge");
    Require(Near(ndc({std::tan(0.9f)*2, 0, -2}).x, 1), "Asymmetric right edge");
    bool rejected = false;
    try { Projection(-1, 1, -1, 1, 0, 10); } catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected, "Invalid clip planes rejected");
    Pose p{{0.032f, 1.65f, -1}, {1,0,0,0}, true};
    auto origin = glm::translate(Matrix4(1), Vector3(4,0,2)) * glm::rotate(Matrix4(1), PI/2, Vector3(0,1,0));
    auto world = TransformPose(origin, p);
    Require(Near(world.position.x, 3) && Near(world.position.z, 1.968f), "World origin rotates and translates tracking space");
    Require(Near(world.position.y, 1.65f), "Origin preserves floor height");
    auto view = glm::inverse(origin * PoseMatrix(p));
    auto eye = view * Vector4(world.position, 1);
    Require(glm::length(Vector3(eye)) < 0.0001f, "View transforms world eye to zero");
    p.valid = false; Require(!TransformPose(origin, p).valid, "Invalid poses stay invalid");
    SnapTurn turn;
    Require(Near(turn.Update(1, true), -PI/6), "Initial snap");
    Require(turn.Update(1, true) == 0 && turn.Update(0.5f, true) == 0, "No repeated snap while held");
    turn.Update(0, true); Require(turn.Update(-1, true) > 0, "Neutral rearms");
    turn.Update(0, false); Require(turn.Update(1, true) == 0, "Focus regain requires neutral");
    GrabOwnership grabs;
    Require(grabs.Update(0, 1, true, 42), "Left hand acquires object");
    Require(!grabs.Update(1, 1, true, 42), "Right hand cannot steal held object");
    grabs.Update(0, 0.5f, true, {}); Require(grabs.Held(0) == 42, "Grip hysteresis retains object");
    grabs.Update(0, 1, false, {}); Require(!grabs.Held(0), "Tracking loss releases object");
    Require(!grabs.Update(0, 1, true, 42), "Tracking recovery requires neutral");
    grabs.Update(0, 0, true, {}); Require(grabs.Update(0, 1, true, 42), "Neutral rearms grip");
    grabs.Update(0, 0, true, {}); Require(!grabs.IsHeld(42), "Release clears ownership");
    Pose aim{{0,1,0}, glm::angleAxis(-PI/4, Vector3(1,0,0)), true};
    auto target = FloorTeleport(aim, {0,1.65f,0}, {-2,-2}, {2,2});
    Require(target && Near(target->y, 0) && Near(target->z, -1), "Teleport hits floor");
    Require(!FloorTeleport(aim, {0,1.65f,0}, {-0.2f,-0.2f}, {0.2f,0.2f}), "Teleport rejects outside standing area");
    aim.orientation = Quaternion(1,0,0,0);
    Require(!FloorTeleport(aim, {0,1.65f,0}, {-2,-2}, {2,2}), "Horizontal aim cannot teleport");
    auto rates = ShadingRateMap(1600, 1600, 16, 16, {0.5f,0.5f});
    Require(rates.size() == 10000 && rates[50*100+50] == 0 && rates[0] == 2, "Foveation preserves center and reduces peripheral shading");
    auto odd = ShadingRateMap(17, 33, 16, 16, {0.5f,0.5f});
    Require(odd.size() == 6, "Foveation rounds partial tiles up");
    auto shifted = ShadingRateMap(1600, 1600, 16, 16, {0.1f,0.5f});
    Require(shifted[50*100+10] == 0 && shifted[50*100+90] == 2, "Foveation follows requested center");
    Pose gaze{{0,0,0}, {1,0,0,0}, true}, eyePose = gaze;
    Vector2 center;
    Require(GazeCenter(gaze, eyePose, Projection(-PI/4,PI/4,-PI/4,PI/4,0.05f,100), center), "Valid gaze projected");
    Require(Near(center.x, 0.5f) && Near(center.y, 0.5f), "Forward gaze is centered");
    gaze.valid = false;
    Require(!GazeCenter(gaze, eyePose, projection, center), "Invalid gaze uses unfoveated fallback");
    return 0;
}
