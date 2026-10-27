#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/Editor.hpp>
#include <Canis/EditorUI.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Scene.hpp>
#include <Canis/SpriteSheet.hpp>
#include <Canis/TextureImport.hpp>
#include <Canis/Window.hpp>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace
{
    constexpr int ScreenWidth = 1920, ScreenHeight = 1200;
    const std::string TexturePath = "assets/ui-atlas.png";

    void Check(bool condition, const std::string &message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    std::string Contents(const std::string &path)
    {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), {}};
    }

    void WriteFixture()
    {
        fs::create_directories("assets");
        constexpr int width = 256, height = 128;
        std::vector<unsigned char> pixels(width * height * 4, 0);
        for (int y = 8; y < 56; ++y)
            for (int x = 8; x < 104; ++x)
            {
                const auto index = (y * width + x) * 4;
                pixels[index] = (x / 8 + y / 8) % 2 ? 233 : 200;
                pixels[index + 1] = 65; pixels[index + 2] = 95; pixels[index + 3] = 255;
            }
        for (int y = 70; y < 110; ++y)
            for (int x = 132; x < 232; ++x)
            {
                const auto index = (y * width + x) * 4;
                pixels[index] = 65; pixels[index + 1] = 130;
                pixels[index + 2] = (x / 8 + y / 8) % 2 ? 245 : 200; pixels[index + 3] = 255;
            }
        Check(stbi_write_png(TexturePath.c_str(), width, height, 4, pixels.data(), width * 4) != 0,
              "Could not write the temporary UI atlas fixture");
        Check(Canis::AssetManager::GetMetaFile(TexturePath) != nullptr, "Texture fixture metadata was not created");
        Canis::SpriteSheet sheet{width, height, {
            {"Health bar", 8, 8, 96, 48, Canis::Vector2(.25f, .75f)},
            {"Shield bar", 132, 70, 100, 40, Canis::Vector2(.75f, .25f)}
        }};
        std::string error;
        Check(Canis::SaveSpriteSheet(TexturePath, sheet, &error), "Could not save sprite fixture: " + error);
    }

    void Screenshot(const fs::path &path)
    {
        std::vector<unsigned char> pixels(size_t(ScreenWidth) * ScreenHeight * 4);
        glReadPixels(0, 0, ScreenWidth, ScreenHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        std::vector<unsigned char> flipped(pixels.size());
        for (int y = 0; y < ScreenHeight; ++y)
            std::copy_n(pixels.data() + size_t(ScreenHeight - 1 - y) * ScreenWidth * 4, ScreenWidth * 4,
                        flipped.data() + size_t(y) * ScreenWidth * 4);
        Check(stbi_write_png(path.string().c_str(), ScreenWidth, ScreenHeight, 4,
                             flipped.data(), ScreenWidth * 4) != 0,
              "Could not write editor review screenshot");
    }
}

