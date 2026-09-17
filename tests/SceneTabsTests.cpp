#include <Canis/App.hpp>
#include <Canis/Editor.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/Window.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
static void Check(bool value, const char* message)
{ if (!value) throw std::runtime_error(message); }

namespace Canis
{
    struct EditorSceneTabsTestAccess
    {
        static void Run(App& app, Editor& editor, Window& window)
        {
            InputManager input;
            app.scene.Init(&app, &window, &input);
            editor.m_scene = &app.scene;
            editor.m_window = &window;
            auto seed = [&](const char* path) {
                std::ofstream(path) << "Entities:\n- Entity: 101\n  Name: Original\n";
            };
            auto setup = [&] {
                seed("first.scene"); seed("second.scene");
                app.scene.Unload(); app.scene.Load("first.scene");
                editor.ResetSceneHistory(); editor.InitializeSceneTabs();
                editor.OpenSceneTab("second.scene");
            };
            auto dirtyFirst = [&] {
                editor.SwitchSceneTab(0);
                app.scene.GetEntities()[0]->SetName("Edited first");
                editor.ResetSceneHistory();
                editor.SwitchSceneTab(1);
            };
            auto diskName = [&] { return YAML::LoadFile("first.scene")["Entities"][0]["Name"].as<std::string>(); };
            auto& io = ImGui::GetIO();
            bool collapsed=false;
            auto frame = [&] {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0,0));
                ImGui::SetNextWindowSize(ImVec2(800,600));
                ImGui::SetNextWindowCollapsed(collapsed,ImGuiCond_Always);
                ImGui::Begin("SceneTabsTest"); editor.DrawSceneTabs(); ImGui::End();
                ImGui::Render();
            };

            setup();
            editor.SwitchSceneTab(0);
            for (int i=0;i<4;++i) frame();
            collapsed=true; frame();
            editor.OpenSceneTab("second.scene");
            for (int i=0;i<3;++i) frame();
            Check(editor.m_sceneTabSelectionRequest==1,"Hidden Scene panel discarded pending scene selection");
            collapsed=false;
            for (int i=0;i<4;++i) frame();
            Check(editor.m_activeSceneTab==1,"Showing Scene panel switched back to previous scene");
            editor.CloseSceneTab(0);
            Check(editor.m_sceneTabs.size()==1 && editor.m_activeSceneTab==0,
                  "Closing clean first tab corrupted the active index");
            setup(); dirtyFirst();
            for (int i=0; i<4; ++i) frame();
            auto* host=ImGui::FindWindowByName("SceneTabsTest");
            auto* bar=ImGui::GetCurrentContext()->TabBars.GetByKey(host->GetID("##SceneAssetTabs"));
            Check(bar && bar->Tabs.Size>=2, "Scene tab bar missing");
            const auto& tab=bar->Tabs[0];
            const ImVec2 close(bar->BarRect.Min.x+tab.Offset+tab.Width-ImGui::GetStyle().FramePadding.x-ImGui::GetFontSize()*.5f,
                               (bar->BarRect.Min.y+bar->BarRect.Max.y)*.5f);
            io.AddMousePosEvent(close.x,close.y); frame();
            io.AddMouseButtonEvent(0,true); frame();
            io.AddMouseButtonEvent(0,false); frame(); frame();
            Check(editor.m_pendingSceneTabClose==0 && editor.m_sceneTabs.size()==2,
                  "Dirty first-tab X did not open confirmation");
            Check(editor.m_activeSceneTab==1, "Closing inactive tab changed the active scene");
            auto* modal=ImGui::FindWindowByName("Unsaved Scene Changes");
            Check(modal && modal->WasActive, "Unsaved scene modal not visible");
            io.AddKeyEvent(ImGuiKey_Escape,true); frame();
            io.AddKeyEvent(ImGuiKey_Escape,false); frame();
            Check(editor.m_pendingSceneTabClose==-1 && editor.IsSceneTabDirty(0),
                  "Cancel lost edits or kept the close request");
            editor.CloseSceneTab(0);
            Check(editor.ResolveSceneTabClose(false), "Discard failed");
            Check(editor.m_sceneTabs.size()==1 && diskName()=="Original", "Discard saved changes to disk");

            setup(); dirtyFirst();
            editor.CloseSceneTab(0);
            Check(editor.ResolveSceneTabClose(true), "Save and close failed");
            Check(editor.m_sceneTabs.size()==1 && editor.m_activeSceneTab==0 &&
                  editor.m_sceneTabs[0].path=="second.scene" && diskName()=="Edited first",
                  "Saving inactive tab saved or closed the wrong scene");

            setup(); dirtyFirst();
            fs::remove("first.scene"); fs::create_directory("first.scene");
            editor.CloseSceneTab(0);
            Check(!editor.ResolveSceneTabClose(true), "Write failure reported success");
            Check(editor.m_sceneTabs.size()==2 && editor.IsSceneTabDirty(0) &&
                  editor.m_activeSceneTab==1 && !editor.m_sceneTabCloseError.empty(),
                  "Write failure closed the tab or lost edits");
            fs::remove("first.scene");
            editor.ResolveSceneTabClose(false);

            setup();
            app.scene.GetEntities()[0]->SetName("Edited second"); editor.ResetSceneHistory();
            editor.CloseSceneTab(1);
            Check(editor.ResolveSceneTabClose(false) && editor.m_activeSceneTab==0 &&
                  app.scene.GetPath()=="first.scene", "Closing active dirty tab did not restore the other scene");
            editor.CloseSceneTab(0);
            Check(editor.m_sceneTabs.size()==1, "Last scene tab unexpectedly closed");
            editor.m_scene = nullptr;
        }
    };
}

int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    const auto original=fs::current_path();
    auto temp=fs::temp_directory_path()/("canis-scene-tabs-"+std::to_string(SDL_GetTicksNS()));
    fs::create_directories(temp); fs::current_path(temp);
    try {
        fs::create_directories("assets");
        fs::copy(argv[1], "assets/shaders", fs::copy_options::recursive);
        Canis::Window window("Scene tab test",800,600,true);
        ImGui::CreateContext();
        auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.DisplaySize=ImVec2(800,600); io.DeltaTime=1.f/60;
        unsigned char* pixels; int width,height; io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        {
            Canis::App app; Canis::Editor editor;
            Canis::EditorSceneTabsTestAccess::Run(app,editor,window);
        }
        ImGui::DestroyContext();
        fs::current_path(original); fs::remove_all(temp);
        std::cout << "Scene tab X, cancel, discard, save, failed save and active-tab checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        fs::current_path(original);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
