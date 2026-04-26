#include <Canis/ECS/Systems/AnimatorSystem.hpp>

#include <algorithm>
#include <cmath>

#include <Canis/AnimationRuntime.hpp>
#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Scene.hpp>

namespace Canis
{
    namespace
    {
        const AnimatorState* FindAnimatorState(const AnimatorControllerAsset& _controller, const std::string& _name)
        {
            for (const AnimatorState& state : _controller.states)
            {
                if (state.name == _name)
                    return &state;
            }

            return nullptr;
        }

        AnimationValue MakeDefaultAnimatorParameterValue(AnimatorParameterType _type)
        {
            switch (_type)
            {
                case AnimatorParameterType::INT: return AnimationValue::Int(0);
                case AnimatorParameterType::BOOL:
                case AnimatorParameterType::TRIGGER: return AnimationValue::Bool(false);
                case AnimatorParameterType::FLOAT:
                default: return AnimationValue::Float(0.0f);
            }
        }

        void SyncAnimatorParameters(Animator& _animator, const AnimatorControllerAsset& _controller)
        {
            for (const AnimatorParameterDefinition& definition : _controller.parameters)
            {
                AnimatorParameterRuntime* runtimeParameter = _animator.GetParameter(definition.name);
                if (runtimeParameter == nullptr)
                {
                    AnimatorParameterRuntime created = {};
                    created.name = definition.name;
                    created.value = definition.defaultValue.type == AnimationValueType::NONE
                        ? MakeDefaultAnimatorParameterValue(definition.type)
                        : definition.defaultValue;
                    created.triggerActive = false;
                    _animator.parameters.push_back(created);
                    continue;
                }

                if (!_animator.parametersInitialized)
                {
                    runtimeParameter->value = definition.defaultValue.type == AnimationValueType::NONE
                        ? MakeDefaultAnimatorParameterValue(definition.type)
                        : definition.defaultValue;
                    runtimeParameter->triggerActive = false;
                }
            }

            _animator.parametersInitialized = true;
        }

        bool AnimatorConditionMatches(const AnimatorTransitionCondition& _condition, const Animator& _animator)
        {
            const AnimatorParameterRuntime* runtimeParameter = _animator.GetParameter(_condition.parameter);
            if (runtimeParameter == nullptr)
                return false;

            switch (_condition.mode)
            {
                case AnimatorConditionMode::GREATER:
                    return runtimeParameter->value.AsFloat() > _condition.value.AsFloat();
                case AnimatorConditionMode::LESS:
                    return runtimeParameter->value.AsFloat() < _condition.value.AsFloat();
                case AnimatorConditionMode::EQUAL:
                {
                    if (runtimeParameter->value.type == AnimationValueType::INT || _condition.value.type == AnimationValueType::INT)
                        return runtimeParameter->value.AsInt() == _condition.value.AsInt();

                    if (runtimeParameter->value.type == AnimationValueType::BOOL || _condition.value.type == AnimationValueType::BOOL)
                        return runtimeParameter->value.AsBool() == _condition.value.AsBool();

                    return std::fabs(runtimeParameter->value.AsFloat() - _condition.value.AsFloat()) <= 0.0001f;
                }
                case AnimatorConditionMode::NOT_EQUAL:
                {
                    if (runtimeParameter->value.type == AnimationValueType::INT || _condition.value.type == AnimationValueType::INT)
                        return runtimeParameter->value.AsInt() != _condition.value.AsInt();

                    if (runtimeParameter->value.type == AnimationValueType::BOOL || _condition.value.type == AnimationValueType::BOOL)
                        return runtimeParameter->value.AsBool() != _condition.value.AsBool();

                    return std::fabs(runtimeParameter->value.AsFloat() - _condition.value.AsFloat()) > 0.0001f;
                }
                case AnimatorConditionMode::IF_TRUE:
                    return runtimeParameter->value.AsBool();
                case AnimatorConditionMode::IF_FALSE:
                    return !runtimeParameter->value.AsBool();
                case AnimatorConditionMode::TRIGGERED:
                    return runtimeParameter->triggerActive;
                default:
                    return false;
            }
        }

        void ConsumeAnimatorTransitionTriggers(const AnimatorTransition& _transition, Animator& _animator)
        {
            for (const AnimatorTransitionCondition& condition : _transition.conditions)
            {
                if (condition.mode != AnimatorConditionMode::TRIGGERED)
                    continue;

                if (AnimatorParameterRuntime* runtimeParameter = _animator.GetParameter(condition.parameter))
                {
                    runtimeParameter->triggerActive = false;
                    runtimeParameter->value = AnimationValue::Bool(false);
                }
            }
        }
    }

