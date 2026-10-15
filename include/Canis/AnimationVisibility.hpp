#pragma once

#include <Canis/Components.hpp>
#include <Canis/AssetManager.hpp>

namespace Canis
{
    // Shared by animation scheduling and both color/shadow visibility tests.
    // Custom source skeletons/layers keep rendering until their deformation can
    // be bounded safely. Ragdolls use their current pose instead of clip bounds.
    inline bool AnimatedModelBounds(entt::registry &registry, entt::entity handle,
        ModelAsset &asset, const Model &model, Vector3 &minimum, Vector3 &maximum)
    {
        if (const auto *material = registry.try_get<Material>(handle))
        {
            auto safeShader = [](i32 id)
            {
                if (id < 0) return true;
                auto *source = AssetManager::GetMaterial(id);
                if (!source) return false;
                if (source->shaderId < 0 || source->shaderId == AssetManager::GetID("assets/shaders/model3d")) return true;
                auto *shader = AssetManager::Get<ShaderAsset>(source->shaderId);
                return shader && shader->SupportsGraphInstancing() && !shader->HasVertexDeformation();
            };
            if (!safeShader(material->materialId)) return false;
            for (i32 slot : material->materialIds)
                if (!safeShader(slot)) return false;
        }
        const auto *animation = registry.try_get<ModelAnimation>(handle);
        const auto *ragdoll = registry.try_get<Ragdoll>(handle);
        if (animation &&
            (!animation->externalLocalNodeMatrices.empty() ||
                (ragdoll && (ragdoll->active || ragdoll->freezePoseOnDisable))))
        {
            if (!animation->poseInitialized || animation->poseModelId != model.modelId ||
                animation->poseGeometryRevision != asset.GetGeometryRevision()) return false;
            return asset.GetPoseBounds(animation->pose, minimum, maximum, model.nodeIndex, model.applyNodeTransform);
        }
        if (animation && ((animation->sourceModelId >= 0 && animation->sourceModelId != model.modelId) || !animation->layers.empty()))
            return false;
        return asset.GetAnimationBounds(minimum, maximum, model.nodeIndex, model.applyNodeTransform);
    }
}