namespace Canis
{
    // Reuse the existing test-only friend rather than adding production hooks
    // or reaching into the draft/state implementations under test.
    struct EditorSceneTabsTestAccess
    {
        static void Run(App &app, Editor &editor, Window &window, const fs::path &artifacts)
        {
            InputManager input;
            app.scene.Init(&app, &window, &input);
            editor.m_scene = &app.scene;
            editor.m_app = &app;
            editor.m_window = &window;
            editor.m_mode = EditorMode::EDIT;
            auto entity = app.scene.CreateEntity("HUD fixture");
            entity.AddComponent<Transform>();
            auto &sprite = *entity.AddComponent<Sprite2D>();
            auto &rect = *entity.AddComponent<RectLayout>();
            const Vector4 originalUV(.1f, .2f, .3f, .4f);
            const Vector2 originalPivot(.5f), originalSize(320.f, 64.f);
            sprite.textureHandle = TextureHandle{-1, {}};
            sprite.uv = originalUV; sprite.flipX = sprite.flipY = true;
            rect.pivot = originalPivot; rect.size = originalSize;
            editor.FocusEntity(entity.TryGet());

            const auto original = Contents(TexturePath + ".meta");
            const auto originalTime = fs::last_write_time(TexturePath + ".meta");
            const auto originalUUID = AssetManager::GetMetaFile(TexturePath)->uuid;
            const auto originalImport = LoadTextureImportSettings(TexturePath);
            auto *texture = AssetManager::GetTexture(TexturePath);
            Check(texture && texture->GetGLTexture().id != 0, "Could not upload UI atlas fixture");
            const auto originalGL = texture->GetGLTexture().id;

            bool showSprite = false, showAssets = true;
            editor.m_assetSearch = "ui-atlas";
            auto frame = [&](ImVec2 mouse = ImVec2(-100, -100), bool down = false,
                             const fs::path &capture = fs::path{}) -> ImGuiID
            {
                ImGui_ImplOpenGL3_NewFrame();
                auto &io = ImGui::GetIO();
                io.AddMousePosEvent(mouse.x, mouse.y);
                io.AddMouseButtonEvent(ImGuiMouseButton_Left, down);
                ImGui::NewFrame();
                if (showAssets)
                {
                    ImGui::SetNextWindowPos(ImVec2(640, 0), ImGuiCond_Always);
                    ImGui::SetNextWindowSize(ImVec2(1280, 400), ImGuiCond_Always);
                    editor.DrawAssetsPanel();
                }
                ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(620, ScreenHeight), ImGuiCond_Always);
                // Exercise the same selection-dependent routing as the editor,
                // rather than directly invoking the texture-specific drawer.
                editor.DrawInspectorPanel(false);
                if (showSprite)
                {
                    // DrawSpriteEditor supplies its own first-use size hint.
                    // Resize the existing window rather than relying on next-
                    // window data which that production hint may replace.
                    if (ImGui::FindWindowByName("Sprite Editor###SpriteEditor"))
                    {
                        ImGui::SetWindowPos("Sprite Editor###SpriteEditor", ImVec2(640, 0), ImGuiCond_Always);
                        ImGui::SetWindowSize("Sprite Editor###SpriteEditor", ImVec2(1280, 1150), ImGuiCond_Always);
                    }
                    ImGui::SetNextWindowPos(ImVec2(640, 0), ImGuiCond_Always);
                    editor.DrawSpriteEditor();
                }
                const auto hovered = ImGui::GetCurrentContext()->HoveredId;
                ImGui::Render();
                Check(ImGui::GetCurrentContext()->ErrorCountCurrentFrame == 0,
                      "Texture/Sprite editor violated ImGui window or font scopes");
                if (!capture.empty())
                {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glViewport(0, 0, ScreenWidth, ScreenHeight);
                    glClearColor(.05f, .065f, .08f, 1.f); glClear(GL_COLOR_BUFFER_BIT);
                    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
                    glFinish(); Screenshot(capture);
                    Check(glGetError() == GL_NO_ERROR, "Editor screenshot rendering generated an OpenGL error");
                }
                return hovered;
            };

            auto inspectorID = [&](const char *label)
            {
                auto *panel = ImGui::FindWindowByName("Inspector");
                Check(panel != nullptr, "Inspector window was not created");
                const auto seed = ImHashStr(TexturePath.c_str(), 0, panel->ID);
                return ImHashStr(label, 0, seed);
            };
            auto spritePanel = [&](bool properties = false)
            {
                ImGuiWindow *panel = nullptr;
                if (!properties) panel = ImGui::FindWindowByName("Sprite Editor###SpriteEditor");
                else for (auto *candidate : ImGui::GetCurrentContext()->Windows)
                    if (std::string(candidate->Name).find("##SpriteProperties") != std::string::npos)
                    { panel = candidate; break; }
                Check(panel != nullptr, "Sprite Editor properties window was not created");
                return panel;
            };
            auto spriteID = [&](const char *label, bool properties = false)
            {
                return ImHashStr(label, 0, spritePanel(properties)->ID);
            };
            auto locate = [&](const char *label, ImGuiID id, float x,
                              float from = 25.f, float to = ScreenHeight - 20.f)
            {
                for (float y = from; y < to; y += 4.f)
                    if (frame(ImVec2(x, y)) == id) return ImVec2(x, y);
                const auto capture = artifacts / ("missing-control-" + std::to_string(id) + ".png");
                frame(ImVec2(-100, -100), false, capture);
                std::cerr << "Could not locate '" << label << "' ID=" << id
                          << " at X=" << x << " in Y=[" << from << ',' << to << "]\n"
                          << "Failure screenshot: " << capture << '\n';
                for (auto *panel : ImGui::GetCurrentContext()->Windows)
                    if (panel->Active)
                        std::cerr << "  Window '" << panel->Name << "' ID=" << panel->ID
                                  << " pos=" << panel->Pos.x << ',' << panel->Pos.y
                                  << " size=" << panel->Size.x << ',' << panel->Size.y
                                  << " clip=" << panel->ClipRect.Min.x << ',' << panel->ClipRect.Min.y
                                  << ".." << panel->ClipRect.Max.x << ',' << panel->ClipRect.Max.y
                                  << " scroll=" << panel->Scroll.x << ',' << panel->Scroll.y
                                  << " content=" << panel->ContentSize.x << ',' << panel->ContentSize.y << '\n';
                throw std::runtime_error("UI control '" + std::string(label) +
                                         "' was not exposed or clickable; ID=" + std::to_string(id));
            };
            auto click = [&](ImVec2 position)
            {
                frame(position); frame(position, true); frame(position, false); frame();
            };

