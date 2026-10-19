#pragma once
#include <Canis/LightingScenario.hpp>
#include <Canis/Math.hpp>
#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
namespace Canis
{
    // Serialized metadata for native offline direct-light baking (including spot
    // cones).
    struct BakeLight
    {
        static constexpr const char* ScriptName = "Canis::BakeLight";
        class Entity* entity = nullptr;
        int type = 2;
        float spotAngle = 30, innerAngle = 20;
        bool enabled = true;
    };
    struct LightingBakeOptions
    {
        float vertexSpacing = .4f;
        int indirectSamples = 8;
        float indirectIntensity = .7f;
        float ambientOcclusionDistance = 4;
        // Empty evaluates every definition; otherwise only these IDs are evaluated,
        // preserving compatible valid channels for other scenarios from the preceding
        // generation.
        std::vector<std::string> selectedScenarioIds;
        std::atomic<bool>* cancel = nullptr;
    };
    struct LightingBakeResult
    {
        YAML::Node scene;
        std::vector<std::string> models;
        size_t triangles = 0, vertices = 0, lights = 0;
    };
    struct LightingScenarioBakeResult : LightingBakeResult
    {
        std::vector<LightingScenario> scenarios;
        std::string manifestPath;
    };
    // Runs entirely in native code; the input is a detached scene snapshot.
    LightingBakeResult BakeSceneLighting(YAML::Node _scene, const std::filesystem::path& _directory,
                                         const LightingBakeOptions& _options = {},
                                         const std::function<void(const std::string&)>& _progress = {});
    // A completed generation is published atomically. Failed/canceled jobs leave
    // the last good generation and its manifest untouched. All channels share one
    // subdivided mesh per entity.
    LightingScenarioBakeResult BakeLightingScenarios(
        YAML::Node _scene, const std::filesystem::path& _directory,
        const std::vector<LightingScenario>& _scenarios, const LightingBakeOptions& _options = {},
        const std::function<void(const std::string&)>& _progress = {});
    YAML::Node CaptureLightingBakeScene(class Scene& _scene);
    void ApplyLightingBake(class Scene& _scene, const LightingBakeResult& _result);
    void RegisterBakeLight(class App& _app);
} // namespace Canis
