#pragma once
#include <string>
#include <unordered_set>
namespace Canis
{
    // Uses the editor's vendored ImGui docking API; panel contents stay live.
    class EditorPanelMaximizer
    {
    public:
        void Update(float x, float y, float width, float height, unsigned int viewport);
        void Update(float x, float y, float width, float height, unsigned int viewport,
            bool toggleRequested, float mouseX, float mouseY);
        bool Begin(const char* name, bool* open = nullptr, int flags = 0);
        void Finish();
        void Restore();
        bool Maximized() const { return !m_window.empty(); }
        const std::string& Window() const { return m_window; }
        bool ShouldRender(const char* name, bool open) const;
    private:
        std::unordered_set<unsigned int> m_panels;
        std::string m_window, m_layout;
        const char* m_iniFilename = nullptr;
        float m_x = 0, m_y = 0, m_width = 0, m_height = 0;
        unsigned int m_viewport = 0;
        int m_submittedFrame = -1;
        bool m_restoreRequested = false;
    };
}
