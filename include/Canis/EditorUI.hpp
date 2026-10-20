#pragma once

#include <algorithm>
#include <imgui.h>

namespace Canis::EditorUI
{
    inline constexpr float BodySize = 14.0f;
    inline constexpr float DetailSize = 13.0f;
    inline constexpr float SectionSize = 18.0f;
    inline constexpr float TitleSize = 20.0f;

    // Nominal sizes follow the editor's selected font and accessibility scale.
    struct ScopedFont
    {
        explicit ScopedFont(float size)
        {
            const ImFont* font = ImGui::GetIO().FontDefault;
            const float base = font ? font->LegacySize : 16.0f;
            ImGui::PushFont(nullptr, base * (size / 16.0f));
        }
        ~ScopedFont() { Reset(); }
        void Reset()
        {
            if (active) ImGui::PopFont();
            active = false;
        }
        ScopedFont(const ScopedFont&) = delete;
        ScopedFont& operator=(const ScopedFont&) = delete;
    private:
        bool active = true;
    };

    inline float ControlWidth(float trailingWidth = 0.0f)
    {
        return std::max(1.0f, ImGui::GetContentRegionAvail().x - trailingWidth);
    }

    inline void PropertyWidth(float trailingWidth = 0.0f)
    {
        ImGui::SetNextItemWidth(ControlWidth(trailingWidth));
    }

    inline float TrailingButtonWidth(const char* label)
    {
        return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f +
               ImGui::GetStyle().ItemSpacing.x;
    }

    inline float LabelWidth()
    {
        const float available = ImGui::GetContentRegionAvail().x;
        return std::max(1.0f, std::min(ImGui::GetFontSize() * 12.0f, available * 0.42f));
    }

    inline void PropertyLabel(const char* label)
    {
        label = label ? label : "";
        const float width = LabelWidth();
        const auto position = ImGui::GetCursorScreenPos();
        const auto& style = ImGui::GetStyle();
        const float textWidth = std::max(1.0f, width - style.ItemInnerSpacing.x);
        const ImVec4 clip(position.x, position.y, position.x + textWidth,
                          position.y + ImGui::GetFrameHeight());
        // A clipped label cannot push the value column out of a narrow panel.
        ImGui::Dummy(ImVec2(width, ImGui::GetFrameHeight()));
        ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
            ImVec2(position.x, position.y + style.FramePadding.y),
            ImGui::GetColorU32(ImGuiCol_Text), label, nullptr, 0.0f, &clip);
        if (ImGui::IsItemHovered() && ImGui::CalcTextSize(label).x > textWidth)
            ImGui::SetTooltip("%s", label);
        ImGui::SameLine(0.0f, style.ItemSpacing.x);
        PropertyWidth();
    }

    inline bool SectionHeader(const char* label, ImGuiTreeNodeFlags flags = 0)
    {
        ScopedFont heading(SectionSize);
        return ImGui::CollapsingHeader(label, flags);
    }
}
