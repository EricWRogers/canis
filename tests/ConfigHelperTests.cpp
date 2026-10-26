#include <Canis/ConfigHelper.hpp>
#include <iostream>
#include <stdexcept>

using namespace Canis;

static void Check(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

struct Properties
{
    Entity entity;
    float speed = 4;
    std::vector<Entity> targets;
};

static void RegistrationAndReferences()
{
    Scene scene;
    Properties properties{scene.CreateEntity("Owner")};
    auto first = scene.CreateEntity("First");
    auto second = scene.CreateEntity("Second");
    ScriptConf conf;
    REGISTER_PROPERTY(conf, Properties, speed);
    REGISTER_PROPERTY(conf, Properties, targets);
    Check(conf.registry.propertyOrder == std::vector<std::string>{"speed", "targets"},
          "Registration changed field order");
    Check(conf.registry.animationTypes.at("speed") == AnimationValueType::FLOAT &&
              !conf.registry.animationTypes.contains("targets"),
          "Scalar animation metadata or non-animatable list registration changed");
    conf.registry.animationSetters.at("speed")(&properties, AnimationValue::Float(7));
    Check(properties.speed == 7 && conf.registry.animationGetters.at("speed")(&properties).AsFloat() == 7,
          "Registered animation did not access the real property");
    YAML::Node references(YAML::NodeType::Sequence);
    for (auto uuid : {second.GetUUID(), UUID(0), first.GetUUID(), UUID(999)})
        references.push_back(uuid);
    conf.registry.setters.at("targets")(references, &properties);
    Check(properties.targets.size() == 4 && properties.targets[0] == second &&
              !properties.targets[1] && properties.targets[2] == first &&
              !properties.targets[3] && properties.targets[3].GetUUID() == UUID(999),
          "Entity list loading lost order, null entries or unresolved reference identity");
    auto encoded = conf.registry.getters.at("targets")(&properties);
    Check(YAML::Dump(encoded) == YAML::Dump(references), "Entity references did not round-trip");
    for (const char* malformed : {"null", "42", "{invalid: list}"})
    {
        auto node = YAML::Load(malformed);
        conf.registry.setters.at("targets")(node, &properties);
        Check(YAML::Dump(conf.registry.getters.at("targets")(&properties)) == YAML::Dump(references),
              "Malformed list data replaced existing references");
    }
    YAML::Node empty(YAML::NodeType::Sequence);
    conf.registry.setters.at("targets")(empty, &properties);
    Check(properties.targets.empty() && conf.registry.getters.at("targets")(&properties).IsSequence(),
          "An empty list did not clear and serialize as a sequence");
}

#if CANIS_EDITOR
static void InspectorLists()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 700);
    io.DeltaTime = 1.f / 60;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    {
        Scene scene;
        auto first = scene.CreateEntity("First");
        auto second = scene.CreateEntity("Second");
        std::vector<Entity> targets{first, second}, otherTargets{second};
        std::vector<bool> flags{true, false};
        Editor editor;
        ImVec2 add, remove;
        std::vector<UUID> drawn;
        editor.RegisterInspectorFieldDrawer<Entity>(
            [&](Editor&, const char* label, const char*, Entity& value)
            {
                ImGui::Text("Target %s", label);
                if (drawn.empty())
                {
                    const auto cursor = ImGui::GetCursorScreenPos();
                    remove = ImVec2(cursor.x + 15, cursor.y + ImGui::GetFrameHeight() / 2);
                }
                drawn.push_back(value.GetUUID());
            });
        auto frame = [&](ImVec2 mouse = ImVec2(-100, -100), bool down = false, bool open = true)
        {
            drawn.clear();
            io.AddMousePosEvent(mouse.x, mouse.y);
            io.AddMouseButtonEvent(0, down);
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(780, 680));
            ImGui::Begin("Property lists", nullptr, ImGuiWindowFlags_NoSavedSettings);
            ImGui::SetNextItemOpen(open, ImGuiCond_Always);
            DrawInspectorField<std::vector<Entity>>(editor, "Targets", "first", targets);
            auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
            add = ImVec2((min.x + max.x) / 2, (min.y + max.y) / 2);
            ImGui::SetNextItemOpen(open, ImGuiCond_Always);
            DrawInspectorField<std::vector<Entity>>(editor, "Targets", "second", otherTargets);
            ImGui::SetNextItemOpen(open, ImGuiCond_Always);
            DrawInspectorField<std::vector<bool>>(editor, "Flags", flags);
            ImGui::End();
            ImGui::Render();
        };
        auto click = [&](ImVec2 point)
        {
            frame(point);
            frame(point, true);
            frame(point, false);
        };
        frame();
        Check(drawn == std::vector<UUID>{first.GetUUID(), second.GetUUID(), second.GetUUID()},
              "List elements did not use the registered entity drawer");
        click(add);
        Check(targets.size() == 3 && !targets.back() && otherTargets.size() == 1,
              "Add failed or identical field labels collided between components");
        frame();
        click(remove);
        Check(targets.size() == 2 && targets[0] == second && !targets[1] && otherTargets.size() == 1,
              "Remove erased the wrong element or another component's list");
        frame(ImVec2(-100, -100), false, false);
        Check(drawn.empty(), "Collapsed lists still drew their children");
    }
    ImGui::DestroyContext();
}
#endif

int main()
{
    try
    {
        RegistrationAndReferences();
#if CANIS_EDITOR
        InspectorLists();
#endif
        std::cout << "Config helper properties and lists passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
