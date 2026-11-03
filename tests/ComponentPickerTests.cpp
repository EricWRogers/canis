#include <Canis/ComponentPicker.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace Canis;
using namespace Canis::Scripting;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct NativeCounter
{
    static constexpr const char* ScriptName = "PickerTest::NativeCounter";
    Entity* entity = nullptr;
    int created = 0;
    void Create() { ++created; }
};
struct NativeScript : ScriptableEntity
{
    static constexpr const char* ScriptName = "PickerTest::NativeScript";
    explicit NativeScript(Entity& e) : ScriptableEntity(e) {}
};
static ComponentConf nativeConf;
static ScriptConf scriptConf;
static void Publish(const ManagedJson& descriptions)
{
    const auto text = descriptions.dump();
    const NativeValue value = Encode(text);
    NativeBindingRegistry::Get().Invoke("Managed.Publish", {&value, 1});
}
struct Fixture
{
    App app;
    ManagedJson descriptions;
    Entity owner;
    Fixture() : owner(app.scene.CreateEntity("Owner"))
    {
        app.scene.app = &app;
        nativeConf = {}; scriptConf = {};
        DEFAULT_COMPONENT_CONFIG(nativeConf, NativeCounter);
        DEFAULT_CONFIG(scriptConf, NativeScript);
        app.RegisterComponent(nativeConf); app.RegisterScript(scriptConf);
        NativeBindingRegistry::Get().RemoveOwner("Canis.Scene");
        RegisterManagedBindings(app, [](Entity*) { return uint64_t(0); }, [](uint64_t) { return Entity(nullptr); });
        const auto source = (std::filesystem::temp_directory_path() / "CanisPickerFixture.cs").string();
        descriptions = ManagedJson::array({
            {{"id", "18446744073709551615"}, {"name", "Managed Counter"}, {"source", source}, {"aliases", ManagedJson::array({"legacy.counter"})},
             {"fields", ManagedJson::array({{{"name", "count"}, {"default", 13}}, {{"name", "label"}, {"default", "counter"}}})}},
            {{"id", "test.other"}, {"name", "Other Script"}, {"source", source}, {"aliases", ManagedJson::array()}, {"fields", ManagedJson::array()}}
        });
        Publish(descriptions);
    }
    ~Fixture() { NativeBindingRegistry::Get().RemoveOwner("Canis.Scene"); }
};
static ComponentPickerEntry Find(App& app, Entity& owner, const std::string& type)
{
    for (const auto& entry : BuildComponentPickerEntries(app, owner)) if (entry.componentName == type) return entry;
    throw std::runtime_error("Picker entry missing: " + type);
}
static bool Contains(App& app, Entity& owner, const std::string& type)
{
    for (const auto& entry : BuildComponentPickerEntries(app, owner)) if (entry.componentName == type) return true;
    return false;
}
static void CommonPicker()
{
    Fixture f;
    auto native = Find(f.app, f.owner, NativeCounter::ScriptName);
    auto script = Find(f.app, f.owner, NativeScript::ScriptName);
    auto managed = Find(f.app, f.owner, "18446744073709551615");
    Check(!native.managed && !script.managed && managed.managed && managed.displayName == "Managed Counter (C#)",
          "Native and managed scripts do not share the picker");
    Check(AddPickedComponent(f.app, f.owner, native) && f.owner.GetComponent<NativeCounter>().created == 1,
          "Native component did not use its existing Add/Create path");
    Check(AddPickedComponent(f.app, f.owner, script) && f.owner.HasScript<NativeScript>(), "C++ script was not attached");
    Check(AddPickedComponent(f.app, f.owner, managed), "C# script was not attached through common picker");
    auto& attachment = *f.owner.GetComponent<ManagedComponents>().items.front();
    Check(attachment.type == "18446744073709551615" && attachment.fields["count"] == 13 && attachment.fields["label"] == "counter",
          "C# stable identity or default fields changed");
    Check(!AddPickedComponent(f.app, f.owner, native) && !AddPickedComponent(f.app, f.owner, managed), "Duplicate add was accepted");
    Check(!Contains(f.app, f.owner, native.componentName) && !Contains(f.app, f.owner, script.componentName) &&
          !Contains(f.app, f.owner, managed.componentName), "Attached components remain in picker");
    attachment.fields["count"] = 42;
    auto other = f.app.scene.CreateEntity("Independent defaults");
    Check(AddManagedComponent(other, "legacy.counter") && other.GetComponent<ManagedComponents>().items.front()->fields["count"] == 13,
          "Alias attachment or independent default fields failed");
    YAML::Node saved; EncodeManagedComponents(saved, f.owner);
    Check(saved["Canis::ManagedScripts"][0]["type"].as<std::string>() == "18446744073709551615" &&
          saved["Canis::ManagedScripts"][0]["fields"]["count"].as<int>() == 42, "Picker attachment changed scene serialization");
    Publish(ManagedJson::array());
    for (const auto& entry : BuildComponentPickerEntries(f.app, other)) Check(!entry.managed, "Unavailable C# types appear in picker");
    Check(!AddManagedComponent(other, "unknown"), "Unknown C# type was added");
}
static void AliasesAndAssets()
{
    Fixture f;
    DecodeManagedComponents(YAML::Load("Canis::ManagedScripts: [{type: legacy.counter, fields: {count: 99}}]"), f.owner);
    Check(!Contains(f.app, f.owner, "18446744073709551615") && !AddManagedComponent(f.owner, "legacy.counter"),
          "Saved alias did not prevent duplicate attachment");
    Check(AddManagedComponentsFromAsset(f.owner, f.descriptions[0]["source"].get<std::string>()), "Shared Add button asset attachment failed");
    auto& items = f.owner.GetComponent<ManagedComponents>().items;
    Check(items.size() == 2 && items.front()->fields["count"] == 99 && items.back()->type == "test.other",
          "Asset attachment overwrote saved fields or missed a second type in the source file");
    Check(!AddManagedComponentsFromAsset(f.owner, f.descriptions[0]["source"].get<std::string>()) &&
          !AddManagedComponentsFromAsset(f.owner, "unrelated.txt"), "Asset drop created duplicates or an unknown script");
}
#if CANIS_EDITOR
namespace Canis
{
    // Reuse the editor's existing test access seam without exposing panel state publicly.
    struct EditorSceneTabsTestAccess
    {
        static void Picker(Editor& editor, App& app, Entity& owner)
        {
            editor.m_app = &app; editor.m_scene = &app.scene; editor.m_index = app.scene.GetEntityIndex(owner);
            editor.DrawAddComponentDropDown(false);
        }
    };
}
static void SharedPopupUI()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = {1000, 800}; io.DeltaTime = 1.f / 60;
    unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    {
        Fixture f; Editor editor; ImVec2 add; bool popup = false;
        auto frame = [&](ImVec2 mouse = {-100, -100}, bool down = false)
        {
            io.AddMousePosEvent(mouse.x, mouse.y); io.AddMouseButtonEvent(0, down);
            ImGui::NewFrame(); ImGui::SetNextWindowPos({0, 0}); ImGui::SetNextWindowSize({700, 650});
            ImGui::Begin("Common picker", nullptr, ImGuiWindowFlags_NoSavedSettings);
            EditorSceneTabsTestAccess::Picker(editor, f.app, f.owner);
            const auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax(); add = {(min.x + max.x) / 2, (min.y + max.y) / 2};
            popup = ImGui::IsPopupOpen("Add Component");
            ImGui::End(); ImGui::Render();
        };
        frame(); const auto point = add; frame(point); frame(point, true); frame(point, false); frame();
        Check(popup, "Common Add Component popup did not open");
        io.AddInputCharactersUTF8("Managed Counter"); frame(); frame();
        io.AddKeyEvent(ImGuiKey_Enter, true); frame(); io.AddKeyEvent(ImGuiKey_Enter, false); frame();
        Check(f.owner.HasComponent<ManagedComponents>() && f.owner.GetComponent<ManagedComponents>().items.size() == 1 &&
              f.owner.GetComponent<ManagedComponents>().items.front()->fields["count"] == 13 && !popup,
              "Searching and pressing Enter did not attach the C# script in the C++ picker");
        // An empty entity's managed inspector contributes no separate add/drop controls.
        auto empty = f.app.scene.CreateEntity("Empty inspector");
        ImGui::NewFrame(); ImGui::Begin("Managed fields only");
        const auto before = ImGui::GetCursorPos(); DrawManagedInspector(editor, empty); const auto after = ImGui::GetCursorPos();
        Check(before.x == after.x && before.y == after.y, "Separate C# attachment controls remain in Inspector");
        ImGui::End(); ImGui::Render();
    }
    ImGui::DestroyContext();
}
#endif
int main()
{
    try { CommonPicker(); AliasesAndAssets();
#if CANIS_EDITOR
        SharedPopupUI();
#endif
        std::cout << "Unified component picker checks passed\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
