#include <Canis/Editor.hpp>
#include <Canis/ConfigData.hpp>
#include <Canis/Debug.hpp>
#if CANIS_EDITOR
#include <imgui.h>
#endif

namespace Canis
{
#if CANIS_EDITOR
    namespace
    {
        template<class Copy, class Paste>
        void DrawValuesMenu(Editor& editor, ComponentClipboard& clipboard, bool canCopy, bool canPaste,
                            Copy copy, Paste paste)
        {
            ImGui::Separator();
            std::string error;
            if (ImGui::MenuItem("Copy Component Values", nullptr, false, canCopy) && !copy(error))
                Debug::Warning("Copy component values: %s", error.c_str());
            if (ImGui::MenuItem("Paste Component Values", nullptr, false, canPaste))
            {
                if (paste(error)) editor.NotifySceneChanged();
                else Debug::Warning("Paste component values: %s", error.c_str());
            }
            if (!canPaste && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                if (clipboard.TypeName().empty()) ImGui::SetTooltip("Copy a component's values first.");
                else ImGui::SetTooltip("Copied type: %s. Paste onto the same component type.", clipboard.TypeName().c_str());
            }
            ImGui::Separator();
        }
    }
#endif

    void Editor::DrawComponentValueMenu(Entity& entity, const ScriptConf& conf)
    {
#if CANIS_EDITOR
        DrawValuesMenu(*this, m_componentClipboard, ComponentClipboard::CanCopy(conf, entity),
            m_componentClipboard.CanPaste(conf, entity),
            [&](std::string& error) { return m_componentClipboard.Copy(conf, entity, error); },
            [&](std::string& error) { return m_componentClipboard.Paste(conf, entity, error); });
#endif
    }

    void Editor::DrawManagedComponentValueMenu(Entity& entity, uint64_t token)
    {
#if CANIS_EDITOR
        DrawValuesMenu(*this, m_componentClipboard, true, m_componentClipboard.CanPasteManaged(entity, token),
            [&](std::string& error) { return m_componentClipboard.CopyManaged(entity, token, error); },
            [&](std::string& error) { return m_componentClipboard.PasteManaged(entity, token, error); });
#endif
    }
}
