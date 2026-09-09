#include <Canis/Asset.hpp>
#include <Canis/Components.hpp>

#include <cassert>
#include <filesystem>

int main()
{
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "canis_animator_layers_test.animator";

    Canis::AnimatorControllerAsset source = {};
    source.entryState = "Idle";
    source.states.push_back(Canis::AnimatorState{});
    source.states.back().name = "Idle";
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
    assert(source.Save(path.string()));

    Canis::AnimatorControllerAsset loaded = {};
    assert(loaded.Load(path.string()));
    assert(loaded.states.size() == 1);
    assert(loaded.layers.size() == 1);
    assert(loaded.layers[0].name == "Upper Body");
    assert(loaded.layers[0].blendMode == Canis::AnimatorLayerBlendMode::ADDITIVE);
    assert(loaded.layers[0].maskRoots.size() == 2);
    assert(loaded.layers[0].maskBones.size() == 1);
    assert(loaded.layers[0].maskBones[0] == "Abdomen");
    assert(loaded.layers[0].maskWeights.size() == 1);
    assert(loaded.layers[0].maskWeights[0].boneName == "Chest");
    assert(loaded.layers[0].maskWeights[0].weight == 0.5f);
    assert(loaded.layers[0].states[0].modelAnimationIndex == 3);

    Canis::Animator animator = {};
    animator.Play("Run");
    animator.Play("Fire", "Upper Body", 0.0f, true);
    animator.SetLayerWeight("Upper Body", 0.25f);
    assert(animator.currentState == "Run");
    assert(animator.layers.size() == 1);
    assert(animator.layers[0].requestedState == "Fire");
    assert(animator.layers[0].restartRequested);
    assert(animator.layers[0].weightOverride == 0.25f);

    std::filesystem::remove(path);
    return 0;
}
