#include <Canis/LOD.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <chrono>

namespace Canis
{
    namespace
    {
        struct Entry
        {
            std::shared_ptr<LODProfile> profile;
            std::filesystem::file_time_type modified{};
            std::chrono::steady_clock::time_point checked{};
        };
        std::unordered_map<std::string, Entry> profiles;
        std::string ModelPath(const YAML::Node& node)
        {
            if (node.IsScalar()) return node.as<std::string>();
            if (node["uuid"])
            {
                auto path = AssetManager::GetPath(UUID(node["uuid"].as<uint64_t>(0)));
                if (path.rfind("Path was not found", 0) != 0) return path;
            }
            return node["path"].as<std::string>("");
        }
    }
    std::shared_ptr<LODProfile> GetLODProfile(const std::string& path, bool reload)
    {
        if (path.empty()) return {};
        auto& entry = profiles[path];
        auto now = std::chrono::steady_clock::now();
        if (!reload && now - entry.checked < std::chrono::seconds(1)) return entry.profile;
        entry.checked = now;
        std::error_code error;
        auto modified = std::filesystem::last_write_time(path, error);
        if (!reload && entry.profile && !error && modified == entry.modified) return entry.profile;
        try
        {
            auto root = YAML::LoadFile(path);
            auto profile = std::make_shared<LODProfile>();
            profile->path = path;
            profile->revision = entry.profile ? entry.profile->revision + 1 : 1;
            for (auto source : root["chains"])
            {
                LODChain chain;
                chain.sourcePath = ModelPath(source["source"]);
                if (chain.sourcePath.empty() || !std::filesystem::exists(chain.sourcePath)) throw std::runtime_error("Missing LOD source model");
                chain.sourceId = AssetManager::LoadModel(chain.sourcePath);
                if(chain.sourceId<0)throw std::runtime_error("Invalid LOD source model");
                float previous = 2;
                for (auto item : source["levels"])
                {
                    LODLevel level;
                    level.modelPath = ModelPath(item["model"]);
                    level.screenHeight = item["screenHeight"].as<float>(0);
                    if (!std::isfinite(level.screenHeight) || level.screenHeight < 0 || level.screenHeight >= previous) throw std::runtime_error("LOD thresholds must be finite, nonnegative and descending");
                    if (level.modelPath.empty() || !std::filesystem::exists(level.modelPath)) throw std::runtime_error("Missing LOD level model");
                    level.modelId = AssetManager::LoadModel(level.modelPath);
                    if(level.modelId<0)throw std::runtime_error("Invalid LOD model");
                    chain.levels.push_back(level);previous = level.screenHeight;
                }
                if (chain.levels.empty() || chain.levels.front().modelId != chain.sourceId) throw std::runtime_error("LOD0 must be the authored source model");
                profile->chains.push_back(std::move(chain));
            }
            if (profile->chains.empty()) throw std::runtime_error("Empty LOD profile");
            entry.profile = profile;entry.modified = modified;
        }
        catch (const std::exception& exception)
        {
            Debug::Warning("LOD profile '%s': %s; retaining the last valid profile or original model.", path.c_str(), exception.what());
        }
        return entry.profile;
    }
    bool LODProfile::Save() const
    {
        for(const auto& chain:chains) {
            if(chain.levels.empty() || chain.levels.front().modelId!=chain.sourceId)return false;
            float previous=2;
            for(const auto& level:chain.levels) {
                if(!std::isfinite(level.screenHeight) || level.screenHeight<0 || level.screenHeight>=previous || level.modelId<0)return false;
                previous=level.screenHeight;
            }
        }
        YAML::Node root;
        for (const auto& chain : chains)
        {
            YAML::Node item;item["source"]["path"] = chain.sourcePath;
            for (const auto& level : chain.levels)
            {
                YAML::Node data;data["model"]["path"] = level.modelPath;data["screenHeight"] = level.screenHeight;
                item["levels"].push_back(data);
            }
            root["chains"].push_back(item);
        }
        std::ofstream output(path);output << root;
        return output.good();
    }
    int SelectLOD(const std::vector<LODLevel>& levels, float height, int previous, float hysteresis)
    {
        if (levels.empty()) return 0;
        height = std::max(0.f, height);
        int result = static_cast<int>(levels.size()) - 1;
        for (int i = 0; i < static_cast<int>(levels.size()); ++i)
            if (height >= levels[i].screenHeight) { result = i;break; }
        if (previous < 0 || previous >= static_cast<int>(levels.size())) return result;
        hysteresis = std::clamp(hysteresis, 0.f, .49f);
        if (result > previous && height >= levels[previous].screenHeight * (1 - hysteresis)) return previous;
        if (result < previous && height <= levels[result].screenHeight * (1 + hysteresis)) return previous;
        return result;
    }
    float ProjectedLODHeight(const Vector3& minimum, const Vector3& maximum,
        const Matrix4& model, const Matrix4& view, const Matrix4& projection)
    {
        Vector3 center = Vector3(model * Vector4((minimum + maximum) * .5f, 1));
        float radius = 0;
        for (int corner = 0; corner < 8; ++corner)
        {
            Vector3 point(model * Vector4(corner & 1 ? maximum.x : minimum.x, corner & 2 ? maximum.y : minimum.y, corner & 4 ? maximum.z : minimum.z, 1));
            radius = std::max(radius, glm::length(point - center));
        }
        float depth = -(view * Vector4(center, 1)).z;
        // A sphere crossing the camera/near plane always retains full detail.
        if (depth <= radius + .01f) return 1;
        float height = std::abs(projection[1][1]) * radius;
        if (std::abs(projection[3][3]) < .5f) height /= depth;
        return std::isfinite(height) ? std::clamp(height, 0.f, 1.f) : 1;
    }
}
