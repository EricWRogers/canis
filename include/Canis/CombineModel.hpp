#pragma once

#include <Canis/Asset.hpp>
#include <Canis/Yaml.hpp>

#include <cstdint>
#include <string>
#include <unordered_set>

namespace Canis
{
    class App;
    class Entity;

    struct CombineModelSummary
    {
        int sourceModels = 0;
        int combinedModels = 0;
        int triangleCount = 0;
        int skippedEntities = 0;
    };

    // Build is an authoring operation and requires the current GL context to
    // import source models. Play only validates and reads the compiled cache.
    bool BuildCombineModel(const YAML::Node& entities, uint64_t rootId,
                           const std::string& cachePath, float cellSize,
                           std::string& error);
    bool LoadCombineModelReplacement(const YAML::Node& entities, uint64_t rootId,
                                    const std::string& cachePath, float cellSize,
                                    YAML::Node& replacements,
                                    std::unordered_set<uint64_t>& removed,
                                    CombineModelSummary& summary,
                                    std::string& error);
    bool RebuildCombineModel(Entity& entity, std::string& error);
    void RegisterCombineModel(App& app);

    // CPU cache decoder shared by ModelAsset::Load and native regression tests.
    // Texture paths are resolved in the current asset library, never stored as
    // process-local numeric IDs in the compiled file.
    bool ReadCombinedModelGeometry(const std::string& path,
                                   std::vector<ModelAsset::PrimitiveBuild3D>& primitives,
                                   std::vector<std::string>& materialSlotNames);
}
