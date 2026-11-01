#include <Canis/Editor.hpp>

#if CANIS_EDITOR
#include <Canis/AssetManager.hpp>
#include <Canis/EditorUI.hpp>
#include <Canis/TextureImport.hpp>
#include <imgui_internal.h>
#include <type_traits>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
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
    return DrawTextureAssetInspector(std::vector<std::string>{assetPath});
}

bool Editor::DrawTextureAssetInspector(const std::vector<std::string> &assetPaths)
{
    if (assetPaths.empty()) return false;
    std::vector<std::string> paths;
    for (const auto &assetPath : assetPaths)
    {
        auto *meta = AssetManager::GetMetaFile(assetPath);
        if (!meta || meta->type != MetaFileAsset::FileType::TEXTURE) return false;
        const auto path = meta->path.empty() ? assetPath : meta->path;
        if (std::find(paths.begin(), paths.end(), path) == paths.end()) paths.push_back(path);
    }

    // Drafts belong to assets, so selection changes retain unapplied edits.
    static std::unordered_map<std::string, TextureInspectorDraft> assetDrafts;
    std::vector<TextureInspectorDraft *> drafts;
    auto reload = [](TextureInspectorDraft &draft, const std::string &path)
    {
        draft.saved = draft.staged = LoadTextureImportSettings(path);
        draft.metadataWriteTime = MetadataWriteTime(path);
        draft.initialized = true;
        draft.error.clear();
        draft.status.clear();
    };
    for (const auto &path : paths)
    {
        auto &draft = assetDrafts[path];
        if (!draft.initialized || (draft.saved == draft.staged && draft.metadataWriteTime != MetadataWriteTime(path)))
            reload(draft, path);
        drafts.push_back(&draft);
    }
    auto &draft = *drafts.front();
    const bool multiple = paths.size() > 1;
    const auto &path = paths.front();
    ImGui::PushID(multiple ? "##TextureSelection" : path.c_str());
    {
        EditorUI::ScopedFont font(EditorUI::TitleSize);
        if (multiple) ImGui::Text("%zu textures selected", paths.size());
        else ImGui::TextWrapped("%s", AssetManager::GetMetaFile(path)->name.c_str());
    }
    if (multiple)
    {
        TextureHelp("Mixed values differ between textures. Changes affect every selected texture; other settings are preserved.");
        if (ImGui::TreeNode("Selected textures"))
        {
            for (const auto &item : paths) ImGui::TextWrapped("%s", item.c_str());
            ImGui::TreePop();
        }
    }
    else
    {
        EditorUI::ScopedFont font(EditorUI::DetailSize);
        ImGui::TextWrapped("Path: %s", path.c_str());
    }
    ImGui::Separator();

    if (!multiple)
    {
        auto *texture = AssetManager::GetTexture(path);
        if (EditorUI::SectionHeader("Texture preview", ImGuiTreeNodeFlags_DefaultOpen))
            TexturePreview(draft, texture ? texture->GetGLTexture() : GLTexture{});
        if (ImGui::Button("Sprite Editor")) OpenSpriteEditor(path);
        TextureHelp("Slice an atlas or adjust sprite rectangles and pivots in the Sprite Editor.");
    }
    auto mixed = [&](auto member)
    {
        return std::any_of(drafts.begin(), drafts.end(), [&](auto *item)
            { return item->staged.*member != draft.staged.*member; });
    };
    auto assign = [&](auto member, auto value)
    {
        for (auto *item : drafts)
        {
            item->staged.*member = value;
            item->status.clear();
        }
    };
    auto combo = [](const char *id, int &value, const char *options, bool isMixed)
    {
        const char *preview = options;
        for (int i = 0; i < value; ++i) preview += std::strlen(preview) + 1;
        bool changed = false;
        if (ImGui::BeginCombo(id, isMixed ? "Mixed" : preview))
        {
            int index = 0;
            for (const char *option = options; *option; option += std::strlen(option) + 1, ++index)
            {
                if (ImGui::Selectable(option, !isMixed && index == value))
                { value = index; changed = true; }
                if (!isMixed && index == value) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        return changed;
    };
    auto enumField = [&](const char *label, const char *id, auto member, const char *options)
    {
        int value = static_cast<int>(draft.staged.*member);
        EditorUI::PropertyLabel(label);
        if (combo(id, value, options, mixed(member)))
            assign(member, static_cast<std::decay_t<decltype(draft.staged.*member)>>(value));
    };
    auto boolField = [&](const char *label, const char *id, auto member)
    {
        EditorUI::PropertyLabel(label);
        const bool isMixed = mixed(member);
        bool value = !isMixed && draft.staged.*member;
        ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, isMixed);
        if (ImGui::Checkbox(id, &value)) assign(member, value);
        ImGui::PopItemFlag();
    };

    if (EditorUI::SectionHeader("Import settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const TextureImportSettings defaults;
        int preset = draft.staged == UIPreset(true) ? 1 : draft.staged == UIPreset(false) ? 2 :
                     draft.staged == defaults ? 3 : 0;
        const bool mixedPreset = std::any_of(drafts.begin(), drafts.end(), [&](auto *item)
            { return item->staged != draft.staged; });
        EditorUI::PropertyLabel("Preset");
        if (combo("##Texture preset", preset, "Custom\0Pixel sprite / UI\0Smooth UI\0Defaults\0", mixedPreset))
        {
            if (preset != 0)
                for (auto *item : drafts)
                {
                    item->staged = preset == 3 ? defaults : UIPreset(preset == 1);
                    item->status.clear();
                }
        }
        enumField("Filter", "##Texture filter", &TextureImportSettings::filter, "Legacy\0Point\0Bilinear\0Trilinear\0");
        TextureHelp("Point keeps pixel art crisp. Bilinear smooths between pixels. Trilinear also blends mip levels when mipmaps are enabled.");
        enumField("Wrap U", "##Texture wrap U", &TextureImportSettings::wrapU, "Repeat\0Clamp\0Mirror\0");
        enumField("Wrap V", "##Texture wrap V", &TextureImportSettings::wrapV, "Repeat\0Clamp\0Mirror\0");
        TextureHelp("Clamp stops sampling the opposite image edge. Sprite atlases still need spacing between neighboring sprites.");
        boolField("Generate mipmaps", "##Texture mipmaps", &TextureImportSettings::mipmaps);
        if (std::any_of(drafts.begin(), drafts.end(), [](auto *item) { return item->staged.mipmaps; }))
            TextureHelp("Mipmaps reduce distant shimmer, but can mix neighboring atlas sprites. Disable for screen-space HUDs or add padded atlas borders.");
        if (draft.staged.filter == TextureFilter::Trilinear && !draft.staged.mipmaps)
            TextureHelp("Trilinear without mipmaps uses bilinear sampling.");
        boolField("Edge color bleed", "##Texture alpha bleed", &TextureImportSettings::alphaBleed);
        TextureHelp("Extends visible RGB colors into fully transparent pixels to reduce dark fringes. Alpha and the source image stay unchanged; this does not separate adjacent atlas sprites.");
        const bool anyMipmaps = std::any_of(drafts.begin(), drafts.end(), [](auto *item) { return item->staged.mipmaps; });
        ImGui::BeginDisabled(!anyMipmaps);
        EditorUI::PropertyLabel("Mip bias");
        float bias = draft.staged.mipBias;
        if (ImGui::SliderFloat("##Texture mip bias", &bias, -2.f, 2.f,
                               mixed(&TextureImportSettings::mipBias) ? "Mixed" : "%.2f"))
            assign(&TextureImportSettings::mipBias, bias);
        ImGui::EndDisabled();
        TextureHelp("Negative bias sharpens distant details and may shimmer; positive bias softens them. Only affects mipmapped textures.");
    }

    const bool dirty = std::any_of(drafts.begin(), drafts.end(), [](auto *item) { return item->saved != item->staged; });
    ImGui::Separator();
    ImGui::TextDisabled("%s", dirty ? "Unapplied changes" : "Import settings saved");
    ImGui::BeginDisabled(!dirty);
    if (ImGui::Button("Apply"))
    {
        for (size_t i = 0; i < paths.size(); ++i)
        {
            auto &item = *drafts[i];
            if (item.saved == item.staged) continue;
            item.error.clear(); item.status.clear();
            if (SaveTextureImportSettings(paths[i], item.staged, &item.error))
            {
                if (AssetManager::ReloadTexture(paths[i]))
                {
                    reload(item, paths[i]);
                    item.status = "Saved and reloaded. Scene and Game views now use these settings.";
                }
                else
                    item.error = "Metadata saved, but the source texture could not be reloaded. The previous texture remains in use. Check the source file and Apply again.";
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert"))
        for (size_t i = 0; i < paths.size(); ++i) reload(*drafts[i], paths[i]);
    ImGui::EndDisabled();
    bool saved = false;
    for (size_t i = 0; i < paths.size(); ++i)
    {
        if (!drafts[i]->error.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, .45f, .35f, 1.f));
            if (multiple) ImGui::TextWrapped("%s: %s", paths[i].c_str(), drafts[i]->error.c_str());
            else ImGui::TextWrapped("%s", drafts[i]->error.c_str());
            ImGui::PopStyleColor();
        }
        saved |= !drafts[i]->status.empty();
    }
    if (saved && !dirty) TextureHelp("Saved and reloaded. Scene and Game views now use these settings.");
    ImGui::PopID();
    return true;
}
}
#else
namespace Canis
{
bool Editor::DrawTextureAssetInspector(const std::string &) { return false; }
bool Editor::DrawTextureAssetInspector(const std::vector<std::string> &) { return false; }
}
#endif
