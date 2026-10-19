#pragma once
#include <Canis/Math.hpp>
#include <cstdint>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace Canis
{
    // IDs are persistent references. Names can change without breaking transitions
    // or baked channels.
    struct LightingScenario
    {
        std::string id, name;
        Vector4 ambientLight{.15f, .15f, .15f, 1.f};
        float ambientIntensity = .4f;
        bool sunEnabled = true;
        Vector3 sunDirection{0, -1, 0};
        Vector4 sunColor{1};
        float sunIntensity = 1;
        uint64_t skyboxUUID = 0;
        std::string skyboxPath;
        float skyboxIntensity = 1;
        float cloudCoverage = 0;
        float cloudDensity = 1;
        std::string manifestPath, sourceFingerprint, settingsFingerprint;
        std::vector<std::string> probeCachePaths;
    };
    struct LightingScenarioBlend
    {
        const LightingScenario* from = nullptr;
        const LightingScenario* to = nullptr;
        float blend = 0;
    };
    YAML::Node SerializeLightingScenario(const LightingScenario& _scenario);
    LightingScenario DeserializeLightingScenario(const YAML::Node& _node);
    // Includes static geometry, source buffers/textures, materials, baked local
    // lights and probe layout. Scenario overrides and moving objects do not
    // invalidate the shared geometry fingerprint.
    std::string LightingScenarioSourceFingerprint(const YAML::Node& _scene);
    std::string LightingScenarioSettingsFingerprint(const LightingScenario& _scenario);
    // Metadata only: safe for UI/runtime signatures; does not read any asset files.
    std::string LightingScenarioAuthoringFingerprint(const LightingScenario& _scenario);
    YAML::Node RestoreLightingScenarioSources(const YAML::Node& _scene, bool _missingOnly = false);
    // IO validation for an explicit prepare/transition action or asynchronous editor status check.
    // Do not call from the per-frame draw path. Scene/source staleness is checked separately.
    bool LightingScenarioDataAvailable(const LightingScenario& _scenario, std::string& _error,
                                       bool _requireProbes = true);
} // namespace Canis
