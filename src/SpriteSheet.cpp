#include <Canis/SpriteSheet.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/UUID.hpp>
#include <SDL3/SDL.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace Canis
{
    namespace
    {
        bool Fail(std::string *error, const std::string &message)
        {
            if (error) *error = message;
            return false;
        }

        void NormalizeRegion(SpriteRegion &region, int width, int height)
        {
            if (width <= 0 || height <= 0) return;
            region.x = std::clamp(region.x, 0, width - 1);
            region.y = std::clamp(region.y, 0, height - 1);
            region.width = std::clamp(region.width, 1, width - region.x);
            region.height = std::clamp(region.height, 1, height - region.y);
            region.pivot.x = std::isfinite(region.pivot.x) ? std::clamp(region.pivot.x, 0.f, 1.f) : .5f;
            region.pivot.y = std::isfinite(region.pivot.y) ? std::clamp(region.pivot.y, 0.f, 1.f) : .5f;
        }
    }

    void NormalizeSpriteSheet(SpriteSheet &sheet)
    {
        std::set<std::string> names;
        for (size_t i = 0; i < sheet.regions.size(); ++i)
        {
            auto &region = sheet.regions[i];
            NormalizeRegion(region, sheet.imageWidth, sheet.imageHeight);
            const std::string base = region.name.empty() ? "sprite_" + std::to_string(i) : region.name;
            region.name = base;
            for (size_t suffix = 2; !names.insert(region.name).second; ++suffix)
                region.name = base + "_" + std::to_string(suffix);
        }
    }

    Vector4 SpriteRegionUV(const SpriteRegion &region, int width, int height)
    {
        if (width <= 0 || height <= 0) return Vector4(0.f, 0.f, 1.f, 1.f);
        SpriteRegion normalized = region;
        NormalizeRegion(normalized, width, height);
        return Vector4(float(normalized.x) / width,
                       1.f - (float(normalized.y) + normalized.height) / height,
                       float(normalized.width) / width, float(normalized.height) / height);
    }

    std::vector<SpriteRegion> SliceSpriteGrid(int width, int height, const SpriteGrid &grid, std::string *error)
    {
        if (error) error->clear();
        auto bad = [&](const char *message) { Fail(error, message); return std::vector<SpriteRegion>{}; };
        if (width <= 0 || height <= 0) return bad("The source image has no pixels.");
        if (grid.offsetX < 0 || grid.offsetY < 0 || grid.paddingX < 0 || grid.paddingY < 0 ||
            grid.offsetX >= width || grid.offsetY >= height)
            return bad("Offset and padding must be nonnegative and leave room inside the image.");
        const bool countMode = grid.columns > 0 || grid.rows > 0;
        std::int64_t cellWidth = grid.cellWidth, cellHeight = grid.cellHeight;
        std::int64_t columns = grid.columns, rows = grid.rows;
        if (countMode)
        {
            if (columns <= 0 || rows <= 0) return bad("Both columns and rows must be positive.");
            cellWidth = (std::int64_t(width) - grid.offsetX - (columns - 1) * grid.paddingX) / columns;
            cellHeight = (std::int64_t(height) - grid.offsetY - (rows - 1) * grid.paddingY) / rows;
        }
        if (cellWidth <= 0 || cellHeight <= 0) return bad("Cell dimensions must be positive and fit the image.");
        if (!countMode)
        {
            columns = (std::int64_t(width) - grid.offsetX + grid.paddingX) / (cellWidth + grid.paddingX);
            rows = (std::int64_t(height) - grid.offsetY + grid.paddingY) / (cellHeight + grid.paddingY);
        }
        if (columns <= 0 || rows <= 0) return bad("No complete cells fit these settings.");
        if (columns > 4096 || rows > 4096 || columns * rows > 4096)
            return bad("A sprite sheet can contain at most 4096 regions.");
        std::vector<SpriteRegion> result;
        result.reserve(static_cast<size_t>(columns * rows));
        for (int row = 0; row < rows; ++row)
            for (int column = 0; column < columns; ++column)
            {
                const auto x = grid.offsetX + column * (cellWidth + grid.paddingX);
                const auto y = grid.offsetY + row * (cellHeight + grid.paddingY);
                if (x + cellWidth > width || y + cellHeight > height)
                    return bad("The grid extends beyond the image.");
                result.push_back({"sprite_" + std::to_string(row) + "_" + std::to_string(column),
                                  static_cast<int>(x), static_cast<int>(y), static_cast<int>(cellWidth),
                                  static_cast<int>(cellHeight), Vector2(.5f)});
            }
        return result;
    }

    bool TrimSpriteRegion(SpriteRegion &region, const std::vector<unsigned char> &rgba,
                          int width, int height, std::uint8_t threshold)
    {
        if (width <= 0 || height <= 0 || rgba.size() != size_t(width) * size_t(height) * 4) return false;
        SpriteRegion trimmed = region;
        NormalizeRegion(trimmed, width, height);
        int minX = width, minY = height, maxX = -1, maxY = -1;
        for (int y = trimmed.y; y < trimmed.y + trimmed.height; ++y)
            for (int x = trimmed.x; x < trimmed.x + trimmed.width; ++x)
                if (rgba[(size_t(y) * width + x) * 4 + 3] > threshold)
                {
                    minX = std::min(minX, x); minY = std::min(minY, y);
                    maxX = std::max(maxX, x); maxY = std::max(maxY, y);
                }
        if (maxX < minX || maxY < minY) return false;
        const float pivotX = trimmed.x + trimmed.width * trimmed.pivot.x;
        const float pivotY = trimmed.y + trimmed.height * (1.f - trimmed.pivot.y);
        trimmed.x = minX; trimmed.y = minY;
        trimmed.width = maxX - minX + 1; trimmed.height = maxY - minY + 1;
        trimmed.pivot = Vector2((pivotX - minX) / trimmed.width, 1.f - (pivotY - minY) / trimmed.height);
        NormalizeRegion(trimmed, width, height);
        region = std::move(trimmed);
        return true;
    }

    bool LoadSpriteSheet(const std::string &texturePath, int width, int height, SpriteSheet &sheet, std::string *error)
    {
        if (error) error->clear();
        if (width <= 0 || height <= 0) return Fail(error, "The source image has no pixels.");
        try
        {
            SpriteSheet loaded{width, height, {}};
            const std::string path = texturePath + ".meta";
            if (!std::filesystem::exists(path)) { sheet = std::move(loaded); return true; }
            const auto root = YAML::LoadFile(path);
            if (!root.IsMap()) return Fail(error, "Texture metadata must be a map.");
            const auto node = root["SpriteSheet"];
            if (!node) { sheet = std::move(loaded); return true; }
            if (!node.IsMap()) return Fail(error, "SpriteSheet metadata must be a map.");
            if (node["Version"] && node["Version"].as<int>() != 1)
                return Fail(error, "Unsupported SpriteSheet version; metadata was left unchanged.");
            const auto regions = node["Regions"];
            if (regions && !regions.IsSequence()) return Fail(error, "SpriteSheet Regions must be a sequence.");
            if (regions && regions.size() > 4096) return Fail(error, "SpriteSheet exceeds the 4096 region limit.");
            if (regions) for (const auto &entry : regions)
            {
                if (!entry.IsMap() || !entry["Bounds"].IsSequence() || entry["Bounds"].size() != 4)
                    return Fail(error, "Every sprite requires four pixel Bounds values.");
                SpriteRegion region;
                region.name = entry["Name"].as<std::string>("");
                region.x = entry["Bounds"][0].as<int>(); region.y = entry["Bounds"][1].as<int>();
                region.width = entry["Bounds"][2].as<int>(); region.height = entry["Bounds"][3].as<int>();
                if (const auto pivot = entry["Pivot"])
                {
                    if (!pivot.IsSequence() || pivot.size() != 2) return Fail(error, "Sprite Pivot needs two normalized values.");
                    region.pivot = Vector2(pivot[0].as<float>(), pivot[1].as<float>());
                    if (!std::isfinite(region.pivot.x) || !std::isfinite(region.pivot.y))
                        return Fail(error, "Sprite Pivot must contain finite numbers.");
                }
                loaded.regions.push_back(std::move(region));
            }
            NormalizeSpriteSheet(loaded);
            sheet = std::move(loaded);
            return true;
        }
        catch (const std::exception &exception) { return Fail(error, exception.what()); }
    }

    bool SaveSpriteSheet(const std::string &texturePath, const SpriteSheet &sheet, std::string *error)
    {
        if (error) error->clear();
        if (sheet.imageWidth <= 0 || sheet.imageHeight <= 0 || sheet.regions.size() > 4096)
            return Fail(error, "A valid image and at most 4096 regions are required.");
        const std::filesystem::path path(texturePath + ".meta");
        std::filesystem::path temporary;
        try
        {
            YAML::Node root;
            if (std::filesystem::exists(path))
            {
                root = YAML::LoadFile(path.string());
                if (!root.IsMap()) return Fail(error, "Existing metadata must be a map; it was left unchanged.");
                if (root["SpriteSheet"] && !root["SpriteSheet"].IsMap())
                    return Fail(error, "Existing SpriteSheet metadata must be a map; it was left unchanged.");
                if (root["SpriteSheet"]["Version"] && root["SpriteSheet"]["Version"].as<int>() != 1)
                    return Fail(error, "Unsupported SpriteSheet version; metadata was left unchanged.");
                SpriteSheet existing;
                if (!LoadSpriteSheet(texturePath, sheet.imageWidth, sheet.imageHeight, existing, error))
                    return false;
            }
            else
            {
                SDL_PathInfo info{};
                if (!SDL_GetPathInfo(texturePath.c_str(), &info) || info.type != SDL_PATHTYPE_FILE)
                    return Fail(error, "Cannot create metadata: source image was not found.");
                root["FileType"] = "TEXTURE";
                root["UUID"] = std::to_string(static_cast<uint64_t>(UUID()));
                root["name"] = GetFileName(texturePath); root["extension"] = GetFileExtension(texturePath);
                root["size"] = info.size; root["modified"] = info.modify_time;
            }
            SpriteSheet normalized = sheet;
            NormalizeSpriteSheet(normalized);
            auto node = root["SpriteSheet"];
            node["Version"] = 1;
            node["SourceWidth"] = normalized.imageWidth; node["SourceHeight"] = normalized.imageHeight;
            YAML::Node regions(YAML::NodeType::Sequence);
            for (const auto &region : normalized.regions)
            {
                YAML::Node entry;
                // Preserve extension fields attached to a named region as well
                // as extension fields in the containing metadata maps.
                if (const auto oldRegions = node["Regions"]; oldRegions && oldRegions.IsSequence())
                    for (const auto &old : oldRegions)
                        if (old["Name"].as<std::string>("") == region.name)
                        { entry = YAML::Clone(old); break; }
                entry["Name"] = region.name;
                entry["Bounds"] = YAML::Node(YAML::NodeType::Sequence);
                auto bounds = entry["Bounds"];
                bounds.push_back(region.x); bounds.push_back(region.y);
                bounds.push_back(region.width); bounds.push_back(region.height);
                bounds.SetStyle(YAML::EmitterStyle::Flow);
                entry["Pivot"] = YAML::Node(YAML::NodeType::Sequence);
                auto pivot = entry["Pivot"];
                pivot.push_back(region.pivot.x); pivot.push_back(region.pivot.y);
                pivot.SetStyle(YAML::EmitterStyle::Flow);
                regions.push_back(entry);
            }
            node["Regions"] = regions;
            root["SpriteSheet"] = node;
            static std::atomic<uint64_t> serial{0};
            temporary = path.string() + ".sprite-tmp-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(++serial);
            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                if (!output) return Fail(error, "Cannot write sprite metadata temporary file.");
                output << root;
                output.flush();
                if (!output) throw std::runtime_error("Failed while writing sprite metadata.");
                output.close();
                if (!output) throw std::runtime_error("Failed while closing sprite metadata.");
            }
            std::filesystem::rename(temporary, path);
            return true;
        }
        catch (const std::exception &exception)
        {
            if (!temporary.empty()) { std::error_code ignored; std::filesystem::remove(temporary, ignored); }
            return Fail(error, exception.what());
        }
    }
}
