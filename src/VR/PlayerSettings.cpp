#include <Canis/VR/PlayerSettings.hpp>
#include <yaml-cpp/yaml.h>
#include <cmath>
#include <filesystem>
#include <set>
#include <stdexcept>

namespace Canis::VR
{
    bool ValidatePlayerSettings(const PlayerSettings& s, std::string& error)
    {
        auto range = [&](float value, float min, float max, const char* name) {
            if (std::isfinite(value) && value >= min && value <= max) return true;
            error = std::string(name) + " must be between " + std::to_string(min) + " and " + std::to_string(max);
            return false;
        };
        if (s.dominantHand > 1) { error = "dominantHand must be left or right"; return false; }
        return range(s.localFloorHeight,0.5f,2.5f,"localFloorHeight") &&
            range(s.heightOffset,-1.0f,1.0f,"heightOffset") &&
            range(s.snapDegrees,15,90,"snapDegrees") && range(s.teleportRange,0.25f,5,"teleportRange") &&
            range(s.grabReach,0.05f,0.35f,"grabReach") && range(s.fadeOutSeconds,0.05f,0.5f,"fadeOutSeconds") &&
            range(s.blackSeconds,0.02f,0.25f,"blackSeconds") && range(s.fadeInSeconds,0.05f,0.5f,"fadeInSeconds");
    }
    bool LoadPlayerSettings(const std::string& path, PlayerSettings& settings, std::string& error)
    {
        try
        {
            if (!std::filesystem::exists(path)) return true;
            auto document = YAML::LoadFile(path);
            if (!document.IsMap()) throw std::runtime_error("expected a YAML mapping");
            PlayerSettings candidate = settings;
            std::set<std::string> keys;
            for (const auto& entry : document)
            {
                auto key = entry.first.as<std::string>();
                if (!keys.insert(key).second) throw std::runtime_error("duplicate setting: " + key);
                const auto& value = entry.second;
                if (key == "version") { if (value.as<int>() != 1) throw std::runtime_error("unsupported VR settings version"); }
                else if (key == "dominantHand")
                {
                    const auto hand = value.as<std::string>();
                    if (hand != "left" && hand != "right") throw std::runtime_error("dominantHand must be left or right");
                    candidate.dominantHand = hand == "left" ? 0 : 1;
                }
                else if (key == "localFloorHeight") candidate.localFloorHeight = value.as<float>();
                else if (key == "heightOffset") candidate.heightOffset = value.as<float>();
                else if (key == "snapDegrees") candidate.snapDegrees = value.as<float>();
                else if (key == "teleportRange") candidate.teleportRange = value.as<float>();
                else if (key == "grabReach") candidate.grabReach = value.as<float>();
                else if (key == "fadeOutSeconds") candidate.fadeOutSeconds = value.as<float>();
                else if (key == "blackSeconds") candidate.blackSeconds = value.as<float>();
                else if (key == "fadeInSeconds") candidate.fadeInSeconds = value.as<float>();
                else throw std::runtime_error("unknown VR setting: " + key);
            }
            std::string validation;
            if (!ValidatePlayerSettings(candidate, validation)) throw std::runtime_error(validation);
            settings = candidate;
            return true;
        }
        catch (const std::exception& e) { error = path + ": " + e.what(); return false; }
    }
}
