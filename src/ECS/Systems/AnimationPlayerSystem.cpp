#include <Canis/ECS/Systems/AnimationPlayerSystem.hpp>

#include <algorithm>
#include <cmath>

#include <Canis/AnimationRuntime.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>

namespace Canis
{
    void AnimationPlayerSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        if (scene == nullptr || scene->app == nullptr)
            return;

        auto animationView = _registry.view<AnimationPlayer>();
        for (const entt::entity entityHandle : animationView)
        {
            AnimationPlayer &animationPlayer = animationView.get<AnimationPlayer>(entityHandle);
            Entity *entity = animationPlayer.entity;
            if (entity == nullptr)
                continue;

            if (!entity->Active())
                continue;

            if (entity->HasComponent<Animator>())
                continue;

            const std::string clipPath = AssetManager::ResolvePath(animationPlayer.clip);
            if (clipPath.empty())
                continue;

            AnimationClipAsset *clip = AssetManager::GetAnimationClip(clipPath);
            if (clip == nullptr)
                continue;

            const float clipLength = std::max(clip->length, 0.0f);
            const float previousSampleTime = animationPlayer.lastEventSampleTime;
            const bool hadPreviousSample = animationPlayer.lastEventSampleValid &&
                animationPlayer.lastEventClipPath == clipPath;

            if (animationPlayer.playing && clipLength > 0.0f)
            {
                animationPlayer.time += _deltaTime * animationPlayer.speed;

                if (animationPlayer.loop)
                {
                    while (animationPlayer.time < 0.0f)
                        animationPlayer.time += clipLength;

                    while (animationPlayer.time >= clipLength)
                        animationPlayer.time -= clipLength;
                }
                else
                {
                    animationPlayer.time = std::clamp(animationPlayer.time, 0.0f, clipLength);
                }
            }

            const float sampleTime = (clipLength <= 0.0f) ? 0.0f :
                (animationPlayer.loop ? animationPlayer.time : std::clamp(animationPlayer.time, 0.0f, clipLength));

            if (hadPreviousSample && clipLength > 0.0f)
            {
                const bool looped = animationPlayer.loop && sampleTime < previousSampleTime;
                DispatchAnimationEventsBetween(*scene->app, *entity, *clip, clipPath, previousSampleTime, sampleTime, looped);
            }

            animationPlayer.lastEventSampleTime = sampleTime;
            animationPlayer.lastEventSampleValid = true;
            animationPlayer.lastEventClipPath = clipPath;
            (void)ApplyAnimationClip(*scene->app, *entity, *clip, sampleTime);
        }
    }
}
