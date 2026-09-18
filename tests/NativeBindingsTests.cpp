#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/Scripting/WebBindings.hpp>
#include <Canis/External/tinygltf/json.hpp>
#include <Canis/Scripting/SceneBindings.hpp>
#include <Canis/App.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Components.hpp>
#include <array>
#include <cstdlib>
#include <iostream>
using namespace Canis;
using namespace Canis::Scripting;
static void Check(bool value, const char* message) { if (!value) { std::cerr << message << '\n'; std::exit(1); } }
template<class F> static void Throws(F f) { try { f(); } catch (const std::invalid_argument&) { return; } Check(false, "Expected native binding rejection"); }
int main()
{
    auto& registry = NativeBindingRegistry::Get();
    float speed = 12;
    registry.Variable("test", "Tuning", "Speed", &speed);
    registry.Variable("test", "Tuning", "ReadOnly", &speed, true);
    registry.Method("test", "Tuning", "Multiply", [](float x, float y) { return x*y; });
    registry.Method("test", "Tuning", "Echo", [](std::string text) { return text; });
    std::array values{Encode(3.f), Encode(4.f)};
    Check(Decode<float>(registry.Invoke("Tuning.Multiply",values)) == 12, "Method arguments/result lost");
    registry.Invoke("Tuning.Speed.set",std::span(values).first(1));
    Check(speed == 3 && Decode<float>(registry.Invoke("Tuning.Speed.get",{})) == 3, "Variable did not update native storage");
    Throws([&] { registry.Invoke("Tuning.ReadOnly.set",std::span(values).first(1)); });
    Throws([&] { registry.Invoke("Tuning.Multiply",{}); });
    values[0] = Encode(1);
    Throws([&] { registry.Invoke("Tuning.Multiply",values); });
    Throws([&] { registry.Method("other","Tuning","Speed",[] { return 0; }); });
    Throws([&] { registry.Method("test","bad-name","X",[] {}); });
    Throws([&] { registry.Method("test","Same","Same",[] {}); });
    auto text = Encode(std::string("Hello \xE2\x98\x83"));
    Check(Decode<std::string>(registry.Invoke("Tuning.Echo",{&text,1})) == "Hello \xE2\x98\x83", "UTF-8 round trip failed");
    const auto q = Quaternion(.5f,.1f,.2f,.3f);
    Check(Decode<Quaternion>(Encode(q)) == q, "Quaternion ordering changed");
    Check(Decode<uint64_t>(Encode(uint64_t(0xFFFFFFFFFFFFFFFE))) == 0xFFFFFFFFFFFFFFFE, "Handle precision lost");
    const auto generated = registry.GenerateCSharp();
    Check(generated.find("public static float @Speed { get") != std::string::npos, "Typed property missing");
    Check(generated.find("Tuning.ReadOnly.set") == std::string::npos, "Read-only property generated setter");
    registry.Method("test","Tuning","Fail",[] { throw std::invalid_argument("expected error"); });
    NativeValue result; unsigned char error[64] = {};
    Check(DispatchNative(reinterpret_cast<const unsigned char*>("Tuning.Fail"),11,nullptr,0,&result,error,64) == -1, "Exception crossed ABI");
    Check(std::string(reinterpret_cast<char*>(error)) == "expected error", "Exception text lost");
    const auto web = [](const std::string& request) { return nlohmann::json::parse(DispatchWebBinding(request)); };
    registry.Method("test", "Tuning", "Handle", [](uint64_t value) { return value; });
    Check(web(R"({"name":"Tuning.Handle","arguments":[{"kind":5,"handle":"18446744073709551614"}]})")["result"]["handle"] == "18446744073709551614", "Web handle precision lost");
    Check(web(R"({"name":"Tuning.Multiply","arguments":[{"kind":3,"numbers":[3]},{"kind":3,"numbers":[4]}]})")["result"]["numbers"][0] == 12, "Web numeric round trip failed");
    Check(web(R"({"name":"Tuning.Echo","arguments":[{"kind":9,"text":"Hello \u2603"}]})")["result"]["text"] == "Hello \xE2\x98\x83", "Web UTF-8 round trip failed");
    Check(web(R"({"name":"Tuning.Fail","arguments":[]})")["error"] == "expected error", "Web exception crossed boundary");
    for (const auto* invalid : {
        "not json",
        R"({"name":"Tuning.Multiply","arguments":[]})",
        R"({"name":"Tuning.Multiply","arguments":[{"kind":2,"numbers":[3]},{"kind":3,"numbers":[4]}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":5,"handle":"18446744073709551616"}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":5,"handle":123}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":5,"handle":"-1"}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":5,"handle":"1junk"}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":2,"numbers":[2147483648]}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":2,"numbers":[1.5]}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":1,"numbers":[2]}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":6,"numbers":[1,2]}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":4294967297,"numbers":[1]}]})",
        R"({"name":"Tuning.Handle","arguments":[{"kind":10}]})"})
        Check(web(invalid).contains("error"), "Malformed web request accepted");
    registry.RemoveOwner("test");
    Throws([&] { registry.Invoke("Tuning.Speed.get",{}); });
    Check(registry.GenerateCSharp().find("@Tuning") == std::string::npos,"Unloaded owner retained callbacks");
    {
        App app;
        Editor editor; app.RegisterDefaults(editor);
        app.scene.app = &app;
        RegisterSceneBindings(app);
        struct BindingCleanup { ~BindingCleanup() { UnregisterSceneBindings(); } } bindingCleanup;
        auto entity = app.scene.CreateEntity("BoundEntity");
        entity->AddComponent<Transform>();
        auto name = Encode(std::string("BoundEntity"));
        auto handle = registry.Invoke("Scene.Find",{&name,1});
        std::array move{handle, Encode(Vector3(1,2,3))};
        registry.Invoke("Transform.SetPosition",move);
        Check(entity->GetComponent<Transform>().position == Vector3(1,2,3),"Managed facade did not write native ECS");
        entity->AddComponent<PointLight>();
        std::array lightArgs{handle, Encode(std::string("PointLight"))};
        const auto lightToken = Decode<uint64_t>(registry.Invoke("Component.Token", lightArgs));
        // Direct ECS removal must invalidate managed wrappers, even if no managed call observes the gap.
        app.scene.GetRegistry().remove<PointLight>(entity->GetHandle());
        entity->AddComponent<PointLight>();
        Check(Decode<uint64_t>(registry.Invoke("Component.Token", lightArgs)) != lightToken,
            "Native ECS removal revived an old component wrapper");
        entity.Destroy();
        Check(!Decode<bool>(registry.Invoke("Scene.IsValid",{&handle,1})),"Destroyed entity still valid");
        entity = app.scene.CreateEntity("BoundEntity");
        name = Encode(std::string("BoundEntity"));
        auto replacement = registry.Invoke("Scene.Find",{&name,1});
        Check(replacement.data != handle.data,"Recycled entity revived old handle");
        app.scene.Unload();
        Check(!Decode<bool>(registry.Invoke("Scene.IsValid",{&replacement,1})),"Scene unload retained valid handle");
        UnregisterSceneBindings();
    }
    std::cout << "Native bindings passed\n";
}
