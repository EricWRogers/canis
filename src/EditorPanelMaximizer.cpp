#include <Canis/EditorPanelMaximizer.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
namespace Canis
{
    void EditorPanelMaximizer::Restore()
    {
        if (m_window.empty() || !ImGui::GetCurrentContext()) return;
        const auto name = m_window;
        m_window.clear();
        ImGui::LoadIniSettingsFromMemory(m_layout.data(), m_layout.size());
        ImGui::GetIO().IniFilename = m_iniFilename;
        m_iniFilename = nullptr;
        m_layout.clear();
        m_restoreRequested = false;
        ImGui::MarkIniSettingsDirty();
        if (auto* window = ImGui::FindWindowByName(name.c_str())) ImGui::FocusWindow(window);
    }

    void EditorPanelMaximizer::Update(float x, float y, float width, float height, unsigned int viewport)
    {
        const auto mouse = ImGui::GetIO().MousePos;
        Update(x,y,width,height,viewport,ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) ||
            ImGui::IsMouseClicked(ImGuiMouseButton_Right),mouse.x,mouse.y);
    }

    void EditorPanelMaximizer::Update(float x, float y, float width, float height, unsigned int viewport,
        bool toggleRequested, float mouseX, float mouseY)
    {
        m_x = x; m_y = y; m_width = std::max(1.0f, width); m_height = std::max(1.0f, height); m_viewport = viewport;
        if (m_restoreRequested) Restore();
        auto& context = *ImGui::GetCurrentContext();
        if (!toggleRequested || !context.OpenPopupStack.empty()) return;
        const ImVec2 mouse(mouseX,mouseY);
        for (int i = context.Windows.Size - 1; i >= 0; --i)
        {
            auto* window = context.Windows[i];
            if (!window->WasActive || !m_panels.count(window->ID)) continue;
            ImRect title;
            if (window->DockNode && window->DockNode->TabBar)
            {
                if (context.HoveredWindow != window->DockNode->HostWindow && context.HoveredWindow != window) continue;
                if (!window->DockNode->TabBar->BarRect.Contains(mouse)) continue;
                title = window->DC.DockTabItemRect;
                // Dock tab close buttons are font-sized, not frame-height-sized.
                // Match TabItemLabelAndCloseButton so the label's trailing area
                // remains a valid maximize target on short tabs such as Game.
                if (window->HasCloseButton) {
                    const auto* tabBar = window->DockNode->TabBar;
                    const float buttonSize = std::max(0.0f, tabBar->BarRect.GetHeight() - 2.0f * tabBar->FramePadding.y);
                    title.Max.x -= tabBar->FramePadding.x + buttonSize;
                }
            }
            else
            {
                if (context.HoveredWindow != window || (window->Flags & ImGuiWindowFlags_NoTitleBar)) continue;
                title = window->TitleBarRect();
                // Keep the collapse/menu button independent of title double-clicks.
                if (!(window->Flags & ImGuiWindowFlags_NoCollapse)) title.Min.x += window->TitleBarHeight;
                if (window->HasCloseButton) title.Max.x -= window->TitleBarHeight;
            }
            if (title.GetWidth() <= 0 || !title.Contains(mouse)) continue;
            // Consume the title gesture before ImGui can collapse/move the panel.
            ImGui::SetKeyOwner(ImGuiKey_MouseLeft, window->ID);
            ImGui::SetKeyOwner(ImGuiKey_MouseRight, window->ID, ImGuiInputFlags_LockThisFrame);
            ImGui::ClearActiveID();
            context.MovingWindow = nullptr;
            if (!m_window.empty()) { Restore(); return; }
            size_t size = 0;
            const char* layout = ImGui::SaveIniSettingsToMemory(&size);
            m_layout.assign(layout, size);
            m_iniFilename = ImGui::GetIO().IniFilename;
            // Persist the real layout; temporary undocking must never replace it.
            if (m_iniFilename) ImGui::SaveIniSettingsToDisk(m_iniFilename);
            ImGui::GetIO().IniFilename = nullptr;
            m_window = window->Name;
            ImGui::DockContextProcessUndockWindow(&context, window);
            ImGui::SetWindowCollapsed(window, false, ImGuiCond_Always);
            ImGui::FocusWindow(window);
            return;
        }
    }

    bool EditorPanelMaximizer::Begin(const char* name, bool* open, int flags)
    {
        const bool maximized = m_window == name;
        if (maximized)
        {
            ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
            ImGui::SetNextWindowViewport(m_viewport);
            ImGui::SetNextWindowPos(ImVec2(m_x, m_y), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(m_width, m_height), ImGuiCond_Always);
            ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
            flags |= ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking;
            m_submittedFrame = ImGui::GetFrameCount();
        }
        const bool visible = ImGui::Begin(name, open, flags);
        m_panels.insert(ImGui::GetCurrentWindow()->ID);
        if (maximized && open && !*open) m_restoreRequested = true;
        return visible;
    }

    void EditorPanelMaximizer::Finish()
    {
        if (m_window.empty()) return;
        if (m_submittedFrame != ImGui::GetFrameCount()) { m_restoreRequested = true; return; }
        if (auto* window = ImGui::FindWindowByName(m_window.c_str()))
            if (ImGui::GetCurrentContext()->OpenPopupStack.empty()) ImGui::BringWindowToDisplayFront(window);
    }
}
