#include <Canis/InputActions.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
using namespace Canis;
static void Check(bool ok,const std::string& message) {if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
int main()
{
    InputDocument document;
    std::string error;
    Check(ParseInputDocument(R"(version: 1
reservedIds: [99]
maps:
- id: 10
  name: Gameplay
  actions:
  - id: 11
    name: Jump
    type: Button
    bindings:
    - {id: 12, scheme: KeyboardMouse, path: Keyboard/Space}
    - {id: 13, scheme: Gamepad, path: Gamepad/South}
  - id: 14
    name: Move
    type: Axis2D
    bindings:
    - {id: 15, scheme: Gamepad, path: Gamepad/LeftStick, deadZone: 0.2}
)",document,error),error);
    Check(AllocateInputId(document)==100,"Allocator reused retired IDs");
    InputDocument roundtrip;
    Check(ParseInputDocument(SerializeInputDocument(document),roundtrip,error),error);
    Check(SerializeInputDocument(document)==SerializeInputDocument(roundtrip),"Serialization changed IDs or bindings");
    InputActionSystem input;auto schema=document;for(auto& a:schema.actions)a.bindings.clear();
    Check(input.RegisterSchema(schema,error),error);Check(input.QueueDocument(document,error),error);input.EnableMap({10});input.Evaluate(true);
    input.BeginCapture(InputScheme::KeyboardMouse,ActionType::Button);input.Evaluate(true);
    input.Record("Keyboard/Enter",{1,0});input.Evaluate(true);
    Check(input.IsCapturing(),"Capture resumed before control release");
    Check(input.TakeCapturedControl()==std::optional<std::string>("Keyboard/Enter"),"Capture missed control");
    Check(!input.Action({11}).down,"Capture drove gameplay");
    input.Record("Keyboard/Enter",{0,0});input.Evaluate(true);input.Evaluate(true);
    Check(!input.IsCapturing(),"Capture failed to resume after neutral");
    input.BeginCapture(InputScheme::KeyboardMouse,ActionType::Button);input.Evaluate(true);
    input.Record("Keyboard/Escape",{1,0});input.Evaluate(true);
    Check(!input.IsCapturing() && !input.TakeCapturedControl(),"Escape did not cancel capture");
    input.Record("Keyboard/Escape",{0,0});input.Evaluate(true);
    auto rebound=input.Bindings({11}).front();rebound.path="Keyboard/Enter";rebound.iconOverride="custom.png";
    Check(input.Rebind({11},12,rebound,error),error);input.Evaluate(true);
    Check(input.Action({11}).key==40 && input.Action({11}).icon.texture=="custom.png","Rebind metadata/glyph missing");
    const auto snapshot=input.Action({11});
    const auto directory=std::filesystem::temp_directory_path()/std::filesystem::path("canis-input-rebinding-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);const auto file=(directory/"overrides.canis").string();
    Check(input.SaveOverrides(file,error),error);
    input.ResetBindings();input.Evaluate(true);Check(input.Action({11}).key==44,"Reset did not restore packaged default");
    Check(snapshot.icon.texture=="custom.png","Old snapshot prompt changed after reset");
    Check(input.LoadOverrides(file,error),error);input.Evaluate(true);Check(input.Action({11}).key==40,"Saved override was not restored by ID");
    {std::ofstream out(file);out<<"version: 1\noverrides:\n- action: 999\n  binding: {id: 998, path: Keyboard/A}\n";}
    Check(input.LoadOverrides(file,error),error);input.Evaluate(true);Check(input.Action({11}).key==44,"Obsolete overrides damaged defaults");
    {std::ofstream out(file);out<<"invalid: [";}
    Check(!input.LoadOverrides(file,error),"Malformed overrides accepted");input.Evaluate(true);Check(input.Action({11}).key==44,"Malformed overrides did not reset safely");
    input.SetNativeInput(true);input.RecordNative({11},{1,0});input.Evaluate(true);
    Check(!input.Action({11}).down,"Backend switch bypassed neutral gate");
    input.RecordNative({11},{0,0});input.Evaluate(true);
    input.RecordNative({11},{1,0});input.RecordNative({14},{0.1f,0});
    input.SetNativePrompt({11},{"Left touchpad",{}});input.Evaluate(true);
    Check(input.Action({11}).pressed && !input.Action({11}).button,"Steam logical button invented a physical button");
    Check(input.Action({14}).Read<Vector2>().x==0.1f,"Native analog value was dead-zoned twice");
    Check(input.Prompt({11},InputScheme::Gamepad).text=="Left touchpad","Steam origin prompt not used");
    input.SetNativeInput(false);input.Evaluate(true);
    Check(input.Action({11}).canceled && !input.Action({11}).released,"Backend loss invented release");
    std::filesystem::remove_all(directory);
    std::cout<<"Input capture, overrides, glyphs and native-provider tests passed\n";
}
