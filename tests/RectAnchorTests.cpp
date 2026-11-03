#include <Canis/RectLayoutEditor.hpp>
#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
#if CANIS_EDITOR
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <Canis/OpenGL.hpp>
#include <Canis/Window.hpp>
#include <stb_image_write.h>
#include <memory>
#include <vector>
#include <algorithm>
#endif

using namespace Canis;
namespace
{
    void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
    bool Near(Vector2 a, Vector2 b) { return glm::length(a - b) < .003f; }
    Vector2 Corner(const RectLayout& rect)
    {
        return Vector2(rect.GetCanvasLocalMatrix() * Vector4(-rect.pivot * rect.GetLogicalSize(), 0, 1));
    }
    struct Fixture
    {
        Scene scene;
        Entity canvas = scene.CreateEntity("World canvas"), root = scene.CreateEntity("Parent"), child = scene.CreateEntity("Child");
        RectLayout& parent = *root.AddComponent<RectLayout>();
        RectLayout& rect = *child.AddComponent<RectLayout>();
        Fixture()
        {
            canvas.AddComponent<Transform>();
            canvas.AddComponent<Canvas>()->renderMode = CanvasRenderMode::WORLD_SPACE;
            parent.Spatial().parent = canvas;
            parent.SetLocalPosition({100, 50}); parent.size = {640, 480}; parent.SetLocalScale({1.5f, .75f}); parent.SetLocalRotation(.3f);
            rect.Spatial().parent = root; rect.SetLocalPosition({23, -41}); rect.size = {88, 64}; rect.SetLocalScale({.8f, 1.2f});
            rect.pivot = {.2f, .7f}; rect.anchorMin = {.15f, .25f}; rect.anchorMax = {.8f, .75f}; rect.SetLocalRotation(-.6f);
        }
    };
    void PreservesBounds()
    {
        for (int x = 1; x <= 4; ++x) for (int y = 1; y <= 4; ++y) for (bool pivot : {false, true})
        {
            Fixture f; auto& r = f.rect;
            const auto size = r.GetResolvedSize(), corner = Corner(r), oldPivot = r.pivot;
            ApplyRectAnchorPreset(r, RectAnchorAxis(x), RectAnchorAxis(y), pivot);
            Check(Near(size, r.GetResolvedSize()) && Near(corner, Corner(r)), "Anchor preset moved or resized the rendered rectangle");
            if (!pivot) Check(r.pivot == oldPivot, "Plain click changed pivot");
            else Check(r.pivot == (r.anchorMin + r.anchorMax) * .5f, "Shift did not match pivot to preset");
        }
    }
    void StretchAndSingleAxis()
    {
        Fixture f; auto& r = f.rect;
        r.SetLocalRotation(0);
        ApplyRectAnchorPreset(r, RectAnchorAxis::Stretch, RectAnchorAxis::Max, true, true);
        Check(r.anchorMin == Vector2(0, 1) && r.anchorMax == Vector2(1, 1) && r.pivot == Vector2(.5f, 1), "Stretch/top anchor coordinates incorrect");
        Check(r.GetLocalPosition() == Vector2(0) && r.size.x == 0, "Alt did not reset selected position and stretch offsets");
        const auto oldSize = r.GetResolvedSize(); f.parent.size.x += 100;
        Check(std::abs(r.GetResolvedSize().x - oldSize.x - 100 * 1.5f * .8f) < .003f, "Stretched rect does not follow parent resize");
        const auto oldYMin = r.anchorMin.y, oldYMax = r.anchorMax.y, oldY = r.Spatial().position.y, oldHeight = r.size.y;
        ApplyRectAnchorPreset(r, RectAnchorAxis::Center, RectAnchorAxis::Custom, false, true);
        Check(r.anchorMin.x == .5f && r.anchorMax.x == .5f && r.Spatial().position.x == 0, "Single axis preset did not apply");
        Check(r.anchorMin.y == oldYMin && r.anchorMax.y == oldYMax && std::abs(r.Spatial().position.y - oldY) < .003f && std::abs(r.size.y - oldHeight) < .003f, "Single axis preset changed untouched axis");
        r.Spatial().scale.x = 0;
        ApplyRectAnchorPreset(r, RectAnchorAxis::Stretch, RectAnchorAxis::Stretch);
        Check(std::isfinite(r.size.x) && std::isfinite(r.Spatial().position.x), "Collapsed axis created nonfinite offsets");
    }
#if CANIS_EDITOR
    void PopupDraws(const char* capture)
    {
        constexpr int screenWidth = 620, screenHeight = 620;
        std::unique_ptr<Window> window;
        if (capture) window = std::make_unique<Window>("Anchor Presets review", screenWidth, screenHeight, true);
        ImGui::CreateContext(); auto& io = ImGui::GetIO();
        io.IniFilename = nullptr; io.DisplaySize = {screenWidth, screenHeight}; io.DeltaTime = 1.f / 60;
        if (capture)
        {
            io.Fonts->AddFontFromFileTTF("assets/fonts/CascadiaMono/CaskaydiaMonoNerdFont-Regular.ttf", 16);
            Check(ImGui_ImplOpenGL3_Init("#version 330 core"), "Could not initialize preview renderer");
        }
        unsigned char* pixels; int width, height; io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        ImGui::StyleColorsDark();
        Fixture f; f.rect.anchorMin = f.rect.anchorMax = f.rect.pivot = Vector2(.5f);
        ImVec2 stretchButton;
        bool openPopup = true, popupOpen = false;
        auto frame = [&](ImVec2 mouse = {-100, -100}, bool down = false)
        {
            if (capture) ImGui_ImplOpenGL3_NewFrame();
            io.AddMousePosEvent(mouse.x, mouse.y); io.AddMouseButtonEvent(0, down);
            ImGui::NewFrame(); ImGui::SetNextWindowPos({16, 16}); ImGui::SetNextWindowSize({580, 580});
            ImGui::Begin("Rect Transform", nullptr, ImGuiWindowFlags_NoSavedSettings);
            if (openPopup)
            {
                ImGui::PushID(&f.rect); ImGui::OpenPopup("Anchor Presets"); ImGui::PopID();
                ImGui::SetNextWindowPos({60, 140});
            }
            DrawRectLayoutAnchorPresets(f.rect);
            ImGui::PushID(&f.rect); popupOpen = ImGui::IsPopupOpen("Anchor Presets"); ImGui::PopID();
            if (popupOpen)
            {
                auto* popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
                auto* table = ImGui::TableFindByID(popup->GetID("Presets"));
                Check(table != nullptr, "Preset grid table missing");
                stretchButton = {(table->Columns[5].WorkMinX + table->Columns[5].WorkMaxX) * .5f,
                                 (table->RowPosY1 + table->RowPosY2) * .5f};
            }
            ImGui::End(); ImGui::Render();
        };
        frame(); frame(); frame();
        Check(popupOpen && ImGui::GetDrawData()->TotalVtxCount > 100, "Visual preset grid did not draw");
        if (capture)
        {
            glViewport(0, 0, screenWidth, screenHeight); glClearColor(.035f, .045f, .06f, 1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            std::vector<unsigned char> image(screenWidth * screenHeight * 4), flipped(image.size());
            glReadPixels(0, 0, screenWidth, screenHeight, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
            for (int y = 0; y < screenHeight; ++y)
                std::copy_n(image.data() + (screenHeight - 1 - y) * screenWidth * 4, screenWidth * 4, flipped.data() + y * screenWidth * 4);
            Check(stbi_write_png(capture, screenWidth, screenHeight, 4, flipped.data(), screenWidth * 4) != 0, "Could not capture anchor popup");
        }
        openPopup = false;
        io.AddKeyEvent(ImGuiMod_Shift, true); io.AddKeyEvent(ImGuiMod_Alt, true);
        const auto point = stretchButton; frame(point); frame(point, true); frame(point, false);
        Check(f.rect.anchorMin == Vector2(0) && f.rect.anchorMax == Vector2(1) &&
              f.rect.GetLocalPosition() == Vector2(0) && f.rect.size == Vector2(0) && f.rect.pivot == Vector2(.5f) && !popupOpen,
              "Clicking stretch with Shift+Alt did not fill parent and close popup");
        if (capture) ImGui_ImplOpenGL3_Shutdown();
        ImGui::DestroyContext();
    }
#endif
}
int main(int argc, char** argv)
{
    try
    {
        PreservesBounds(); StretchAndSingleAxis();
#if CANIS_EDITOR
        PopupDraws(argc > 1 ? argv[1] : nullptr);
#endif
        std::cout << "Rect anchor preset checks passed\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
