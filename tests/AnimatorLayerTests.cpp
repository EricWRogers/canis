#include <Canis/Asset.hpp>
#include <Canis/Components.hpp>

#include <stdexcept>
#include <chrono>
#include <filesystem>

static void Check(bool ok) { if (!ok) throw std::runtime_error("Animator serialization check failed"); }

int main()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("canis_animator_layers_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".animator");

    Canis::AnimatorControllerAsset source = {};
    source.entryState = "Idle";
    source.states.push_back(Canis::AnimatorState{});
    source.states.back().name = "Idle";
    source.states.back().clip.path = "assets/idle.anim";
    source.states.back().speed = 1.25f;
    source.states.back().speedParameter = "Speed";
    source.states.back().loop = false;
    source.states.back().rootMotionMask = {1, 0, 1};
    source.parameters = {{"Speed", Canis::AnimatorParameterType::FLOAT, Canis::AnimationValue::Float(.5f)},
                         {"Fire", Canis::AnimatorParameterType::TRIGGER, Canis::AnimationValue::Bool(false)}};
    Canis::AnimatorTransition transition;
    transition.toState = "Idle";
    transition.hasExitTime = true;
    transition.exitTimeNormalized = .8f;
    transition.duration = .25f;
    transition.preserveNormalizedTime = true;
    transition.conditions = {{"Fire", Canis::AnimatorConditionMode::TRIGGERED, Canis::AnimationValue::Bool(true)}};
    source.states.back().transitions.push_back(transition);
    Canis::AnimatorLayer layer = {};
    layer.name = "Upper Body";
    layer.blendMode = Canis::AnimatorLayerBlendMode::ADDITIVE;
    layer.weight = 0.65f;
    layer.weightParameter = "RecoilWeight";
    layer.maskRoots = {"Chest", "Shoulder.R"};
    layer.maskBones = {"Abdomen"};
    layer.maskWeights = {{"Chest", 0.5f}};
    layer.entryState = "Aim";
    layer.states.push_back(Canis::AnimatorState{});
    layer.states.back().name = "Aim";
    layer.states.back().modelPath = "assets/character.glb";
    layer.states.back().modelAnimationIndex = 3;
    source.layers.push_back(layer);
    Check(source.Save(path.string()));

    Canis::AnimatorControllerAsset loaded = {};
    Check(loaded.Load(path.string()));
    Check(loaded.states.size() == 1);
    Check(loaded.entryState == "Idle" && loaded.parameters.size() == 2);
    Check(loaded.parameters[0].defaultValue.AsFloat() == .5f);
    Check(loaded.parameters[1].type == Canis::AnimatorParameterType::TRIGGER);
    Check(loaded.states[0].clip.path == "assets/idle.anim");
    Check(loaded.states[0].speed == 1.25f && loaded.states[0].speedParameter == "Speed");
    Check(!loaded.states[0].loop && loaded.states[0].rootMotionMask == Canis::Vector3(1,0,1));
    Check(loaded.states[0].transitions.size() == 1);
    const auto& savedTransition = loaded.states[0].transitions[0];
    Check(savedTransition.toState == "Idle" && savedTransition.hasExitTime);
    Check(savedTransition.exitTimeNormalized == .8f && savedTransition.duration == .25f);
    Check(savedTransition.preserveNormalizedTime && savedTransition.conditions.size() == 1);
    Check(savedTransition.conditions[0].parameter == "Fire");
    Check(savedTransition.conditions[0].mode == Canis::AnimatorConditionMode::TRIGGERED);
    Check(savedTransition.conditions[0].value.AsBool());
    Check(loaded.layers.size() == 1);
    Check(loaded.layers[0].name == "Upper Body");
    Check(loaded.layers[0].weight == .65f && loaded.layers[0].weightParameter == "RecoilWeight");
    Check(loaded.layers[0].entryState == "Aim");
    Check(loaded.layers[0].states[0].modelPath == "assets/character.glb");
    Check(loaded.layers[0].blendMode == Canis::AnimatorLayerBlendMode::ADDITIVE);
    Check(loaded.layers[0].maskRoots.size() == 2);
    Check(loaded.layers[0].maskBones.size() == 1);
    Check(loaded.layers[0].maskBones[0] == "Abdomen");
    Check(loaded.layers[0].maskWeights.size() == 1);
    Check(loaded.layers[0].maskWeights[0].boneName == "Chest");
    Check(loaded.layers[0].maskWeights[0].weight == 0.5f);
    Check(loaded.layers[0].states[0].modelAnimationIndex == 3);

    Canis::Animator animator = {};
    animator.Play("Run");
    animator.Play("Fire", "Upper Body", 0.0f, true);
    animator.SetLayerWeight("Upper Body", 0.25f);
    Check(animator.currentState == "Run");
    Check(animator.layers.size() == 1);
    Check(animator.layers[0].requestedState == "Fire");
    Check(animator.layers[0].restartRequested);
    Check(animator.layers[0].weightOverride == 0.25f);

    std::filesystem::remove(path);
    return 0;
}
