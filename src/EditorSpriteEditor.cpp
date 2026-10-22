#include <Canis/Editor.hpp>

#if CANIS_EDITOR
#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/EditorUI.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/Scene.hpp>
#include <Canis/SpriteSheet.hpp>
#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <future>
#include <memory>

namespace Canis
{
    struct SpriteEditorState
    {
        bool open = false, dirty = false, fit = true, drawNew = false;
        bool nativeSize = false, gridByCount = false, sliceAppend = false;
        bool confirmDiscard = false, closing = false;
        std::string path, pendingPath, status;
        SpriteSheet sheet;
        DecodedImage image;
        SpriteGrid grid;
        int selected = -1, alphaThreshold = 0, drag = 0, handle = -1;
        float zoom = 1.f;
        ImVec2 pan = ImVec2(0, 0), dragStart = ImVec2(0, 0);
        SpriteRegion dragRegion;
    };

    namespace
    {
        void LoadSheet(SpriteEditorState &state, const std::string &path)
        {
            state.path = path;
            // DecodeImageRGBA deliberately sets stb's thread-local flip flag.
            // Keep that worker setting away from the render thread, where
            // legacy model/texture loaders still use the global flip flag.
            state.image = std::async(std::launch::async, [path]() { return DecodeImageRGBA(path); }).get();
            state.status.clear();
            state.sheet = {state.image.width, state.image.height, {}};
            if (!state.image.error.empty()) state.status = state.image.error;
            else if (LoadSpriteSheet(path, state.image.width, state.image.height, state.sheet, &state.status))
            {
                // A useful starting selection for an unsliced image. Merely
                // opening it never creates or changes a metadata file.
                if (state.sheet.regions.empty())
                    state.sheet.regions.push_back({"sprite_0", 0, 0, state.image.width, state.image.height, Vector2(.5f)});
            }
            state.selected = state.sheet.regions.empty() ? -1 : 0;
            state.dirty = false; state.fit = true; state.pan = ImVec2(0, 0);
            state.drawNew = false; state.drag = 0;
            state.open = true;
        }

        bool SaveSheet(SpriteEditorState &state)
        {
            NormalizeSpriteSheet(state.sheet);
            if (!SaveSpriteSheet(state.path, state.sheet, &state.status)) return false;
            state.dirty = false;
            state.status = "Sprite regions saved. Source image and import settings are unchanged.";
            return true;
        }

        bool Contains(ImVec2 point, ImVec2 min, ImVec2 max)
        {
            return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
        }

        ImVec2 ScreenPoint(const ImVec2 &origin, float zoom, float x, float y)
        {
            return ImVec2(origin.x + x * zoom, origin.y + y * zoom);
        }

