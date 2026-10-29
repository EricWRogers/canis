#include <Canis/ECS/Systems/ModelAnimation3DSystem.hpp>

#include <cmath>
#include <algorithm>

#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Time.hpp>
#include <Canis/AnimationVisibility.hpp>
#include <Canis/Frustum.hpp>
#include <Canis/RenderMetrics.hpp>
#include <Canis/Window.hpp>

namespace Canis
{
    void ModelAnimation3DSystem::Ready()
    {
        // No cached views required.
    }

    void ModelAnimation3DSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        const float deltaTime = std::isfinite(_deltaTime) ? std::max(_deltaTime, 0.0f) : 0.0f;

        // Use the live gameplay camera so a turn/teleport refreshes newly visible
        // poses in this update, rather than one frame after renderer feedback.
        Matrix4 clip(1);
        bool hasView = false;
        if (scene && window && !scene->GetVRCamera().enabled && !scene->HasEditorCamera3DOverride())
        {
            Camera *camera = nullptr;
            Transform *cameraTransform = nullptr;
            auto cameras = _registry.view<Camera, Transform>();
            for (auto handle : cameras)
            {
                auto &candidate = cameras.get<Camera>(handle);
                auto &transform = cameras.get<Transform>(handle);
                if (!transform.IsActiveInHierarchy()) continue;
                if (!camera || candidate.primary)
                {
                    camera = &candidate; cameraTransform = &transform;
                    if (candidate.primary) break;
                }
            }
            if (camera)
            {
                const float aspect = window->GetScreenHeight() > 0 ?
                    float(window->GetScreenWidth()) / window->GetScreenHeight() : 1.0f;
                const auto eye = cameraTransform->GetGlobalPosition();
                clip = glm::perspective(DEG2RAD * camera->fovDegrees, aspect, camera->nearClip, camera->farClip) *
                    glm::lookAt(eye, eye + cameraTransform->GetForward(), cameraTransform->GetUp());
                hasView = true;
            }
        }

