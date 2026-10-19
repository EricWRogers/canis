#pragma once
#include <Canis/Math.hpp>
#include <cmath>
#include <yaml-cpp/yaml.h>

namespace Canis
{
    // World-space overhead volume. Coverage/density belong to each lighting
    // scenario; geometry, noise scale and wind stay shared during a transition.
    struct CloudVolumeSettings
    {
        bool enabled = false;
        float baseHeight = 80, thickness = 60;
        Vector2 center{0}, halfExtents{450};
        float noiseScale = .008f;
        Vector2 windVelocity{2, .7f};
        int steps = 24;
        float extinction = .08f;
        bool operator==(const CloudVolumeSettings&) const = default;

        void Sanitize()
        {
            auto finite = [](float value, float fallback) { return std::isfinite(value) ? value : fallback; };
            baseHeight = glm::clamp(finite(baseHeight, 80), -10000.f, 10000.f);
            thickness = glm::clamp(finite(thickness, 60), 1.f, 2000.f);
            for (int axis = 0; axis < 2; ++axis)
            {
                center[axis] = glm::clamp(finite(center[axis], 0), -100000.f, 100000.f);
                halfExtents[axis] = glm::clamp(finite(halfExtents[axis], 450), 1.f, 20000.f);
                windVelocity[axis] = glm::clamp(finite(windVelocity[axis], axis == 0 ? 2 : .7f), -1000.f, 1000.f);
            }
            noiseScale = glm::clamp(finite(noiseScale, .008f), .0001f, 1.f);
            steps = glm::clamp(steps, 8, 64);
            extinction = glm::clamp(finite(extinction, .08f), .001f, 1.f);
        }
    };

    inline YAML::Node SerializeCloudVolumeSettings(CloudVolumeSettings settings)
    {
        settings.Sanitize();
        YAML::Node node;
        node["Enabled"] = settings.enabled;
        node["BaseHeight"] = settings.baseHeight;
        node["Thickness"] = settings.thickness;
        auto pair = [](Vector2 value) {
            YAML::Node result(YAML::NodeType::Sequence);
            result.push_back(value.x); result.push_back(value.y);
            result.SetStyle(YAML::EmitterStyle::Flow);
            return result;
        };
        node["Center"] = pair(settings.center);
        node["HalfExtents"] = pair(settings.halfExtents);
        node["NoiseScale"] = settings.noiseScale;
        node["WindVelocity"] = pair(settings.windVelocity);
        node["Steps"] = settings.steps;
        node["Extinction"] = settings.extinction;
        return node;
    }

    inline CloudVolumeSettings DeserializeCloudVolumeSettings(const YAML::Node& node)
    {
        CloudVolumeSettings result;
        if (!node || !node.IsMap()) return result;
        result.enabled = node["Enabled"].as<bool>(result.enabled);
        result.baseHeight = node["BaseHeight"].as<float>(result.baseHeight);
        result.thickness = node["Thickness"].as<float>(result.thickness);
        auto pair = [&](const char* key, Vector2 fallback) {
            const auto value = node[key];
            return value && value.IsSequence() && value.size() == 2 ?
                Vector2(value[0].as<float>(fallback.x), value[1].as<float>(fallback.y)) : fallback;
        };
        result.center = pair("Center", result.center);
        result.halfExtents = pair("HalfExtents", result.halfExtents);
        result.noiseScale = node["NoiseScale"].as<float>(result.noiseScale);
        result.windVelocity = pair("WindVelocity", result.windVelocity);
        result.steps = node["Steps"].as<int>(result.steps);
        result.extinction = node["Extinction"].as<float>(result.extinction);
        result.Sanitize();
        return result;
    }
}
