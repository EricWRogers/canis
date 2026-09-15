#include <Canis/EditorPanelMaximizer.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <chrono>
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    const auto ini = (std::filesystem::temp_directory_path() / ("canis-maximize-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".ini")).string();
    io.IniFilename = ini.c_str();
    auto readIni = [&] { std::ifstream file(ini); return std::string(std::istreambuf_iterator<char>(file), {}); };
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = ImVec2(1000, 700); io.DeltaTime = 1.0f / 60;
    unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    Canis::EditorPanelMaximizer panels;
    ImGuiID dock = 0;
    bool showScene = true;
    auto frame = [&](ImVec2 mouse = ImVec2(-100, -100), bool down = false)
    {
        io.AddMousePosEvent(mouse.x, mouse.y); io.AddMouseButtonEvent(0, down);
        ImGui::NewFrame();
        panels.Update(0, 40, io.DisplaySize.x, io.DisplaySize.y - 40, ImGui::GetMainViewport()->ID);
        ImGui::SetNextWindowPos(ImVec2(0, 40)); ImGui::SetNextWindowSize(ImVec2(1000, 660));
        ImGui::Begin("DockHost", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus);
        dock = ImGui::GetID("Dock"); ImGui::DockSpace(dock); ImGui::End();
        if (showScene) { panels.Begin("Scene", &showScene); ImGui::TextUnformatted("scene contents"); ImGui::End(); }
        panels.Begin("Game"); ImGui::End();
        panels.Begin("Inspector"); ImGui::End();
        ImGui::SetNextWindowPos(ImVec2(400, 150), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 250), ImGuiCond_FirstUseEver);
        panels.Begin("Script Editor"); ImGui::End();
        panels.Finish(); ImGui::Render();
    };
    auto clickTitle = [&](const char* name)
    {
        for (int i = 0; i < 25; ++i) frame();
        auto* window = ImGui::FindWindowByName(name);
        if (!window->DockNode) ImGui::FocusWindow(window);
        auto rect = window->DockNode ? window->DC.DockTabItemRect : window->TitleBarRect();
        ImVec2 point(rect.Min.x + 35, (rect.Min.y + rect.Max.y) / 2);
        frame(point); frame(point, true); frame(point); frame(point, true); frame(point); frame();
    };
    try
    {
        frame();
        ImGui::NewFrame(); ImGui::Begin("DockHost");
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, ImVec2(1000, 660));
        ImGuiID left, right;
        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.3f, &left, &right);
        ImGui::DockBuilderDockWindow("Inspector", left);
        ImGui::DockBuilderDockWindow("Scene", right);
        ImGui::DockBuilderDockWindow("Game", right);
        ImGui::DockBuilderFinish(dock);
        ImGui::End(); ImGui::Render();
        frame(); frame(); frame();
        clickTitle("Game");
        Check(panels.Maximized() && panels.Window() == "Game", "dock tab did not maximize");
        auto* game = ImGui::FindWindowByName("Game");
        Check(game->DockId == 0 && game->Pos.y == 40 && game->Size.x == 1000 && game->Size.y == 660, "wrong maximized bounds");
        const auto savedLayout = readIni();
        Check(!savedLayout.empty() && io.IniFilename == nullptr, "maximization must preserve saved layout");
        for (int i = 0; i < 400; ++i) frame();
        Check(readIni() == savedLayout, "automatic save overwrote layout while maximized");
        io.DisplaySize = ImVec2(1100, 800); frame();
        Check(game->Size.x == 1100 && game->Size.y == 760, "maximized panel must follow resizing");
        clickTitle("Game");
        Check(!panels.Maximized(), "second double-click did not restore");
        Check(io.IniFilename == ini.c_str(), "layout saving must resume after restoring");
        Check(ImGui::FindWindowByName("Game")->DockId == right && ImGui::FindWindowByName("Scene")->DockId == right &&
              ImGui::FindWindowByName("Inspector")->DockId == left, "dock layout not restored");
        auto* script = ImGui::FindWindowByName("Script Editor");
        ImGui::FocusWindow(script);
        const ImVec2 content(script->Pos.x + 100, script->Pos.y + 100);
        for (int i = 0; i < 25; ++i) frame();
        frame(content, true); frame(content); frame(content, true); frame(content);
        Check(!panels.Maximized(), "double-clicking content must not maximize");
        const auto position = script->Pos, size = script->Size;
        clickTitle("Script Editor"); Check(panels.Maximized(), "floating title did not maximize");
        clickTitle("Script Editor"); Check(!panels.Maximized(), "floating title did not restore");
        Check(script->Pos.x == position.x && script->Pos.y == position.y && script->Size.x == size.x && script->Size.y == size.y,
              "floating geometry not restored");
        clickTitle("Scene"); Check(panels.Maximized(), "inactive dock tab did not maximize");
        showScene = false; frame(); frame();
        Check(!panels.Maximized(), "hiding maximized panel did not restore layout");
        std::cout << "Panel maximize/restore, dock layout, floating geometry, resize and hidden-panel checks passed\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; panels.Restore(); ImGui::DestroyContext(); std::filesystem::remove(ini); return 1; }
    ImGui::DestroyContext();
    std::filesystem::remove(ini);
}
