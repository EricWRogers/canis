#include <Canis/InputActions.hpp>
#include <Canis/InputManager.hpp>
#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace Canis;
enum class TestInputAction : uint32_t { Jump=1001, Move=1002 };
namespace Canis { template<> struct InputActionEnum<TestInputAction> : std::true_type {}; }
static void Check(bool condition, const char* message) { if (!condition) {std::cerr << message << '\n'; std::exit(1);} }
static InputDocument Defaults()
{
    InputDocument d; d.maps = {{100,"Gameplay"}};
    InputBinding space; space.id=2001; space.path="Keyboard/Space";
    InputBinding south; south.id=2002; south.path="Gamepad/South"; south.scheme=InputScheme::Gamepad;
    d.actions.push_back({1001,100,"Jump",ActionType::Button,{space,south}});
    InputBinding move; move.id=2003; move.path="Composite/Axis2D";
    move.parts={{"up","Keyboard/W"},{"down","Keyboard/S"},{"left","Keyboard/A"},{"right","Keyboard/D"}};
    InputBinding stick; stick.id=2004; stick.path="Gamepad/LeftStick"; stick.scheme=InputScheme::Gamepad; stick.deadZone=0.2f;
    d.actions.push_back({1002,100,"Move",ActionType::Axis2D,{move,stick}});
    InputBinding axis; axis.id=2005; axis.path="Gamepad/RightTrigger"; axis.scheme=InputScheme::Gamepad;
    d.actions.push_back({1003,100,"Fire",ActionType::Button,{axis}});
    d.actions.push_back({1004,100,"Throttle",ActionType::Axis1D,{InputBinding{2006,InputScheme::KeyboardMouse,"Composite/Axis1D",{{"positive","Keyboard/W"},{"negative","Keyboard/S"}}}}});
    return d;
}
static void Setup(InputActionSystem& input)
{
    auto d=Defaults(); auto schema=d; for(auto& a:schema.actions) a.bindings.clear();
    std::string error;
    Check(input.RegisterSchema(schema,error),error.c_str()); Check(input.QueueDocument(d,error),error.c_str());
    input.EnableMap({100}); input.Evaluate(true);
}
static void ControllerQueries()
{
    InputActionSystem input; Setup(input);
    input.TrackController(10, "xbox"); input.TrackController(20, "playstation");
    input.Evaluate(true);
    input.Record("Keyboard/Space", {1,0});
    input.Record("Gamepad/South", {1,0}, 1); // Synthetic input is not physical controller 0.
    input.Evaluate(true);
    Check(input.Action({1001}).down && !input.Action({1001},10).down && !input.Action({1001},20).down,
          "Controller query included keyboard or synthetic input");
    input.Record("Gamepad/South", {1,0}, 10); input.Evaluate(true);
    Check(input.Action({1001},10).pressed && !input.Action({1001},20).down && !input.Action({1001}).pressed,
          "Controller edge was masked by aggregate hold");
    const auto saved = input.Action({1001},10);
    input.Record("Gamepad/South", {1,0}, 20); input.Record("Gamepad/South", {0,0}, 20); input.Evaluate(true);
    Check(input.Action({1001},20).pressed && input.Action({1001},20).released && !input.Action({1001},20).down,
          "Independent controller same-frame tap lost");
    Check(!input.Action({1001},10).pressed && saved.pressed, "Controller snapshot mutated or edge repeated");
    Check(!saved.key && saved.button == GamepadButton::South, "Controller metadata included keyboard");
    input.Record("Gamepad/LeftStick", {0.6f,0}, 10);
    input.Record("Gamepad/LeftStick", {-1,0}, 20); input.Evaluate(true);
    Check(std::abs(input.Action({1002},10).Read<Vector2>().x - 0.5f) < 0.001f &&
          input.Action({1002},20).Read<Vector2>().x == -1, "Controller axes merged");
    input.Record("Gamepad/RightTrigger", {0.8f,0}, 10); input.Evaluate(true);
    input.Record("Gamepad/RightTrigger", {0.45f,0}, 10);
    input.Record("Gamepad/RightTrigger", {0.45f,0}, 20); input.Evaluate(true);
    Check(input.Action({1003},10).down && !input.Action({1003},20).down, "Controller hysteresis leaked");
    input.RemoveSource(20, InputCancellation::Disconnected); input.Evaluate(true);
    Check(input.Action({1002},20).canceled && !input.Action({1002},20).released &&
          input.Action({1001},10).down && !input.Action({1001},10).canceled, "Disconnect canceled another controller");
    input.DisableMap({100}); input.Evaluate(true);
    Check(input.Action({1001},10).canceled && !input.Action({1001},10).released, "Map disable missed controller");
    input.EnableMap({100}); input.Evaluate(true);
    Check(!input.Action({1001},10).down, "Held controller bypassed map neutral gate");
    input.Record("Gamepad/South", {0,0}, 10); input.Evaluate(true);
    input.Record("Gamepad/South", {1,0}, 10); input.Evaluate(true);
    input.BeginCapture(InputScheme::KeyboardMouse, ActionType::Button); input.Evaluate(true);
    Check(input.Action({1001},10).canceled && !input.Action({1001},10).released, "Capture missed controller cancellation");
    input.CancelCapture(); input.Evaluate(true);
    Check(!input.Action({1001},10).down, "Capture resume bypassed controller neutral gate");
    input.SetNativeInput(true); input.TrackController(20,"unknown",true);
    input.RecordNative({1001},{0,0}); input.Evaluate(true);
    input.SetNativePrompt({1001},{"Steam touchpad",{}});
    input.RecordNative({1001},{1,0}); input.Evaluate(true);
    Check(input.Action({1001},20).pressed && !input.Action({1001},20).button &&
          input.Action({1001},20).prompt->text == "Steam touchpad" && !input.Action({1001},10).down,
          "Native controller ownership or prompt leaked");
    input.ForgetController(20);
    Check(input.Action({1002},20).Read<Vector2>() == Vector2(0) && !input.Action({1001},20).down,
          "Missing controller did not return typed neutral");
    std::string error;
    Check(input.QueueDocument(Defaults(),error), error.c_str()); input.Evaluate(true);
    Check(!input.Action({1001},10).down, "Reload bypassed controller neutral gate");
    input.UnregisterSchema(); Setup(input); input.TrackController(10,"xbox"); input.Evaluate(true);
    Check(!input.Action({1001},10).down, "Module reload reused stale controller state");
}
int main()
{
    ControllerQueries();
    InputActionSystem input; Setup(input);
    auto jump=[&](){return input.Action({1001});};
    input.Record("Keyboard/Space",{1,0}); input.Record("Keyboard/Space",{0,0}); input.Evaluate(true);
    Check(jump().pressed && jump().released && !jump().down,"Same-frame tap lost");
    const auto saved=jump(); input.Evaluate(true);
    Check(!jump().pressed && !jump().released && saved.pressed,"Snapshot mutated or edges repeated");
    input.Record("Keyboard/Space",{1,0}); input.Record("Gamepad/South",{1,0},10); input.Evaluate(true);
    Check(jump().pressed && jump().down,"Multiple bindings did not merge");
    input.Record("Keyboard/Space",{0,0}); input.Evaluate(true);
    Check(jump().down && !jump().released && !jump().pressed,"Releasing one contribution released action");
    input.DisableMap({100}); input.Evaluate(true);
    Check(jump().canceled && !jump().released && !jump().down,"Map disable must cancel, not release");
    input.EnableMap({100}); input.Evaluate(true); Check(!jump().pressed && !jump().down,"Held control reactivated map");
    input.Record("Gamepad/South",{0,0},10); input.Evaluate(true);
    input.Record("Gamepad/South",{1,0},10); input.Evaluate(true); Check(jump().pressed,"Neutral gate did not reopen");
    input.Evaluate(false); Check(jump().canceled && !jump().released,"Capture must cancel");
    input.Evaluate(true); Check(!jump().down,"Held control reactivated after capture");
    input.Record("Gamepad/South",{0,0},10); input.Evaluate(true);
    input.Evaluate(false);
    input.Record("Keyboard/Space",{1,0}); input.Evaluate(false);
    input.Evaluate(true); Check(!jump().down,"Input arriving during capture bypassed neutral gate");
    input.Record("Keyboard/Space",{0,0}); input.Evaluate(true);
    input.Record("Gamepad/RightTrigger",{0.8f,0},10);
    input.Record("Gamepad/RightTrigger",{0,0},10); input.Evaluate(true);
    Check(input.Action({1003}).pressed && input.Action({1003}).released && !input.Action({1003}).down,"Same-frame threshold tap lost");
    input.Record("Keyboard/W",{1,0}); input.Record("Keyboard/D",{1,0}); input.Evaluate(true);
    auto move=input.Action({1002}).Read<Vector2>(); Check(std::abs(glm::length(move)-1)<0.0001f,"Digital diagonal not normalized");
    input.Record("Keyboard/S",{1,0}); input.Record("Keyboard/A",{1,0}); input.Evaluate(true);
    Check(input.Action({1002}).Read<Vector2>() == Vector2(0),"Opposing keys did not cancel");
    Check(input.Action({1004}).Read<float>() == 0,"Axis1D opposing keys did not cancel");
    input.Record("Gamepad/LeftStick",{0.1f,0},10); input.Evaluate(true);
    Check(input.Action({1002}).Read<Vector2>() == Vector2(0),"Stick drift bypassed dead zone");
    input.Record("Gamepad/LeftStick",{0.6f,0},10); input.Evaluate(true);
    Check(std::abs(input.Action({1002}).Read<Vector2>().x-0.5f)<0.0001f,"Dead zone applied incorrectly");
    input.Record("Gamepad/RightTrigger",{0.6f,0},10); input.Evaluate(true); Check(input.Action({1003}).pressed,"Threshold did not press");
    input.Record("Gamepad/RightTrigger",{0.45f,0},10); input.Evaluate(true); Check(input.Action({1003}).down,"Threshold hysteresis lost");
    input.Record("Gamepad/RightTrigger",{0.3f,0},10); input.Evaluate(true); Check(input.Action({1003}).released,"Threshold did not release");
    input.Record("Gamepad/South",{1,0},10); input.Evaluate(true);
    input.RemoveSource(10,InputCancellation::Disconnected); input.Evaluate(true);
    Check(jump().canceled && !jump().released,"Disconnect must cancel");
    Check(jump().key == Key::SPACE && jump().button == GamepadButton::South,"Binding metadata unavailable");
    Check(input.Prompt({1002},InputScheme::KeyboardMouse).text == "W / A / S / D","Composite prompt order wrong");
    auto document=Defaults(); std::string error;
    document.actions[0].name="Renamed";
    Check(!input.QueueDocument(document,error),"Incompatible schema accepted");
    Check(input.Document().actions[0].name=="Jump","Invalid reload replaced active document");
    document=Defaults(); document.actions[0].bindings[0].path="Keyboard/Enter";
    Check(input.QueueDocument(document,error),error.c_str()); input.Evaluate(true);
    Check(jump().key == Key::RETURN,"Compatible binding reload did not apply");
    document.actions[0].bindings[0].id=2002; Check(!ValidateInputDocument(document,error),"Duplicate ID accepted");
    document=Defaults(); document.reservedIds={2001}; Check(!ValidateInputDocument(document,error),"Reserved ID reused");
    document=Defaults(); document.actions[0].name="class"; Check(!ValidateInputDocument(document,error),"C++ keyword accepted");
    document=Defaults(); document.actions[1].bindings[0].parts.erase("up"); Check(!ValidateInputDocument(document,error),"Malformed composite accepted");
    // Synthetic changes are collected after SDL Update, and must survive until publication.
    InputManager manager; Setup(manager.Actions());
    Check(!manager.Action(TestInputAction::Jump, 0).down &&
          manager.Action(TestInputAction::Move, 999).Read<Vector2>() == Vector2(0),
          "Indexed enum overload did not return typed neutral for missing controllers");
    manager.BeginSyntheticInputFrame(); manager.SetSyntheticKey(Key::SPACE,true); manager.SetSyntheticKey(Key::SPACE,false);
    manager.EvaluateActions();
    Check(manager.Action(ActionId{1001}).pressed && manager.Action(ActionId{1001}).released,"Synthetic tap lost at frame boundary");
    InputActionSystem independent; Setup(independent);
    independent.Record("Keyboard/Space",{1,0},0); independent.Record("Keyboard/Space",{1,0},1); independent.Evaluate(true);
    independent.Record("Keyboard/Space",{0,0},1); independent.Evaluate(true);
    Check(independent.Action({1001}).down && !independent.Action({1001}).released,"Synthetic release cleared physical key");
    auto tie=Defaults(); tie.actions[1].bindings[1].deadZone=0;
    Check(independent.QueueDocument(tie,error),error.c_str()); independent.Evaluate(true);
    independent.Record("Keyboard/W",{1,0}); independent.Record("Gamepad/LeftStick",{1,0},5); independent.Evaluate(true);
    Check(independent.Action({1002}).value == Vector2(0,1),"Equal-magnitude tie ignored binding ID");
    independent.Cancel(InputCancellation::FocusLost); independent.Evaluate(false);
    Check(independent.Action({1002}).cancellation == InputCancellation::FocusLost,"Focus reason replaced by capture");
    InputActionSystem resume; Setup(resume);
    resume.Evaluate(false);
    resume.Record("Keyboard/Space",{1,0}); resume.Evaluate(true);
    Check(!resume.Action({1001}).down && !resume.Action({1001}).pressed,"Focus-gain sample bypassed neutral gate");
    resume.Record("Keyboard/Space",{0,0}); resume.Evaluate(true);
    resume.Record("Keyboard/Space",{1,0}); resume.Evaluate(true);
    Check(resume.Action({1001}).pressed,"Physical release did not reopen focus gate");
    InputDocument parsed;
    Check(!ParseInputDocument("version: 1\nversion: 1\nmaps: []",parsed,error),"Duplicate YAML fields accepted");
    Check(!ParseInputDocument("version: 2\nmaps: []",parsed,error),"Unknown YAML version accepted");
    std::cout << "Input action tests passed\n";
}
