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
            }

            _animator.parametersInitialized = true;
        }

        float GetAnimatorStateSpeed(const AnimatorState& _state, const Animator& _animator)
        {
            const float parameterMultiplier = _state.speedParameter.empty()
                ? 1.0f
                : _animator.GetFloat(_state.speedParameter, 1.0f);
            return _state.speed * parameterMultiplier;
        }

        ModelAsset* GetAnimatorStateModel(const AnimatorState& _state, i32& _outAnimationIndex)
        {
            _outAnimationIndex = 0;
            if (_state.modelPath.empty())
                return nullptr;

            const i32 modelId = AssetManager::LoadModel(_state.modelPath);
            ModelAsset* model = AssetManager::GetModel(modelId);
            if (model == nullptr || model->GetAnimationCount() <= 0)
                return nullptr;

            _outAnimationIndex = std::clamp(
                _state.modelAnimationIndex,
                0,
                model->GetAnimationCount() - 1);
            return model;
        }

        float GetAnimatorStateLength(
            const AnimatorState& _state,
            AnimationClipAsset*& _outClip,
            std::string& _outClipPath)
        {
            _outClip = nullptr;
            _outClipPath.clear();

            if (!_state.modelPath.empty())
            {
                i32 animationIndex = 0;
                if (ModelAsset* model = GetAnimatorStateModel(_state, animationIndex))
                    return std::max(model->GetAnimationDuration(animationIndex), 0.0f);
                return 0.0f;
            }

            _outClipPath = AssetManager::ResolvePath(_state.clip);
            _outClip = _outClipPath.empty()
                ? nullptr
                : AssetManager::GetAnimationClip(_outClipPath);
            return (_outClip != nullptr) ? std::max(_outClip->length, 0.0f) : 0.0f;
        }

        void ApplyAnimatorModelState(
            Entity& _entity,
            Animator& _animator,
            const AnimatorState& _state,
            const AnimatorTransition* _transition,
            float _previousNormalizedTime)
        {
            if (!_entity.HasComponent<Model>() || !_entity.HasComponent<ModelAnimation>())
            {
                _animator.drivesModelAnimation = false;
                return;
            }

            i32 animationIndex = 0;
            ModelAsset* targetModel = GetAnimatorStateModel(_state, animationIndex);
            if (targetModel == nullptr)
            {
                _animator.drivesModelAnimation = false;
                if (_entity.HasComponent<ModelAnimation>())
                    _entity.GetComponent<ModelAnimation>().sourceModelId = -1;
                return;
            }

            ModelAnimation& animation = _entity.GetComponent<ModelAnimation>();
            const float transitionDuration = (_transition != nullptr)
                ? std::max(_transition->duration, 0.0f)
                : 0.0f;

            if (transitionDuration > 0.0f &&
                animation.poseInitialized &&
                !animation.pose.localNodeMatrices.empty())
            {
                animation.transitionLocalNodeMatrices =
                    animation.pose.localNodeMatrices;
                animation.transitionDuration = transitionDuration;
                animation.transitionElapsed = 0.0f;
            }
            else
            {
                animation.transitionLocalNodeMatrices.clear();
                animation.transitionDuration = 0.0f;
                animation.transitionElapsed = 0.0f;
            }

            animation.sourceModelId =
                AssetManager::LoadModel(_state.modelPath);
            animation.animationIndex = animationIndex;
            animation.animationTime = 0.0f;
            if (_transition != nullptr && _transition->preserveNormalizedTime && _state.loop)
            {
                animation.animationTime =
                    std::clamp(_previousNormalizedTime, 0.0f, 1.0f) *
                    std::max(targetModel->GetAnimationDuration(animationIndex), 0.0f);
            }
            animation.animationSpeed = GetAnimatorStateSpeed(_state, _animator);
            animation.loop = _state.loop;
            animation.playAnimation = true;
            animation.rootMotionMask = _state.rootMotionMask;

            _animator.time = animation.animationTime;
            _animator.drivesModelAnimation = true;
        }

        void ApplyAnimatorState(
            Entity& _entity,
            Animator& _animator,
            const AnimatorState& _state,
            const AnimatorTransition* _transition,
            float _previousNormalizedTime)
        {
            if (!_state.modelPath.empty())
            {
                ApplyAnimatorModelState(
                    _entity,
                    _animator,
                    _state,
                    _transition,
                    _previousNormalizedTime);
            }
            else
            {
                _animator.drivesModelAnimation = false;
                _animator.time = 0.0f;
                if (_entity.HasComponent<ModelAnimation>())
                    _entity.GetComponent<ModelAnimation>().sourceModelId = -1;
            }

            _animator.appliedState = _state.name;
            _animator.lastEventSampleTime = 0.0f;
            _animator.lastEventSampleValid = false;
            _animator.lastEventClipPath.clear();
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
                animator.appliedState.clear();
                animator.drivesModelAnimation = false;
                animator.lastEventSampleValid = false;
                state = FindAnimatorState(*controller, animator.currentState);
            }

            if (state == nullptr)
                continue;

            if (animator.appliedState != state->name)
                ApplyAnimatorState(*entity, animator, *state, nullptr, 0.0f);

            std::string activeClipPath = {};
            AnimationClipAsset* clip = nullptr;
            float clipLength =
                GetAnimatorStateLength(*state, clip, activeClipPath);
            const float previousSampleTime = animator.lastEventSampleTime;
            bool hadPreviousSample = animator.lastEventSampleValid &&
                animator.lastEventClipPath == activeClipPath;

            if (animator.playing && clipLength > 0.0f)
            {
                animator.time += _deltaTime * GetAnimatorStateSpeed(*state, animator);

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
                const float previousNormalizedTime = normalizedTime;
                ConsumeAnimatorTransitionTriggers(*transitionToTake, animator);
                animator.currentState = transitionToTake->toState;

                state = FindAnimatorState(*controller, animator.currentState);
                if (state == nullptr)
                    continue;

                ApplyAnimatorState(
                    *entity,
                    animator,
                    *state,
                    transitionToTake,
                    previousNormalizedTime);
                clipLength = GetAnimatorStateLength(*state, clip, activeClipPath);
                hadPreviousSample = false;
            }

            if (!state->modelPath.empty())
            {
                if (animator.drivesModelAnimation &&
                    entity->HasComponent<ModelAnimation>())
                {
                    ModelAnimation& modelAnimation =
                        entity->GetComponent<ModelAnimation>();
                    modelAnimation.animationTime = animator.time;
                    modelAnimation.animationSpeed =
                        GetAnimatorStateSpeed(*state, animator);
                    modelAnimation.loop = state->loop;
                    modelAnimation.playAnimation = animator.playing;
                    modelAnimation.rootMotionMask = state->rootMotionMask;
                }
                continue;
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
