#include <Canis/TextureImport.hpp>

#include <Canis/Debug.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/UUID.hpp>
#include <Canis/Yaml.hpp>
#include <SDL3/SDL_filesystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace Canis
{
    namespace
    {
        const char *FilterName(TextureFilter filter)
        {
            switch (filter)
            {
            case TextureFilter::Legacy: return "Legacy";
            case TextureFilter::Point: return "Point";
            case TextureFilter::Bilinear: return "Bilinear";
            case TextureFilter::Trilinear: return "Trilinear";
            }
            return nullptr;
        }

        const char *WrapName(TextureWrap wrap)
        {
            switch (wrap)
            {
            case TextureWrap::Repeat: return "Repeat";
            case TextureWrap::Clamp: return "Clamp";
            case TextureWrap::Mirror: return "Mirror";
            }
            return nullptr;
        }

        TextureFilter ReadFilter(const YAML::Node &node)
        {
            const std::string value = node.as<std::string>();
            if (value == "Legacy") return TextureFilter::Legacy;
            if (value == "Point") return TextureFilter::Point;
            if (value == "Bilinear") return TextureFilter::Bilinear;
            if (value == "Trilinear") return TextureFilter::Trilinear;
            throw std::runtime_error("unknown TextureImport.Filter '" + value + "'");
        }

        TextureWrap ReadWrap(const YAML::Node &node)
        {
            const std::string value = node.as<std::string>();
            if (value == "Repeat") return TextureWrap::Repeat;
            if (value == "Clamp") return TextureWrap::Clamp;
            if (value == "Mirror") return TextureWrap::Mirror;
            throw std::runtime_error("unknown texture wrap '" + value + "'");
        }

        bool Fail(std::string *error, const std::string &message)
        {
            if (error) *error = message;
            return false;
        }
    }

    TextureImportSettings LoadTextureImportSettings(const std::string &assetPath)
    {
        TextureImportSettings settings;
        const std::string path = assetPath + ".meta";
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) return settings;
        try
        {
            const YAML::Node root = YAML::LoadFile(path);
            if (!root.IsMap()) throw std::runtime_error("metadata must be a map");
            const YAML::Node node = root["TextureImport"];
            if (!node) return settings;
            if (!node.IsMap()) throw std::runtime_error("TextureImport must be a map");
            if (node["Filter"]) settings.filter = ReadFilter(node["Filter"]);
            if (node["WrapU"]) settings.wrapU = ReadWrap(node["WrapU"]);
            if (node["WrapV"]) settings.wrapV = ReadWrap(node["WrapV"]);
            if (node["Mipmaps"]) settings.mipmaps = node["Mipmaps"].as<bool>();
            if (node["AlphaBleed"]) settings.alphaBleed = node["AlphaBleed"].as<bool>();
            if (node["MipBias"]) settings.mipBias = node["MipBias"].as<float>();
            if (!std::isfinite(settings.mipBias) || settings.mipBias < -4.0f || settings.mipBias > 4.0f)
                throw std::runtime_error("MipBias must be between -4 and 4");
            return settings;
        }
        catch (const std::exception &error)
        {
            Debug::Warning("Texture import settings '%s': %s; using legacy defaults.", path.c_str(), error.what());
            return {};
        }
    }

    bool SaveTextureImportSettings(const std::string &assetPath,
                                   const TextureImportSettings &settings,
                                   std::string *error)
    {
        if (error) error->clear();
        if (!FilterName(settings.filter) || !WrapName(settings.wrapU) || !WrapName(settings.wrapV))
            return Fail(error, "Invalid texture filter or wrap mode.");
        if (!std::isfinite(settings.mipBias) || settings.mipBias < -4.0f || settings.mipBias > 4.0f)
            return Fail(error, "Mip bias must be between -4 and 4.");
        const std::filesystem::path path(assetPath + ".meta");
        std::filesystem::path temporary;
        try
        {
            YAML::Node root;
            if (std::filesystem::exists(path))
            {
                root = YAML::LoadFile(path.string());
                if (!root.IsMap()) return Fail(error, "Existing texture metadata must be a map; it was left unchanged.");
                if (root["TextureImport"] && !root["TextureImport"].IsMap())
                    return Fail(error, "Existing TextureImport metadata must be a map; it was left unchanged.");
            }
            else
            {
                // Metadata can be authored before the asset scanner first visits
                // a source image. Use the same schema and timestamp units.
                SDL_PathInfo info{};
                if (!SDL_GetPathInfo(assetPath.c_str(), &info) || info.type != SDL_PATHTYPE_FILE)
                    return Fail(error, "Cannot create texture metadata: source image was not found.");
                root["FileType"] = "TEXTURE";
                root["UUID"] = std::to_string(static_cast<uint64_t>(UUID()));
                root["name"] = GetFileName(assetPath);
                root["extension"] = GetFileExtension(assetPath);
                root["size"] = info.size;
                root["modified"] = info.modify_time;
            }

            YAML::Node node = root["TextureImport"];
            node["Filter"] = FilterName(settings.filter);
            node["WrapU"] = WrapName(settings.wrapU);
            node["WrapV"] = WrapName(settings.wrapV);
            node["Mipmaps"] = settings.mipmaps;
            node["AlphaBleed"] = settings.alphaBleed;
            node["MipBias"] = settings.mipBias;
            root["TextureImport"] = node;

            // A sibling temporary plus rename keeps interrupted Apply operations
            // from truncating an existing UUID or its import settings.
            static std::atomic<uint64_t> serial{0};
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            temporary = path.string() + ".tmp-" + std::to_string(stamp) + "-" + std::to_string(++serial);
            {
                std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
                if (!output) return Fail(error, "Cannot write texture metadata temporary file.");
                output << root;
                output.flush();
                if (!output) throw std::runtime_error("Failed while writing texture metadata.");
                output.close();
                if (!output) throw std::runtime_error("Failed while closing texture metadata.");
            }
            std::filesystem::rename(temporary, path);
            return true;
        }
        catch (const std::exception &exception)
        {
            if (!temporary.empty())
            {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
            return Fail(error, exception.what());
        }
    }

    void ApplyTextureAlphaBleed(DecodedImage &image, int maxDistance)
    {
        if (maxDistance <= 0 || image.width <= 0 || image.height <= 0) return;
        const size_t width = static_cast<size_t>(image.width);
        const size_t height = static_cast<size_t>(image.height);
        if (width > std::numeric_limits<size_t>::max() / height / 4) return;
        const size_t count = width * height;
        if (image.pixels.size() != count * 4) return;
        maxDistance = std::min(maxDistance, 64);

        std::vector<unsigned char> distance(count, 0);
        std::vector<size_t> frontier;
        // Seed only the transparent border. Opaque texels and alpha are never
        // modified, and the queue does not retain every opaque source texel.
        for (size_t pixel = 0; pixel < count; ++pixel)
        {
            if (image.pixels[pixel * 4 + 3] != 0) continue;
            const int x = static_cast<int>(pixel % width);
            const int y = static_cast<int>(pixel / width);
            bool found = false;
            for (int dy = -1; dy <= 1 && !found; ++dy)
            {
                if (y + dy < 0 || y + dy >= image.height) continue;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (x + dx < 0 || x + dx >= image.width) continue;
                    const size_t neighbor = static_cast<size_t>(y + dy) * width + static_cast<size_t>(x + dx);
                    if (image.pixels[neighbor * 4 + 3] == 0) continue;
                    std::copy_n(image.pixels.data() + neighbor * 4, 3, image.pixels.data() + pixel * 4);
                    distance[pixel] = 1;
                    frontier.push_back(pixel);
                    found = true;
                    break;
                }
            }
        }

        for (size_t cursor = 0; cursor < frontier.size(); ++cursor)
        {
            const size_t pixel = frontier[cursor];
            if (distance[pixel] >= maxDistance) continue;
            const int x = static_cast<int>(pixel % width);
            const int y = static_cast<int>(pixel / width);
            for (int dy = -1; dy <= 1; ++dy)
            {
                if (y + dy < 0 || y + dy >= image.height) continue;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    if (x + dx < 0 || x + dx >= image.width) continue;
                    const size_t neighbor = static_cast<size_t>(y + dy) * width + static_cast<size_t>(x + dx);
                    if (distance[neighbor] || image.pixels[neighbor * 4 + 3] != 0) continue;
                    std::copy_n(image.pixels.data() + pixel * 4, 3, image.pixels.data() + neighbor * 4);
                    distance[neighbor] = static_cast<unsigned char>(distance[pixel] + 1);
                    frontier.push_back(neighbor);
                }
            }
        }
    }
}
