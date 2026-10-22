#include <Canis/CombineModelLoad.hpp>
#include <Canis/App.hpp>
#include <Canis/Canis.hpp>
#include <Canis/Components.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Scene.hpp>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <cstdlib>

namespace Canis
{
    bool ShouldLoadCombinedModels(const Scene& scene)
    {
        if (!scene.app) return false;
        if (!IsEditorRuntimeEnabled()) return true;
#if CANIS_EDITOR
        auto* editor = scene.app->TryGetEditor();
        return editor && editor->GetMode() != EditorMode::EDIT;
#else
        return true;
#endif
    }

    YAML::Node PrepareCombinedModelNodes(const YAML::Node& authored, bool buildMissing,
                                         std::vector<CombinedModelLoad>& groups)
    {
        groups.clear();
        if (!authored || !authored.IsSequence()) return authored;
        std::unordered_set<uint64_t> used;
        for (auto node : authored) used.insert(node["Entity"].as<uint64_t>(0));
        std::unordered_set<uint64_t> removed;
        YAML::Node generated(YAML::NodeType::Sequence);
        for (auto node : authored)
        {
            const auto component = node["Canis::CombineModel"];
            if (!component || !component["active"].as<bool>(true)) continue;
            const auto id = node["Entity"].as<uint64_t>(0);
            const std::string path = component["cachePath"].as<std::string>("").empty()
                ? "Library/CombinedModels/" + std::to_string(id) + "/model.manifest"
                : component["cachePath"].as<std::string>();
            const float cellSize = component["cellSize"].as<float>(0);
            YAML::Node replacements;
            CombinedModelLoad group;
            group.root = id;
            std::string error;
            bool ready = LoadCombineModelReplacement(authored, id, path, cellSize,
                replacements, group.removed, group.summary, error);
            if (!ready && buildMissing && BuildCombineModel(authored, id, path, cellSize, error))
                ready = LoadCombineModelReplacement(authored, id, path, cellSize,
                    replacements, group.removed, group.summary, error);
            if (ready)
            {
                std::unordered_set<uint64_t> replacementIDs;
                for (auto replacement : replacements)
                {
                    const auto replacementID = replacement["Entity"].as<uint64_t>(0);
                    if (!replacementID || used.contains(replacementID) ||
                        !replacementIDs.insert(replacementID).second)
                    { ready = false; error = "Combined model entity UUID conflicts with the scene."; break; }
                }
                for (auto source : group.removed)
                    if (removed.contains(source))
                    { ready = false; error = "Overlapping CombineModel source groups."; break; }
                if (ready) used.insert(replacementIDs.begin(), replacementIDs.end());
            }
            if (!ready)
            {
                Debug::Warning("COMBINE_MODEL_FALLBACK root=%llu reason=%s",
                    static_cast<unsigned long long>(id), error.c_str());
                continue;
            }
            removed.insert(group.removed.begin(), group.removed.end());
            for (auto replacement : replacements) generated.push_back(YAML::Clone(replacement));
            groups.push_back(std::move(group));
        }
        if (groups.empty()) return authored;
        YAML::Node result(YAML::NodeType::Sequence);
        for (auto node : authored)
        {
            if (removed.contains(node["Entity"].as<uint64_t>(0))) continue;
            auto kept = YAML::Clone(node);
            for (const char* type : {"Canis::Transform", "Canis::RectTransform"})
            {
                if (!kept[type] || !kept[type]["children"].IsSequence()) continue;
                YAML::Node children(YAML::NodeType::Sequence);
                for (auto child : kept[type]["children"])
                    if (!removed.contains(child.as<uint64_t>(0))) children.push_back(child);
                for (auto replacement : generated)
                    if (replacement[type] && replacement[type]["parent"].as<uint64_t>(0) ==
                        kept["Entity"].as<uint64_t>(0)) children.push_back(replacement["Entity"]);
                kept[type]["children"] = children;
            }
            result.push_back(kept);
        }
        for (auto replacement : generated) result.push_back(replacement);
        return result;
    }

    void FinishCombinedModelLoad(Scene& scene, const std::vector<CombinedModelLoad>& groups,
                                size_t authoredCount, size_t loadedCount, double loadMs)
    {
        for (const auto& group : groups)
        {
            Entity root = nullptr;
            scene.GetEntityAfterLoad(UUID(group.root), root);
            if (root)
                if (auto* component = root.TryGetComponent<CombineModel>())
                {
                    component->sourceModels = group.summary.sourceModels;
                    component->combinedModels = group.summary.combinedModels;
                    component->triangleCount = group.summary.triangleCount;
                    component->status = "Loaded cached models; source render entities omitted.";
                }
            Debug::Log("COMBINE_MODEL_LOAD {\"root\":%llu,\"source_models\":%d,\"combined_models\":%d,\"triangles\":%d,\"removed_entities\":%zu,\"authored_entities\":%zu,\"loaded_entities\":%zu,\"load_ms\":%.3f}",
                static_cast<unsigned long long>(group.root), group.summary.sourceModels,
                group.summary.combinedModels, group.summary.triangleCount, group.removed.size(),
                authoredCount, loadedCount, loadMs);
        }
    }

    bool Scene::ActivateCombinedModelsForPlay()
    {
        auto authored = EncodeScene()["Entities"];
        const auto start = SDL_GetTicksNS();
        std::vector<CombinedModelLoad> groups;
        auto prepared = PrepareCombinedModelNodes(authored, true, groups);
        if (groups.empty()) return false;
        std::unordered_set<uint64_t> removed, original;
        for (auto node : authored) original.insert(node["Entity"].as<uint64_t>(0));
        for (const auto& group : groups) removed.insert(group.removed.begin(), group.removed.end());
        // Destroying a Transform also destroys its descendants. Validate this
        // before changing the editor's live scene, including malformed caches.
        for (auto id : removed)
            if (auto* source = GetEntityWithUUID(UUID(id)))
                if (auto* transform = source->TryGetComponent<Transform>())
                    for (auto child : transform->children)
                        if (child && !removed.contains(uint64_t(child.GetUUID())))
                        {
                            Debug::Warning("COMBINE_MODEL_FALLBACK root=%llu reason=retained child under removed group",
                                static_cast<unsigned long long>(id));
                            return false;
                        }
        for (auto id : removed)
            if (auto* source = GetEntityWithUUID(UUID(id))) source->Destroy();
        YAML::Node replacements(YAML::NodeType::Sequence);
        for (auto node : prepared)
            if (!original.contains(node["Entity"].as<uint64_t>(0))) replacements.push_back(node);
        LoadEntityNodes(replacements, false);
        FinishCombinedModelLoad(*this, groups, authored.size(),
            authored.size() - removed.size() + replacements.size(), double(SDL_GetTicksNS() - start) / 1e6);
        return true;
    }
}
