#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

namespace Canis
{
    inline void PrepareViewportGizmo(float x, float y, float width, float height)
    {
        // Keep ImGuizmo's internal helper window attached to the Scene viewport.
        // This preserves gizmo interaction while preventing it from showing up as its own platform window.
        ImGuiWindow* sceneWindow = ImGui::GetCurrentWindow();
        if (sceneWindow != nullptr && sceneWindow->Viewport != nullptr)
            ImGui::SetNextWindowViewport(sceneWindow->Viewport->ID);

        ImGuiWindowClass gizmoWindowClass = {};
        gizmoWindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoTaskBarIcon;
        gizmoWindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&gizmoWindowClass);
        ImGuizmo::BeginFrame();

        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
        ImGuizmo::SetRect(x, y, width, height);
        ImGuizmo::Enable(true);

    }
}
