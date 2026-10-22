#pragma once
#include <Canis/CombineModel.hpp>
#include <vector>

namespace Canis
{
    class Scene;
    struct CombinedModelLoad
    {
        uint64_t root = 0;
        CombineModelSummary summary;
        std::unordered_set<uint64_t> removed;
    };
    bool ShouldLoadCombinedModels(const Scene& scene);
    YAML::Node PrepareCombinedModelNodes(const YAML::Node& authored, bool buildMissing,
                                         std::vector<CombinedModelLoad>& groups);
    void FinishCombinedModelLoad(Scene& scene, const std::vector<CombinedModelLoad>& groups,
                                size_t authoredCount, size_t loadedCount, double loadMs);
}
