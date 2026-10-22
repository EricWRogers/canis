#include <Canis/Editor.hpp>

#if CANIS_EDITOR
#include <Canis/AssetManager.hpp>
#include <Canis/EditorUI.hpp>
#include <Canis/TextureImport.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Canis
{
namespace
{
    struct TextureInspectorDraft
    {
        TextureImportSettings saved;
        TextureImportSettings staged;
        std::filesystem::file_time_type metadataWriteTime{};
        bool initialized = false;
        bool oneToOne = false;
        int background = 0;
        std::string error;
        std::string status;
    };

    void TextureHelp(const char *text)
    {
        EditorUI::ScopedFont font(EditorUI::DetailSize);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
    }

    std::filesystem::file_time_type MetadataWriteTime(const std::string &path)
    {
        std::error_code error;
        const auto time = std::filesystem::last_write_time(path + ".meta", error);
        return error ? std::filesystem::file_time_type{} : time;
    }

    TextureImportSettings UIPreset(bool point)
    {
        TextureImportSettings settings;
        settings.filter = point ? TextureFilter::Point : TextureFilter::Bilinear;
        settings.wrapU = settings.wrapV = TextureWrap::Clamp;
        settings.mipmaps = false;
        settings.alphaBleed = true;
        settings.mipBias = 0.f;
        return settings;
    }

    void Checkerboard(ImDrawList *draw, const ImVec2 &origin, const ImVec2 &size)
    {
        constexpr float tile = 12.f;
        const auto clipMin = draw->GetClipRectMin();
        const auto clipMax = draw->GetClipRectMax();
        const float left = std::max(origin.x, clipMin.x);
        const float top = std::max(origin.y, clipMin.y);
        const float right = std::min(origin.x + size.x, clipMax.x);
        const float bottom = std::min(origin.y + size.y, clipMax.y);
        if (right <= left || bottom <= top)
            return;
        const int startX = static_cast<int>(std::floor((left - origin.x) / tile));
        const int startY = static_cast<int>(std::floor((top - origin.y) / tile));
        const int endX = static_cast<int>(std::ceil((right - origin.x) / tile));
        const int endY = static_cast<int>(std::ceil((bottom - origin.y) / tile));
        for (int y = startY; y < endY; ++y)
            for (int x = startX; x < endX; ++x)
                draw->AddRectFilled(
                    ImVec2(origin.x + x * tile, origin.y + y * tile),
                    ImVec2(std::min(origin.x + (x + 1) * tile, origin.x + size.x),
                           std::min(origin.y + (y + 1) * tile, origin.y + size.y)),
                    ((x + y) & 1) ? IM_COL32(85, 85, 85, 255) : IM_COL32(130, 130, 130, 255));
    }

    void TexturePreview(TextureInspectorDraft &draft, const GLTexture &texture)
    {
        if (texture.id == 0 || texture.width <= 0 || texture.height <= 0)
        {
            TextureHelp("Preview unavailable. Check that the source image can be decoded.");
            return;
        }

        ImGui::Text("%d x %d pixels", texture.width, texture.height);
        ImGui::Checkbox("1:1 pixels", &draft.oneToOne);
        EditorUI::PropertyLabel("Background");
        ImGui::Combo("##Texture background", &draft.background, "Checkerboard\0Dark\0White\0");

        const float available = std::max(1.f, ImGui::GetContentRegionAvail().x);
        const float previewHeight = std::min(280.f, std::max(160.f, available));
        if (ImGui::BeginChild("Texture preview", ImVec2(0.f, previewHeight), ImGuiChildFlags_Borders,
                             ImGuiWindowFlags_HorizontalScrollbar))
        {
            const auto region = ImGui::GetContentRegionAvail();
            const float scale = draft.oneToOne ? 1.f : std::min(
                std::max(1.f, region.x) / static_cast<float>(texture.width),
                std::max(1.f, region.y) / static_cast<float>(texture.height));
            const ImVec2 imageSize(texture.width * scale, texture.height * scale);
            const auto cursor = ImGui::GetCursorScreenPos();
            const ImVec2 imageOrigin(cursor.x + std::max(0.f, (region.x - imageSize.x) * .5f),
                                     cursor.y + std::max(0.f, (region.y - imageSize.y) * .5f));
            auto *draw = ImGui::GetWindowDrawList();
            if (draft.background == 0)
                Checkerboard(draw, imageOrigin, imageSize);
            else
                draw->AddRectFilled(imageOrigin,
                    ImVec2(imageOrigin.x + imageSize.x, imageOrigin.y + imageSize.y),
                    draft.background == 1 ? IM_COL32(15, 15, 15, 255) : IM_COL32(255, 255, 255, 255));
            draw->AddImage(static_cast<ImTextureID>(static_cast<intptr_t>(texture.id)), imageOrigin,
                ImVec2(imageOrigin.x + imageSize.x, imageOrigin.y + imageSize.y));
            ImGui::Dummy(ImVec2(std::max(imageSize.x, region.x), std::max(imageSize.y, region.y)));
        }
        ImGui::EndChild();
        TextureHelp("Preview uses the applied import settings. Apply changes to update the preview and every use of this texture.");
    }
}

bool Editor::DrawTextureAssetInspector(const std::string &assetPath)
{
    auto *meta = AssetManager::GetMetaFile(assetPath);
    if (!meta || meta->type != MetaFileAsset::FileType::TEXTURE)
        return false;
    const std::string path = meta->path.empty() ? assetPath : meta->path;

    // Drafts belong to assets, rather than individual UI frames. Merely selecting
    // a texture never changes its metadata or how it is rendered.
    static std::unordered_map<std::string, TextureInspectorDraft> drafts;
    auto &draft = drafts[path];
    const auto writeTime = MetadataWriteTime(path);
    if (!draft.initialized || (draft.saved == draft.staged && draft.metadataWriteTime != writeTime))
    {
        draft.saved = draft.staged = LoadTextureImportSettings(path);
        draft.metadataWriteTime = writeTime;
        draft.initialized = true;
    }

    ImGui::PushID(path.c_str());
    {
        EditorUI::ScopedFont font(EditorUI::TitleSize);
        ImGui::TextWrapped("%s", meta->name.c_str());
    }
    {
        EditorUI::ScopedFont font(EditorUI::DetailSize);
        ImGui::TextWrapped("Path: %s", path.c_str());
    }
    ImGui::Separator();

    TextureAsset *texture = AssetManager::GetTexture(path);
    if (EditorUI::SectionHeader("Texture preview", ImGuiTreeNodeFlags_DefaultOpen))
        TexturePreview(draft, texture ? texture->GetGLTexture() : GLTexture{});
    if (ImGui::Button("Sprite Editor"))
        OpenSpriteEditor(path);
    TextureHelp("Slice an atlas or adjust sprite rectangles and pivots in the Sprite Editor.");

    if (EditorUI::SectionHeader("Import settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const TextureImportSettings defaults;
        int preset = draft.staged == UIPreset(true) ? 1 : draft.staged == UIPreset(false) ? 2 :
                     draft.staged == defaults ? 3 : 0;
        EditorUI::PropertyLabel("Preset");
        if (ImGui::Combo("##Texture preset", &preset,
                         "Custom\0Pixel sprite / UI\0Smooth UI\0Defaults\0"))
        {
            if (preset == 1 || preset == 2)
                draft.staged = UIPreset(preset == 1);
            else if (preset == 3)
                draft.staged = defaults;
            draft.status.clear();
        }

        int filter = static_cast<int>(draft.staged.filter);
        EditorUI::PropertyLabel("Filter");
        if (ImGui::Combo("##Texture filter", &filter, "Legacy\0Point\0Bilinear\0Trilinear\0"))
            draft.staged.filter = static_cast<TextureFilter>(filter);
        TextureHelp("Point keeps pixel art crisp. Bilinear smooths between pixels. Trilinear also blends mip levels when mipmaps are enabled.");

        int wrapU = static_cast<int>(draft.staged.wrapU);
        int wrapV = static_cast<int>(draft.staged.wrapV);
        EditorUI::PropertyLabel("Wrap U");
        if (ImGui::Combo("##Texture wrap U", &wrapU, "Repeat\0Clamp\0Mirror\0"))
            draft.staged.wrapU = static_cast<TextureWrap>(wrapU);
        EditorUI::PropertyLabel("Wrap V");
        if (ImGui::Combo("##Texture wrap V", &wrapV, "Repeat\0Clamp\0Mirror\0"))
            draft.staged.wrapV = static_cast<TextureWrap>(wrapV);
        TextureHelp("Clamp stops sampling the opposite image edge. Sprite atlases still need spacing between neighboring sprites.");

        EditorUI::PropertyLabel("Generate mipmaps");
        ImGui::Checkbox("##Texture mipmaps", &draft.staged.mipmaps);
        if (draft.staged.mipmaps)
            TextureHelp("Mipmaps reduce distant shimmer, but can mix neighboring atlas sprites. Disable for screen-space HUDs or add padded atlas borders.");
        if (draft.staged.filter == TextureFilter::Trilinear && !draft.staged.mipmaps)
            TextureHelp("Trilinear without mipmaps uses bilinear sampling.");

        EditorUI::PropertyLabel("Edge color bleed");
        ImGui::Checkbox("##Texture alpha bleed", &draft.staged.alphaBleed);
        TextureHelp("Extends visible RGB colors into fully transparent pixels to reduce dark fringes. Alpha and the source image stay unchanged; this does not separate adjacent atlas sprites.");

        ImGui::BeginDisabled(!draft.staged.mipmaps);
        EditorUI::PropertyLabel("Mip bias");
        ImGui::SliderFloat("##Texture mip bias", &draft.staged.mipBias, -2.f, 2.f, "%.2f");
        ImGui::EndDisabled();
        TextureHelp("Negative bias sharpens distant details and may shimmer; positive bias softens them. Only affects mipmapped textures.");
    }

    const bool dirty = !(draft.saved == draft.staged);
    if (dirty)
        draft.status.clear();
    ImGui::Separator();
    ImGui::TextDisabled("%s", dirty ? "Unapplied changes" : "Import settings saved");
    ImGui::BeginDisabled(!dirty);
    if (ImGui::Button("Apply"))
    {
        draft.error.clear();
        draft.status.clear();
        if (SaveTextureImportSettings(path, draft.staged, &draft.error))
        {
            if (AssetManager::ReloadTexture(path))
            {
                draft.saved = draft.staged = LoadTextureImportSettings(path);
                draft.metadataWriteTime = MetadataWriteTime(path);
                draft.status = "Saved and reloaded. Scene and Game views now use these settings.";
            }
            else
            {
                // Save succeeded but the old GL resource remains in use. Keep
                // the draft dirty so Apply can retry after fixing the source.
                draft.error = "Metadata saved, but the source texture could not be reloaded. The previous texture remains in use. Check the source file and Apply again.";
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert"))
    {
        draft.saved = draft.staged = LoadTextureImportSettings(path);
        draft.metadataWriteTime = MetadataWriteTime(path);
        draft.error.clear();
        draft.status.clear();
    }
    ImGui::EndDisabled();
    if (!draft.error.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, .45f, .35f, 1.f));
        ImGui::TextWrapped("%s", draft.error.c_str());
        ImGui::PopStyleColor();
    }
    else if (!draft.status.empty())
        TextureHelp(draft.status.c_str());
    ImGui::PopID();
    return true;
}
}
#else
namespace Canis
{
bool Editor::DrawTextureAssetInspector(const std::string &) { return false; }
}
#endif