        void DrawCanvas(SpriteEditorState &state, const GLTexture &texture)
        {
            const auto origin = ImGui::GetCursorScreenPos();
            ImVec2 available = ImGui::GetContentRegionAvail();
            available.x = std::max(available.x, 64.f); available.y = std::max(available.y, 64.f);
            ImGui::InvisibleButton("##SpriteCanvas", available, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
            const bool hovered = ImGui::IsItemHovered();
            auto &io = ImGui::GetIO();
            const auto mouse = io.MousePos;
            const int width = state.sheet.imageWidth, height = state.sheet.imageHeight;
            if (state.fit && width > 0 && height > 0)
            {
                state.zoom = std::clamp(std::min((available.x - 32) / width, (available.y - 32) / height), .02f, 64.f);
                state.pan = ImVec2(0, 0); state.fit = false;
            }
            auto imageOrigin = [&]() {
                return ImVec2(origin.x + available.x * .5f + state.pan.x - width * state.zoom * .5f,
                              origin.y + available.y * .5f + state.pan.y - height * state.zoom * .5f);
            };
            if (hovered && io.MouseWheel != 0)
            {
                const auto before = imageOrigin();
                const float pixelX = (mouse.x - before.x) / state.zoom;
                const float pixelY = (mouse.y - before.y) / state.zoom;
                state.zoom = std::clamp(state.zoom * std::pow(1.2f, io.MouseWheel), .02f, 64.f);
                const auto after = imageOrigin();
                state.pan.x += mouse.x - (after.x + pixelX * state.zoom);
                state.pan.y += mouse.y - (after.y + pixelY * state.zoom);
            }
            const bool pan = (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) &&
                              (hovered || ImGui::IsItemActive())) ||
                             (hovered && ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseDragging(ImGuiMouseButton_Left));
            if (pan)
            {
                state.pan.x += io.MouseDelta.x; state.pan.y += io.MouseDelta.y;
                state.drag = 0;
            }
            const auto topLeft = imageOrigin();
            const ImVec2 bottomRight = ScreenPoint(topLeft, state.zoom, float(width), float(height));
            const auto pixel = ImVec2((mouse.x - topLeft.x) / state.zoom, (mouse.y - topLeft.y) / state.zoom);
            const int hx[] = {-1, 0, 1, 1, 1, 0, -1, -1};
            const int hy[] = {-1, -1, -1, 0, 1, 1, 1, 0};
            if (hovered && !pan && !ImGui::IsKeyDown(ImGuiKey_Space) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                state.drag = 0;
                if (state.drawNew && Contains(mouse, topLeft, bottomRight))
                {
                    if (state.sheet.regions.size() >= 4096) state.status = "A sheet can contain at most 4096 regions.";
                    else
                    {
                        SpriteRegion region{"sprite_" + std::to_string(state.sheet.regions.size()),
                            std::clamp(int(std::floor(pixel.x)), 0, width - 1),
                            std::clamp(int(std::floor(pixel.y)), 0, height - 1), 1, 1, Vector2(.5f)};
                        state.sheet.regions.push_back(region);
                        state.selected = int(state.sheet.regions.size()) - 1;
                        state.drag = 3; state.dirty = true;
                    }
                }
                else
                {
                    if (state.selected >= 0 && state.selected < int(state.sheet.regions.size()))
                    {
                        const auto &region = state.sheet.regions[state.selected];
                        for (int handle = 0; handle < 8; ++handle)
                        {
                            const auto p = ScreenPoint(topLeft, state.zoom,
                                region.x + region.width * (hx[handle] + 1.f) * .5f,
                                region.y + region.height * (hy[handle] + 1.f) * .5f);
                            if (std::abs(mouse.x - p.x) <= 7 && std::abs(mouse.y - p.y) <= 7)
                            { state.drag = 2; state.handle = handle; break; }
                        }
                    }
                    if (state.drag == 0)
                        for (int index = int(state.sheet.regions.size()) - 1; index >= 0; --index)
                        {
                            const auto &region = state.sheet.regions[index];
                            if (pixel.x >= region.x && pixel.y >= region.y &&
                                pixel.x < region.x + region.width && pixel.y < region.y + region.height)
                            { state.selected = index; state.drag = 1; break; }
                        }
                }
                if (state.drag != 0)
                {
                    state.dragStart = pixel;
                    state.dragRegion = state.sheet.regions[state.selected];
                }
            }
            if (state.drag != 0 && ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                auto &region = state.sheet.regions[state.selected];
                const auto before = region;
                const int dx = int(std::round(pixel.x - state.dragStart.x));
                const int dy = int(std::round(pixel.y - state.dragStart.y));
                if (state.drag == 1)
                {
                    region.x = std::clamp(state.dragRegion.x + dx, 0, width - region.width);
                    region.y = std::clamp(state.dragRegion.y + dy, 0, height - region.height);
                }
                else
                {
                    int left = state.dragRegion.x, top = state.dragRegion.y;
                    int right = left + state.dragRegion.width, bottom = top + state.dragRegion.height;
                    if (state.drag == 3)
                    {
                        const int x = std::clamp(int(std::floor(pixel.x)), 0, width - 1);
                        const int y = std::clamp(int(std::floor(pixel.y)), 0, height - 1);
                        right = std::max(left, x) + 1; bottom = std::max(top, y) + 1;
                        left = std::min(left, x); top = std::min(top, y);
                    }
                    else
                    {
                        if (hx[state.handle] < 0) left = std::clamp(left + dx, 0, right - 1);
                        if (hx[state.handle] > 0) right = std::clamp(right + dx, left + 1, width);
                        if (hy[state.handle] < 0) top = std::clamp(top + dy, 0, bottom - 1);
                        if (hy[state.handle] > 0) bottom = std::clamp(bottom + dy, top + 1, height);
                    }
                    region.x = left; region.y = top; region.width = right - left; region.height = bottom - top;
                }
                if (before.x != region.x || before.y != region.y || before.width != region.width || before.height != region.height)
                    state.dirty = true;
            }
            if (state.drag != 0 && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                if (state.drag == 3) state.drawNew = false;
                state.drag = 0;
            }

            auto *draw = ImGui::GetWindowDrawList();
            const ImVec2 canvasMax(origin.x + available.x, origin.y + available.y);
            draw->PushClipRect(origin, canvasMax, true);
            draw->AddRectFilled(origin, canvasMax, IM_COL32(20, 23, 28, 255));
            const ImVec2 clippedMin(std::max(topLeft.x, origin.x), std::max(topLeft.y, origin.y));
            const ImVec2 clippedMax(std::min(bottomRight.x, canvasMax.x), std::min(bottomRight.y, canvasMax.y));
            draw->PushClipRect(clippedMin, clippedMax, true);
            constexpr float checker = 12.f;
            for (int row = 0; row < int(available.y / checker) + 1; ++row)
                for (int col = 0; col < int(available.x / checker) + 1; ++col)
                    draw->AddRectFilled(ImVec2(origin.x + col * checker, origin.y + row * checker),
                        ImVec2(origin.x + (col + 1) * checker, origin.y + (row + 1) * checker),
                        (row + col) % 2 ? IM_COL32(65, 69, 77, 255) : IM_COL32(88, 92, 100, 255));
            draw->AddImage((ImTextureID)(intptr_t)texture.id, topLeft, bottomRight, ImVec2(0, 0), ImVec2(1, 1));
            if (state.zoom >= 8)
            {
                const int firstX = std::max(0, int(std::floor((origin.x - topLeft.x) / state.zoom)));
                const int lastX = std::min(width, int(std::ceil((canvasMax.x - topLeft.x) / state.zoom)));
                const int firstY = std::max(0, int(std::floor((origin.y - topLeft.y) / state.zoom)));
                const int lastY = std::min(height, int(std::ceil((canvasMax.y - topLeft.y) / state.zoom)));
                for (int x = firstX; x <= lastX; ++x)
                    draw->AddLine(ScreenPoint(topLeft, state.zoom, float(x), float(firstY)),
                                  ScreenPoint(topLeft, state.zoom, float(x), float(lastY)), IM_COL32(0, 0, 0, 65));
                for (int y = firstY; y <= lastY; ++y)
                    draw->AddLine(ScreenPoint(topLeft, state.zoom, float(firstX), float(y)),
                                  ScreenPoint(topLeft, state.zoom, float(lastX), float(y)), IM_COL32(0, 0, 0, 65));
            }
            draw->PopClipRect();
            for (int i = 0; i < int(state.sheet.regions.size()); ++i)
            {
                const auto &region = state.sheet.regions[i];
                const auto min = ScreenPoint(topLeft, state.zoom, float(region.x), float(region.y));
                const auto max = ScreenPoint(topLeft, state.zoom, float(region.x + region.width), float(region.y + region.height));
                const bool selected = i == state.selected;
                const auto color = selected ? IM_COL32(255, 207, 80, 255) : IM_COL32(107, 211, 255, 210);
                draw->AddRect(min, max, color, 0.f, 0, selected ? 2.f : 1.f);
                if (selected || (max.x - min.x > 70 && max.y - min.y > 25))
                    draw->AddText(ImVec2(min.x + 4, min.y + 3), color, region.name.c_str());
                if (selected)
                {
                    for (int handle = 0; handle < 8; ++handle)
                    {
                        const auto p = ScreenPoint(topLeft, state.zoom,
                            region.x + region.width * (hx[handle] + 1.f) * .5f,
                            region.y + region.height * (hy[handle] + 1.f) * .5f);
                        draw->AddRectFilled(ImVec2(p.x - 3, p.y - 3), ImVec2(p.x + 3, p.y + 3), color);
                    }
                    const auto pivot = ScreenPoint(topLeft, state.zoom, region.x + region.width * region.pivot.x,
                                                   region.y + region.height * (1.f - region.pivot.y));
                    draw->AddCircle(pivot, 5.f, IM_COL32(255, 122, 180, 255), 12, 2.f);
                    draw->AddLine(ImVec2(pivot.x - 8, pivot.y), ImVec2(pivot.x + 8, pivot.y), IM_COL32(255, 122, 180, 255));
                    draw->AddLine(ImVec2(pivot.x, pivot.y - 8), ImVec2(pivot.x, pivot.y + 8), IM_COL32(255, 122, 180, 255));
                }
            }
            draw->PopClipRect();
            if (hovered && state.drawNew) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            if (hovered) ImGui::SetTooltip("Pixel %.0f, %.0f | Wheel: zoom | Middle drag / Space + drag: pan", pixel.x, pixel.y);
        }
    }