            frame(); frame();
            auto *assets = ImGui::FindWindowByName("Assets");
            Check(assets != nullptr, "Actual Assets panel was not drawn");
            const auto cardSeed = ImHashStr(TexturePath.c_str(), 0, assets->ID);
            const auto cardID = ImHashStr("##AssetCard", 0, cardSeed);
            const auto card = locate("Texture thumbnail in Assets", cardID, assets->Pos.x + 30.f, 70.f, 350.f);
            Check(editor.m_selectedAssetPath.empty(), "Texture was selected without clicking its asset card");
            click(card);
            Check(editor.m_selectedAssetPath == TexturePath,
                  "Clicking a texture thumbnail in the actual Assets panel did not select it for the Inspector");
            frame(ImVec2(-100, -100), false, artifacts / "texture-inspector-defaults.png");
            Check(Contents(TexturePath + ".meta") == original &&
                  fs::last_write_time(TexturePath + ".meta") == originalTime,
                  "Selecting a texture in the Inspector rewrote its metadata");
            showAssets = false;
            frame(); frame();

            // Toggle a real importer widget, then discard it through Revert.
            click(locate("Generate mipmaps", inspectorID("##Texture mipmaps"), 190.f));
            Check(Contents(TexturePath + ".meta") == original && texture->GetImportSettings() == originalImport,
                  "A staged importer edit changed disk or the live texture before Apply");
            click(locate("Inspector Revert", inspectorID("Revert"), 90.f));
            Check(Contents(TexturePath + ".meta") == original && texture->GetImportSettings() == originalImport,
                  "Revert wrote metadata or changed the applied texture");

            click(locate("Generate mipmaps", inspectorID("##Texture mipmaps"), 190.f));
            click(locate("Edge color bleed", inspectorID("##Texture alpha bleed"), 190.f));
            Check(Contents(TexturePath + ".meta") == original, "Staged settings were saved without Apply");
            click(locate("Inspector Apply", inspectorID("Apply"), 30.f));
            const auto applied = LoadTextureImportSettings(TexturePath);
            Check(!applied.mipmaps && applied.alphaBleed, "Apply did not save the chosen mipmap/edge settings");
            Check(texture->GetImportSettings() == applied && texture->GetGLTexture().id == originalGL,
                  "Inspector Apply did not live reload settings while preserving the GL texture ID");
            Check(AssetManager::GetMetaFile(TexturePath)->uuid == originalUUID,
                  "Changing texture import settings replaced the asset UUID");
            SpriteSheet retained;
            std::string error;
            Check(LoadSpriteSheet(TexturePath, 256, 128, retained, &error) && retained.regions.size() == 2 &&
                  retained.regions[0].name == "Health bar", "Texture Apply lost unrelated SpriteSheet metadata");
            const auto appliedBytes = Contents(TexturePath + ".meta");
            frame(); frame();
            Check(Contents(TexturePath + ".meta") == appliedBytes,
                  "An idle Inspector draw rewrote the newly applied metadata");

            // Opening is also tested through its actual Inspector button. The
            // sprite window stays independent of the selected asset inspector.
            click(locate("Open Sprite Editor", inspectorID("Sprite Editor"), 45.f));
            Check(editor.m_spriteEditor != nullptr, "Inspector button did not open the Sprite Editor");
            showSprite = true;
            frame(); frame();
            Check(ImGui::FindWindowByName("Sprite Editor###SpriteEditor") != nullptr,
                  "Sprite Editor did not draw its window");
            Check(Contents(TexturePath + ".meta") == appliedBytes,
                  "Opening the Sprite Editor rewrote saved regions");

            const auto applyRegion = locate("Apply region to selected Sprite2D",
                spriteID("Apply region to selected Sprite2D", true),
                spritePanel(true)->Pos.x + spritePanel(true)->Size.x * .5f, 300.f, 1050.f);
            editor.m_sceneHistoryEditWasActive = false;
            click(applyRegion);
            const auto expectedUV = SpriteRegionUV(retained.regions[0], 256, 128);
            Check(glm::length(sprite.uv - expectedUV) < .00001f &&
                  glm::length(rect.pivot - retained.regions[0].pivot) < .00001f &&
                  rect.size == originalSize && !sprite.flipX && !sprite.flipY,
                  "Applying the region did not set the sprite UV/pivot without changing its authored size");
            Check(AssetManager::GetPath(sprite.textureHandle.id) == TexturePath,
                  "Applying a sprite region did not assign the selected atlas texture");
            Check(editor.m_sceneHistoryEditWasActive, "Applying a sprite region did not notify Scene Undo");
            Check(Contents(TexturePath + ".meta") == appliedBytes,
                  "Applying a region to a scene sprite rewrote atlas metadata");

