#include <Canis/ECS/Systems/ModelAnimation3DSystem.hpp>

#include <cmath>
#include <algorithm>

#include <Canis/AssetManager.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Time.hpp>

namespace Canis
{
    void ModelAnimation3DSystem::Ready()
    {
        // No cached views required.
    }

    void ModelAnimation3DSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        const float deltaTime = _deltaTime;

        auto animationView = _registry.view<Model, ModelAnimation>();
        for (const entt::entity entityHandle : animationView)
        {
            Model &modelRenderer = animationView.get<Model>(entityHandle);
            ModelAnimation &modelAnimation = animationView.get<ModelAnimation>(entityHandle);

            Entity *entity = modelRenderer.entity;
            if (entity == nullptr)
                entity = modelAnimation.entity;

            if (entity == nullptr || !entity->active)
                continue;

            if (modelRenderer.modelId < 0)
                continue;

            ModelAsset *model = AssetManager::GetModel(modelRenderer.modelId);
            if (model == nullptr)
                continue;

            if (modelAnimation.poseModelId != modelRenderer.modelId)
            {
                modelAnimation.poseModelId = modelRenderer.modelId;
                modelAnimation.poseInitialized = false;
                modelAnimation.lastEvaluatedAnimationIndex = -1;
                modelAnimation.lastEvaluatedAnimationTime = 0.0f;
            }

            const i32 animationCount = model->GetAnimationCount();
            if (animationCount <= 0)
            {
                if (!modelAnimation.poseInitialized)
                {
                    model->ResetPose(modelAnimation.pose);
                    modelAnimation.poseInitialized = true;
                }
                continue;
            }

            modelAnimation.animationIndex = std::clamp(modelAnimation.animationIndex, 0, animationCount - 1);
            const float animationDuration = model->GetAnimationDuration(modelAnimation.animationIndex);

            if (modelAnimation.playAnimation && animationDuration > 0.0f)
            {
                modelAnimation.animationTime += deltaTime * modelAnimation.animationSpeed;

                if (modelAnimation.loop)
                {
                    while (modelAnimation.animationTime < 0.0f)
                        modelAnimation.animationTime += animationDuration;

                    while (modelAnimation.animationTime >= animationDuration)
                        modelAnimation.animationTime -= animationDuration;
                }
                else
                {
                    modelAnimation.animationTime = std::clamp(modelAnimation.animationTime, 0.0f, animationDuration);
                }
            }

            const bool animationChanged =
                (modelAnimation.lastEvaluatedAnimationIndex != modelAnimation.animationIndex) ||
                (std::fabs(modelAnimation.lastEvaluatedAnimationTime - modelAnimation.animationTime) > 1e-6f);

            if (!modelAnimation.poseInitialized || animationChanged)
            {
                if (!model->UpdateAnimation(modelAnimation.pose, modelAnimation.animationIndex, modelAnimation.animationTime))
                    model->ResetPose(modelAnimation.pose);

                modelAnimation.poseInitialized = true;
                modelAnimation.lastEvaluatedAnimationIndex = modelAnimation.animationIndex;
                modelAnimation.lastEvaluatedAnimationTime = modelAnimation.animationTime;
            }
        }
    }
} // end of Canis namespace
