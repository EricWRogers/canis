#include <Canis/ConfigHelper.hpp>

namespace Canis
{
    void DrawSignalInspector(Editor* editor, const char* label, const char* suffix, SignalBase& signal)
    {
#if CANIS_EDITOR
        const auto shortName = [](const std::string& name)
        {
            const auto separator = name.rfind("::");
            return separator == std::string::npos ? name : name.substr(separator + 2);
        };
        const auto changed = [&] { if (editor) editor->NotifySceneChanged(); };
        ImGui::PushID(label);
        if (suffix) ImGui::PushID(suffix);
        if (ImGui::TreeNode("Signal", "%s (%zu listener%s)", label, signal.listeners.size(), signal.listeners.size() == 1 ? "" : "s"))
        {
            if (!signal.ArgumentName().empty()) ImGui::TextDisabled("Emits: %s", signal.ArgumentName().c_str());
            for (size_t i = 0; i < signal.listeners.size(); ++i)
            {
                auto& listener = signal.listeners[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::Separator();
                DrawInspectorField<bool>(editor, "Enabled", nullptr, listener.enabled);
                if (ImGui::IsItemEdited()) changed();
                const auto previousTarget = listener.target;
                DrawInspectorField<Entity>(editor, "Target", nullptr, listener.target);
                if (listener.target != previousTarget) changed();
                // Keep authored names when a target changes: compatible methods
                // remain selected; missing ones are shown instead of discarded.
                struct Option { std::string label, script, method; SignalArgumentMode mode; SignalConstant constant; };
                std::vector<Option> options;
                if (listener.target && listener.target.scene.app)
                    for (auto& conf : listener.target.scene.app->GetScriptRegistry())
                    {
                        if (!conf.Has || !conf.Get) continue;
                        auto target = listener.target;
                        if (!conf.Has(target) || !conf.Get(target)) continue;
                        for (const auto& [name, action] : conf.signalActions)
                        {
                            const auto display = shortName(conf.name) + " / " + name;
                            if (action.argumentType == typeid(void) || action.argumentType == signal.ArgumentType())
                                options.push_back({display + (action.argumentType == typeid(void) ? " ()" : " (event value)"),
                                    conf.name, name, SignalArgumentMode::Dynamic, {}});
                            SignalConstant constant;
                            if (action.argumentType == typeid(float)) constant = 0.0f;
                            else if (action.argumentType == typeid(int)) constant = 0;
                            else if (action.argumentType == typeid(bool)) constant = false;
                            else if (action.argumentType == typeid(std::string)) constant = std::string{};
                            if (constant.index() != 0)
                                options.push_back({display + " (constant " + action.argumentName + ")",
                                    conf.name, name, SignalArgumentMode::Constant, constant});
                        }
                    }
                std::sort(options.begin(), options.end(), [](const auto& a, const auto& b) { return a.label < b.label; });
                auto preview = listener.method.empty() ? "Choose a method..." : shortName(listener.script) + " / " + listener.method +
                    (listener.argumentMode == SignalArgumentMode::Constant ? " (constant)" : " (event)");
                for (const auto& option : options)
                    if (option.script == listener.script && option.method == listener.method && option.mode == listener.argumentMode)
                        preview = option.label;
                DrawInspectorFieldLabel("Method");
                SetNextInspectorFieldItemWidth();
                if (ImGui::BeginCombo("##Method", preview.c_str()))
                {
                    if (ImGui::Selectable("None", listener.method.empty())) { listener.script.clear(); listener.method.clear(); changed(); }
                    for (const auto& option : options)
                    {
                        const auto optionID = option.script + "::" + option.method + std::to_string(static_cast<int>(option.mode));
                        ImGui::PushID(optionID.c_str());
                        const bool selected = option.script == listener.script && option.method == listener.method && option.mode == listener.argumentMode;
                        if (ImGui::Selectable(option.label.c_str(), selected))
                        {
                            listener.script = option.script;
                            listener.method = option.method;
                            listener.argumentMode = option.mode;
                            if (listener.constant.index() != option.constant.index()) listener.constant = option.constant;
                            changed();
                        }
                        if (selected) ImGui::SetItemDefaultFocus();
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", option.script.c_str());
                        ImGui::PopID();
                    }
                    if (options.empty()) ImGui::TextDisabled("Assign an entity with registered signal actions.");
                    ImGui::EndCombo();
                }
                if (listener.argumentMode == SignalArgumentMode::Constant)
                    std::visit([&](auto& constant)
                    {
                        using T = std::decay_t<decltype(constant)>;
                        if constexpr (!std::is_same_v<T, std::monostate>)
                        {
                            DrawInspectorField<T>(editor, "Value", nullptr, constant);
                            if (ImGui::IsItemEdited()) changed();
                        }
                    }, listener.constant);
                if (!listener.method.empty() && !signal.IsListenerValid(listener))
                    ImGui::TextColored(ImVec4(1, .65f, .25f, 1), "Missing target, method, or incompatible argument.");
                if (ImGui::Button("Remove listener"))
                {
                    signal.listeners.erase(signal.listeners.begin() + i);
                    changed();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add listener")) { signal.listeners.emplace_back(); changed(); }
            ImGui::TreePop();
        }
        if (suffix) ImGui::PopID();
        ImGui::PopID();
#else
        (void)editor; (void)label; (void)suffix; (void)signal;
#endif
    }
}