    void Editor::OpenSpriteEditor(const std::string &path)
    {
        if (!m_spriteEditor) m_spriteEditor = std::make_shared<SpriteEditorState>();
        auto &state = *m_spriteEditor;
        if (state.dirty && path != state.path)
        {
            state.pendingPath = path; state.confirmDiscard = true; state.open = true;
        }
        else if (path != state.path || !state.open) LoadSheet(state, path);
        else state.open = true;
        ImGui::SetWindowFocus("Sprite Editor###SpriteEditor");
    }

    void Editor::DrawSpriteEditor()
    {
        if (!m_spriteEditor || !m_spriteEditor->open) return;
        auto &state = *m_spriteEditor;
        ImGui::SetNextWindowSize(ImVec2(1100, 720), ImGuiCond_FirstUseEver);
        bool open = true;
        const auto flags = state.dirty ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;
        const bool visible = ImGui::Begin("Sprite Editor###SpriteEditor", &open, flags);
        if (!open)
        {
            if (state.dirty) { state.confirmDiscard = true; state.closing = true; }
            else state.open = false;
        }
        if (!visible) { ImGui::End(); return; }
        EditorUI::ScopedFont font(EditorUI::BodySize);
        ImGui::TextUnformatted(state.path.c_str());
        ImGui::BeginDisabled(!state.dirty);
        if (ImGui::Button("Apply")) SaveSheet(state);
        ImGui::SameLine();
        if (ImGui::Button("Revert")) LoadSheet(state, state.path);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Fit")) state.fit = true;
        ImGui::SameLine();
        if (ImGui::Button("1:1")) { state.zoom = 1.f; state.fit = false; state.pan = ImVec2(0, 0); }
        ImGui::SameLine();
        ImGui::Text("%.0f%%", state.zoom * 100);
        ImGui::SameLine();
        ImGui::Checkbox("Draw new rectangle", &state.drawNew);
        ImGui::Separator();

        TextureAsset *asset = AssetManager::GetTexture(state.path);
        const GLTexture texture = asset ? asset->GetGLTexture() : GLTexture{};
        if (texture.id && state.sheet.imageWidth > 0 && state.sheet.imageHeight > 0)
        {
            const float sidebarWidth = std::clamp(ImGui::GetContentRegionAvail().x * .32f, 270.f, 355.f);
            ImGui::BeginChild("##SpriteCanvasPane", ImVec2(std::max(64.f, ImGui::GetContentRegionAvail().x - sidebarWidth - 8), -36.f), ImGuiChildFlags_Borders);
            DrawCanvas(state, texture);
            ImGui::EndChild();
            ImGui::SameLine();
            ImGui::BeginChild("##SpriteProperties", ImVec2(0, -36.f));
            ImGui::Text("%d x %d pixels", state.sheet.imageWidth, state.sheet.imageHeight);
            ImGui::Text("%zu regions", state.sheet.regions.size());
            ImGui::BeginChild("##SpriteRegions", ImVec2(0, 144), ImGuiChildFlags_Borders);
            for (int i = 0; i < int(state.sheet.regions.size()); ++i)
            {
                ImGui::PushID(i);
                if (ImGui::Selectable(state.sheet.regions[i].name.c_str(), state.selected == i)) state.selected = i;
                ImGui::PopID();
            }
            ImGui::EndChild();
            if (ImGui::Button("Full image") && state.sheet.regions.size() < 4096)
            {
                state.sheet.regions.push_back({"sprite_" + std::to_string(state.sheet.regions.size()), 0, 0,
                    state.sheet.imageWidth, state.sheet.imageHeight, Vector2(.5f)});
                state.selected = int(state.sheet.regions.size()) - 1; state.dirty = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(state.selected < 0);
            if (ImGui::Button("Delete"))
            {
                state.sheet.regions.erase(state.sheet.regions.begin() + state.selected);
                state.selected = std::min(state.selected, int(state.sheet.regions.size()) - 1); state.dirty = true;
            }
            ImGui::EndDisabled();
            if (state.selected >= 0 && state.selected < int(state.sheet.regions.size()))
            {
                auto &region = state.sheet.regions[state.selected];
                char name[256]; std::snprintf(name, sizeof(name), "%s", region.name.c_str());
                EditorUI::PropertyLabel("Name");
                if (ImGui::InputText("##SpriteName", name, sizeof(name))) { region.name = name; state.dirty = true; }
                int bounds[] = {region.x, region.y, region.width, region.height};
                EditorUI::PropertyLabel("X / Y / W / H");
                if (ImGui::DragInt4("##SpriteBounds", bounds, 1.f))
                {
                    region.x = bounds[0]; region.y = bounds[1]; region.width = bounds[2]; region.height = bounds[3];
                    NormalizeSpriteSheet(state.sheet); state.dirty = true;
                }
                EditorUI::PropertyLabel("Pivot X / Y");
                if (ImGui::SliderFloat2("##SpritePivot", &region.pivot.x, 0.f, 1.f, "%.2f")) state.dirty = true;
                EditorUI::PropertyLabel("Pivot preset");
                if (ImGui::BeginCombo("##SpritePivotPreset", "Choose preset"))
                {
                    const char *names[] = {"Bottom left", "Bottom", "Bottom right", "Left", "Center", "Right", "Top left", "Top", "Top right"};
                    for (int index = 0; index < 9; ++index)
                        if (ImGui::Selectable(names[index]))
                        { region.pivot = Vector2((index % 3) * .5f, (index / 3) * .5f); state.dirty = true; }
                    ImGui::EndCombo();
                }
                EditorUI::PropertyLabel("Alpha threshold");
                ImGui::SliderInt("##SpriteTrimThreshold", &state.alphaThreshold, 0, 254);
                if (ImGui::Button("Trim to alpha"))
                {
                    if (TrimSpriteRegion(region, state.image.pixels, state.sheet.imageWidth, state.sheet.imageHeight,
                                         static_cast<std::uint8_t>(state.alphaThreshold))) state.dirty = true;
                    else state.status = "The selected rectangle has no pixels above the alpha threshold.";
                }
                const auto uv = SpriteRegionUV(region, state.sheet.imageWidth, state.sheet.imageHeight);
                ImGui::Text("UV %.3f, %.3f, %.3f, %.3f", uv.x, uv.y, uv.z, uv.w);
                ImGui::Checkbox("Set native pixel size", &state.nativeSize);
                const auto selection = GetSelectedEntities();
                const bool canApply = m_mode == EditorMode::EDIT && std::any_of(selection.begin(), selection.end(),
                    [](Entity *entity) { return entity && entity->HasComponent<Sprite2D>() && entity->HasComponent<RectTransform>(); });
                ImGui::BeginDisabled(!canApply);
                if (ImGui::Button("Apply region to selected Sprite2D", ImVec2(-1, 0)))
                {
                    const auto handle = AssetManager::GetTextureHandle(state.path);
                    for (auto *entity : selection)
                        if (entity && entity->HasComponent<Sprite2D>() && entity->HasComponent<RectTransform>())
                        {
                            auto &sprite = entity->GetComponent<Sprite2D>();
                            auto &rect = entity->GetComponent<RectTransform>();
                            sprite.textureHandle = handle; sprite.uv = uv;
                            sprite.flipX = false; sprite.flipY = false;
                            rect.pivot = region.pivot;
                            if (state.nativeSize) rect.size = Vector2(region.width, region.height);
                        }
                    NotifySceneChanged();
                    state.status = "Region applied to the selected scene sprites. Scene Undo is available.";
                }
                ImGui::EndDisabled();
                if (!canApply) ImGui::TextWrapped("Select a scene entity with Sprite2D and RectTransform to use this region.");
            }
            if (EditorUI::SectionHeader("Grid slicing"))
            {
                ImGui::Checkbox("By columns and rows", &state.gridByCount);
                int cell[] = {state.grid.cellWidth, state.grid.cellHeight};
                int count[] = {std::max(1, state.grid.columns), std::max(1, state.grid.rows)};
                EditorUI::PropertyLabel(state.gridByCount ? "Columns / rows" : "Cell W / H");
                if (state.gridByCount)
                {
                    if (ImGui::InputInt2("##SpriteGridCount", count))
                    { state.grid.columns = std::clamp(count[0], 1, 4096); state.grid.rows = std::clamp(count[1], 1, 4096); }
                }
                else if (ImGui::InputInt2("##SpriteGridSize", cell))
                { state.grid.cellWidth = std::max(1, cell[0]); state.grid.cellHeight = std::max(1, cell[1]); }
                int offset[] = {state.grid.offsetX, state.grid.offsetY};
                EditorUI::PropertyLabel("Offset X / Y");
                if (ImGui::InputInt2("##SpriteGridOffset", offset))
                { state.grid.offsetX = std::max(0, offset[0]); state.grid.offsetY = std::max(0, offset[1]); }
                int padding[] = {state.grid.paddingX, state.grid.paddingY};
                EditorUI::PropertyLabel("Padding X / Y");
                if (ImGui::InputInt2("##SpriteGridPadding", padding))
                { state.grid.paddingX = std::max(0, padding[0]); state.grid.paddingY = std::max(0, padding[1]); }
                ImGui::Checkbox("Append to existing regions", &state.sliceAppend);
                if (ImGui::Button("Slice grid", ImVec2(-1, 0)))
                {
                    auto grid = state.grid;
                    grid.columns = state.gridByCount ? std::max(1, grid.columns) : 0;
                    grid.rows = state.gridByCount ? std::max(1, grid.rows) : 0;
                    auto regions = SliceSpriteGrid(state.sheet.imageWidth, state.sheet.imageHeight, grid, &state.status);
                    if (!regions.empty())
                    {
                        if (state.sliceAppend && state.sheet.regions.size() + regions.size() > 4096)
                            state.status = "Appending this grid would exceed the 4096 region limit.";
                        else
                        {
                            if (state.sliceAppend) state.sheet.regions.insert(state.sheet.regions.end(), regions.begin(), regions.end());
                            else state.sheet.regions = std::move(regions);
                            NormalizeSpriteSheet(state.sheet); state.selected = 0; state.dirty = true;
                            state.status = "Grid sliced. Apply saves the regions; Revert restores the saved sheet.";
                        }
                    }
                }
            }
            ImGui::EndChild();
        }
        else ImGui::TextWrapped("Unable to display the source image. %s", state.image.error.c_str());
        if (!state.status.empty()) ImGui::TextWrapped("%s", state.status.c_str());

        if (state.confirmDiscard)
        {
            ImGui::OpenPopup("Unsaved sprite regions"); state.confirmDiscard = false;
        }
        if (ImGui::BeginPopupModal("Unsaved sprite regions", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Apply your sprite changes before continuing?");
            auto proceed = [&]() {
                if (state.closing) state.open = false;
                else if (!state.pendingPath.empty()) LoadSheet(state, state.pendingPath);
                state.closing = false; state.pendingPath.clear();
                ImGui::CloseCurrentPopup();
            };
            if (ImGui::Button("Apply")) { if (SaveSheet(state)) proceed(); }
            ImGui::SameLine();
            if (ImGui::Button("Discard")) { state.dirty = false; proceed(); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            { state.closing = false; state.pendingPath.clear(); ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
        font.Reset();
        ImGui::End();
    }
}
#else
namespace Canis
{
    void Editor::OpenSpriteEditor(const std::string &) {}
    void Editor::DrawSpriteEditor() {}
}
#endif