        auto animationView = _registry.view<Model, ModelAnimation>();
        for (const entt::entity entityHandle : animationView)
        {
            Model &modelRenderer = animationView.get<Model>(entityHandle);
            ModelAnimation &modelAnimation = animationView.get<ModelAnimation>(entityHandle);

            Entity *entity = modelRenderer.entity;
            if (entity == nullptr)
                entity = modelAnimation.entity;

            if (entity == nullptr || !entity->Active())
                continue;

            if (modelRenderer.modelId < 0)
                continue;

            const i32 animationModelId =
                modelAnimation.sourceModelId >= 0
                    ? modelAnimation.sourceModelId
                    : modelRenderer.modelId;
            ModelAsset *model = AssetManager::GetModel(animationModelId);
            if (model == nullptr)
                continue;

            if (modelAnimation.poseModelId != animationModelId ||
                modelAnimation.poseGeometryRevision != model->GetGeometryRevision())
            {
                modelAnimation.poseModelId = animationModelId;
                modelAnimation.poseGeometryRevision = model->GetGeometryRevision();
                modelAnimation.poseInitialized = false;
                modelAnimation.poseUpdateAccumulator = 0.0;
                modelAnimation.lastEvaluatedAnimationIndex = -1;
                modelAnimation.lastEvaluatedAnimationTime = 0.0f;
                modelAnimation.lastEvaluatedRootMotionMask = Vector3(0.0f);
            }

            if (!modelAnimation.externalLocalNodeMatrices.empty())
            {
                if (!modelAnimation.poseInitialized) model->ResetPose(modelAnimation.pose);
                if (model->BlendPoseFromLocalMatrices(modelAnimation.pose, modelAnimation.externalLocalNodeMatrices, 0.0f))
                {
                    modelAnimation.poseInitialized = true;
                    // Force resampling the clip when external tracking ends,
                    // even when its animation time has not changed.
                    modelAnimation.lastEvaluatedAnimationIndex = -1;
                    continue;
                }
                modelAnimation.poseInitialized = false;
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

            const bool animatorOwnsPlayback =
                entity->HasComponent<Animator>() &&
                entity->GetComponent<Animator>().drivesModelAnimation;
            if (!animatorOwnsPlayback &&
                modelAnimation.playAnimation &&
                animationDuration > 0.0f)
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
                (std::fabs(modelAnimation.lastEvaluatedAnimationTime - modelAnimation.animationTime) > 1e-6f) ||
                (glm::length(
                    modelAnimation.lastEvaluatedRootMotionMask -
                    modelAnimation.rootMotionMask) > 1e-6f);

            const bool transitionActive =
                modelAnimation.transitionDuration > 0.0f &&
                !modelAnimation.transitionLocalNodeMatrices.empty();
            if (transitionActive && modelAnimation.playAnimation)
                modelAnimation.transitionElapsed += std::max(deltaTime, 0.0f);

            // Advance clocks above every frame, but sample/skin only at the requested
            // cadence. Preserve fractional time and coalesce missed ticks at low FPS.
            // Clip changes, invalidated poses and editor scrubbing refresh immediately.
            bool offscreen = false;
            // Editor previews and VR keep full-rate evaluation: more than one
            // camera may consume the pose. External/ragdoll poses bypass above.
            if (hasView && deltaTime > 0 && RenderMetrics::CullingEnabled() &&
                modelAnimation.offscreenUpdateRate > 0 && std::isfinite(modelAnimation.offscreenUpdateRate))
                if (auto *transform = _registry.try_get<Transform>(entityHandle))
                {
                    Vector3 minimum, maximum;
                    if (AnimatedModelBounds(_registry, entityHandle, *model, modelRenderer, minimum, maximum))
                        offscreen = BoundsOutsideFrustum(clip * transform->GetModelMatrix(), minimum, maximum);
                }
            const bool becameVisible = modelAnimation.wasOffscreen && !offscreen;
            modelAnimation.wasOffscreen = offscreen;
            const bool forceEvaluation = !modelAnimation.poseInitialized || becameVisible ||
                (!offscreen && (modelAnimation.lastEvaluatedAnimationIndex != modelAnimation.animationIndex ||
                    glm::length(modelAnimation.lastEvaluatedRootMotionMask - modelAnimation.rootMotionMask) > 1e-6f)) ||
                deltaTime <= 0.0f;
            float rate = modelAnimation.updateRate;
            if (offscreen)
                rate = rate > 0 && std::isfinite(rate) ? std::min(rate, modelAnimation.offscreenUpdateRate) : modelAnimation.offscreenUpdateRate;
            if (rate > 0.0f && std::isfinite(rate))
            {
                const double interval = 1.0 / rate;
                modelAnimation.poseUpdateAccumulator += std::max(deltaTime, 0.0f);
                if (!forceEvaluation && modelAnimation.poseUpdateAccumulator + 1e-7 < interval)
                    continue;
                modelAnimation.poseUpdateAccumulator = forceEvaluation ? 0.0 :
                    std::max(0.0, modelAnimation.poseUpdateAccumulator -
                        interval * std::floor((modelAnimation.poseUpdateAccumulator + 1e-7) / interval));
            }
            else
                modelAnimation.poseUpdateAccumulator = 0.0;

            if (!modelAnimation.poseInitialized || animationChanged ||
                transitionActive || !modelAnimation.layers.empty())
            {
                if (!model->UpdateAnimation(
                        modelAnimation.pose,
                        modelAnimation.animationIndex,
                        modelAnimation.animationTime,
                        modelAnimation.rootMotionMask))
                    model->ResetPose(modelAnimation.pose);

                if (transitionActive)
                {
                    const float targetWeight = std::clamp(
                        modelAnimation.transitionElapsed /
                            modelAnimation.transitionDuration,
                        0.0f,
                        1.0f);
                    if (!model->BlendPoseFromLocalMatrices(
                            modelAnimation.pose,
                            modelAnimation.transitionLocalNodeMatrices,
                            targetWeight) ||
                        targetWeight >= 1.0f)
                    {
                        modelAnimation.transitionLocalNodeMatrices.clear();
                        modelAnimation.transitionDuration = 0.0f;
                        modelAnimation.transitionElapsed = 0.0f;
                    }
                }

                for (const ModelAnimation::LayerSample& layer : modelAnimation.layers)
                {
                    if (layer.weight <= 0.0f)
                        continue;
                    ModelAsset* layerModel = AssetManager::GetModel(
                        layer.sourceModelId >= 0 ? layer.sourceModelId : animationModelId);
                    if (layerModel == nullptr || layerModel->GetAnimationCount() <= 0)
                        continue;
                    ModelAsset::Pose3D layerPose = {};
                    const i32 layerAnimationIndex = std::clamp(
                        layer.animationIndex, 0, layerModel->GetAnimationCount() - 1);
                    if (!layerModel->UpdateAnimation(
                            layerPose,
                            layerAnimationIndex,
                            layer.animationTime,
                            Vector3(1.0f)))
                        continue;
                    if (layer.transitionSourceModelId >= 0 &&
                        layer.transitionAnimationIndex >= 0 &&
                        layer.transitionTargetWeight < 1.0f)
                    {
                        ModelAsset* previousModel = AssetManager::GetModel(layer.transitionSourceModelId);
                        ModelAsset::Pose3D previousPose = {};
                        if (previousModel != nullptr && previousModel->UpdateAnimation(
                                previousPose,
                                layer.transitionAnimationIndex,
                                layer.transitionAnimationTime,
                                Vector3(1.0f)))
                        {
                            layerModel->BlendPoseFromLocalMatrices(
                                layerPose,
                                previousPose.localNodeMatrices,
                                layer.transitionTargetWeight);
                        }
                    }
                    model->BlendPoseLayer(
                        modelAnimation.pose,
                        layerPose.localNodeMatrices,
                        layer.weight,
                        layer.maskRoots,
                        layer.maskBones,
                        layer.maskWeights,
                        layer.additive);
                }

                modelAnimation.poseInitialized = true;
                modelAnimation.lastEvaluatedAnimationIndex = modelAnimation.animationIndex;
                modelAnimation.lastEvaluatedAnimationTime = modelAnimation.animationTime;
                modelAnimation.lastEvaluatedRootMotionMask =
                    modelAnimation.rootMotionMask;
            }
        }
    }
} // end of Canis namespace