            click(locate("Set native pixel size", spriteID("Set native pixel size", true),
                         spritePanel(true)->Pos.x + 10.f, 300.f, 1050.f));
            click(applyRegion);
            Check(rect.size == Vector2(96.f, 48.f), "Optional native pixel size did not use the selected region size");

            // Exercise sheet persistence through the actual Sprite Editor UI:
            // a new region is staged, saved, then another staged region is
            // discarded. Import settings and asset identity must survive both.
            click(locate("Full image", spriteID("Full image", true),
                         spritePanel(true)->Pos.x + 25.f, 75.f, 1050.f));
            Check(Contents(TexturePath + ".meta") == appliedBytes,
                  "Adding an unsaved sprite rectangle rewrote metadata before Apply");
            const auto sheetApply = locate("Sprite Editor Apply", spriteID("Apply"),
                                           spritePanel()->Pos.x + 30.f, 25.f, 160.f);
            click(sheetApply);
            SpriteSheet savedSheet;
            Check(LoadSpriteSheet(TexturePath, 256, 128, savedSheet, &error) && savedSheet.regions.size() == 3 &&
                  savedSheet.regions.back().x == 0 && savedSheet.regions.back().y == 0 &&
                  savedSheet.regions.back().width == 256 && savedSheet.regions.back().height == 128,
                  "Sprite Editor Apply did not persist the newly authored full-image region");
            Check(LoadTextureImportSettings(TexturePath) == applied &&
                  AssetManager::GetMetaFile(TexturePath)->uuid == originalUUID,
                  "Saving sprite rectangles changed texture import settings or asset UUID");
            const auto savedSheetBytes = Contents(TexturePath + ".meta");
            click(locate("Full image", spriteID("Full image", true),
                         spritePanel(true)->Pos.x + 25.f, 75.f, 1050.f));
            Check(Contents(TexturePath + ".meta") == savedSheetBytes,
                  "A second unsaved sprite rectangle changed the saved sheet");
            click(locate("Sprite Editor Revert", spriteID("Revert"),
                         spritePanel()->Pos.x + 90.f, 25.f, 160.f));
            // A correctly reverted draft disables Apply. If the fourth region
            // remained staged, this click would save it and fail the count.
            click(sheetApply);
            SpriteSheet revertedSheet;
            Check(Contents(TexturePath + ".meta") == savedSheetBytes &&
                  LoadSpriteSheet(TexturePath, 256, 128, revertedSheet, &error) && revertedSheet.regions.size() == 3,
                  "Sprite Editor Revert did not retain the three saved regions");
            Check(LoadTextureImportSettings(TexturePath) == applied &&
                  AssetManager::GetMetaFile(TexturePath)->uuid == originalUUID,
                  "Reverting sprite rectangles changed texture import settings or asset UUID");
            frame(ImVec2(-100, -100), false, artifacts / "texture-and-sprite-editor.png");
            editor.m_scene = nullptr;
            editor.m_app = nullptr;
            editor.m_window = nullptr;
        }
    };
}

int main(int argc, char **argv)
{
    const auto originalDirectory = fs::current_path();
    const fs::path artifacts = fs::absolute(argc > 1 ? argv[1] : "/tmp/mm-texture-editor-check");
    const auto temporary = fs::temp_directory_path() / ("canis-texture-inspector-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(artifacts); fs::create_directories(temporary);
    fs::current_path(temporary);
    try
    {
        Canis::Window window("Texture Inspector integration", ScreenWidth, ScreenHeight, true);
        ImGui::CreateContext();
        auto &io = ImGui::GetIO();
        io.IniFilename = nullptr; io.DisplaySize = ImVec2(ScreenWidth, ScreenHeight); io.DeltaTime = 1.f / 60.f;
        Check(ImGui_ImplOpenGL3_Init("#version 330 core"), "Could not initialize ImGui OpenGL renderer");
        ImGui::StyleColorsDark();
        WriteFixture();
        while (glGetError() != GL_NO_ERROR) {}
        {
            Canis::App app; Canis::Editor editor;
            Canis::EditorSceneTabsTestAccess::Run(app, editor, window, artifacts);
        }
        Canis::AssetManager::Free<Canis::TextureAsset>(TexturePath);
        ImGui_ImplOpenGL3_Shutdown(); ImGui::DestroyContext();
        fs::current_path(originalDirectory); fs::remove_all(temporary);
        std::cout << "Texture Inspector staged Apply/Revert, persistent metadata, live reload, Sprite Editor selection/UV/pivot and Scene Undo notification passed.\n"
                  << "Review screenshots: " << artifacts << '\n';
        return 0;
    }
    catch (const std::exception &error)
    {
        fs::current_path(originalDirectory);
        std::cerr << error.what() << "\nTemporary fixture retained: " << temporary << '\n';
        return 1;
    }
}