    void AnimatorSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        if (scene == nullptr || scene->app == nullptr)
            return;

        auto animatorView = _registry.view<Animator>();
        for (const entt::entity entityHandle : animatorView)
        {
            Animator& animator = animatorView.get<Animator>(entityHandle);
            Entity* entity = animator.entity;
            if (entity == nullptr || !entity->active)
                continue;

            const std::string controllerPath = AssetManager::ResolvePath(animator.controller);
            if (controllerPath.empty())
                continue;

            AnimatorControllerAsset* controller = AssetManager::GetAnimatorController(controllerPath);
            if (controller == nullptr || controller->states.empty())
                continue;

            SyncAnimatorParameters(animator, *controller);

            if (animator.currentState.empty())
                animator.currentState = controller->entryState.empty() ? controller->states.front().name : controller->entryState;

            const AnimatorState* state = FindAnimatorState(*controller, animator.currentState);
            if (state == nullptr)
            {
                animator.currentState = controller->entryState.empty() ? controller->states.front().name : controller->entryState;
                animator.time = 0.0f;
                animator.lastEventSampleValid = false;
                state = FindAnimatorState(*controller, animator.currentState);
            }

            if (state == nullptr)
                continue;

            std::string activeClipPath = AssetManager::ResolvePath(state->clip);
            AnimationClipAsset* clip = activeClipPath.empty() ? nullptr : AssetManager::GetAnimationClip(activeClipPath);
            const float clipLength = (clip != nullptr) ? std::max(clip->length, 0.0f) : 0.0f;
            const float previousSampleTime = animator.lastEventSampleTime;
            bool hadPreviousSample = animator.lastEventSampleValid &&
                animator.lastEventClipPath == activeClipPath;

            if (animator.playing && clipLength > 0.0f)
            {
                animator.time += _deltaTime * state->speed;

                if (state->loop)
                {
                    while (animator.time < 0.0f)
                        animator.time += clipLength;

                    while (animator.time >= clipLength)
                        animator.time -= clipLength;
                }
                else
                {
                    animator.time = std::clamp(animator.time, 0.0f, clipLength);
                }
            }

            const float normalizedTime = (clipLength > 0.0f) ? std::clamp(animator.time / clipLength, 0.0f, 1.0f) : 1.0f;

            const AnimatorTransition* transitionToTake = nullptr;
            if (animator.playing)
            {
                for (const AnimatorTransition& transition : state->transitions)
                {
                    if (transition.hasExitTime && normalizedTime + 0.0001f < transition.exitTimeNormalized)
                        continue;

                    bool matches = true;
                    for (const AnimatorTransitionCondition& condition : transition.conditions)
                    {
                        if (!AnimatorConditionMatches(condition, animator))
                        {
                            matches = false;
                            break;
                        }
                    }

                    if (matches)
                    {
                        transitionToTake = &transition;
                        break;
                    }
                }
            }

            if (transitionToTake != nullptr && !transitionToTake->toState.empty())
            {
                ConsumeAnimatorTransitionTriggers(*transitionToTake, animator);
                animator.currentState = transitionToTake->toState;
                animator.time = 0.0f;
                animator.lastEventSampleTime = 0.0f;
                animator.lastEventSampleValid = false;
                animator.lastEventClipPath.clear();

                state = FindAnimatorState(*controller, animator.currentState);
                if (state == nullptr)
                    continue;

                activeClipPath = AssetManager::ResolvePath(state->clip);
                clip = activeClipPath.empty() ? nullptr : AssetManager::GetAnimationClip(activeClipPath);
                hadPreviousSample = false;
            }

            if (clip == nullptr)
                continue;

            const float sampleTime = (clip->length <= 0.0f) ? 0.0f :
                (state->loop ? animator.time : std::clamp(animator.time, 0.0f, clip->length));

            if (hadPreviousSample && animator.playing)
            {
                const bool looped = state->loop && sampleTime < previousSampleTime;
                DispatchAnimationEventsBetween(*scene->app, *entity, *clip, activeClipPath, previousSampleTime, sampleTime, looped);
            }

            animator.lastEventSampleTime = sampleTime;
            animator.lastEventSampleValid = true;
            animator.lastEventClipPath = activeClipPath;
            (void)ApplyAnimationClip(*scene->app, *entity, *clip, sampleTime);
        }
    }
}
