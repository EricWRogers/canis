#pragma once

#include <algorithm>
#include <cmath>
#include <yaml-cpp/yaml.h>

namespace Canis
{
    // Only renderers explicitly marked as weather surfaces use these controls.
    // The amount can be animated independently of the authored material assets.
    struct SurfaceWetnessSettings
    {
        bool enabled = false;
        float amount = 0.0f;
        float dryRoughness = .85f;
        float wetRoughness = .22f;
        float darkening = .22f;
        bool operator==(const SurfaceWetnessSettings&) const = default;

        void Sanitize()
        {
            auto finite = [](float value, float fallback) { return std::isfinite(value) ? value : fallback; };
            amount = std::clamp(finite(amount, 0), 0.f, 1.f);
            dryRoughness = std::clamp(finite(dryRoughness, .85f), .04f, 1.f);
            wetRoughness = std::clamp(finite(wetRoughness, .22f), .04f, 1.f);
            darkening = std::clamp(finite(darkening, .22f), 0.f, 1.f);
        }
    };

    inline YAML::Node SerializeSurfaceWetnessSettings(SurfaceWetnessSettings settings)
    {
        settings.Sanitize();
        YAML::Node node;
        node["Enabled"] = settings.enabled;
        node["Amount"] = settings.amount;
        node["DryRoughness"] = settings.dryRoughness;
        node["WetRoughness"] = settings.wetRoughness;
        node["Darkening"] = settings.darkening;
        return node;
    }

    inline SurfaceWetnessSettings DeserializeSurfaceWetnessSettings(const YAML::Node& node)
    {
        SurfaceWetnessSettings result;
        if (!node || !node.IsMap())
            return result;
        result.enabled = node["Enabled"].as<bool>(result.enabled);
        result.amount = node["Amount"].as<float>(result.amount);
        result.dryRoughness = node["DryRoughness"].as<float>(result.dryRoughness);
        result.wetRoughness = node["WetRoughness"].as<float>(result.wetRoughness);
        result.darkening = node["Darkening"].as<float>(result.darkening);
        result.Sanitize();
        return result;
    }
} // namespace Canis
