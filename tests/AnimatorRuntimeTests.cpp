#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/ECS/Systems/AnimatorSystem.hpp>
#include <Canis/ECS/Systems/ModelAnimation3DSystem.hpp>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Canis;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void Near(float a, float b, const char* message) { Check(std::isfinite(a) && std::abs(a-b)<.0001f, message); }
static constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
static constexpr float Inf = std::numeric_limits<float>::infinity();

struct Fixture
{
    App app;
    AnimatorSystem system;
    Entity owner;
    Animator* animator;
    AnimatorControllerAsset* controller;
    std::filesystem::path root;
    std::string modelPath;
    Fixture()
    {
        root = std::filesystem::temp_directory_path() / ("canis-animator-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        AnimationClipAsset clip; clip.length = 2;
        Check(clip.Save((root/"clip.anim").string()), "save clip fixture");
        AnimatorControllerAsset data; data.entryState = "Idle";
        for (auto name : {"Idle", "Run"})
        {
            AnimatorState state; state.name = name; state.clip.path = (root/"clip.anim").string();
            data.states.push_back(state);
        }
        data.parameters.push_back({"Speed", AnimatorParameterType::FLOAT, AnimationValue::Float(1)});
        Check(data.Save((root/"controller.animator").string()), "save controller fixture");
        controller = AssetManager::GetAnimatorController((root/"controller.animator").string());
        Check(controller != nullptr, "load controller fixture");
        app.scene.app = &app; system.scene = &app.scene;
        owner = app.scene.CreateEntity("Animated");
        animator = owner.AddComponent<Animator>();
        animator->controller.path = (root/"controller.animator").string();
        // An imported animation with no render primitives needs no GPU context.
        const float buffer[] = {0, 2, 0, 0, 0, 2, 0, 0};
        { std::ofstream out(root/"motion.bin", std::ios::binary); out.write(reinterpret_cast<const char*>(buffer), sizeof(buffer)); }
        { std::ofstream out(root/"motion.gltf"); out << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"motion.bin","byteLength":32}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":8},{"buffer":0,"byteOffset":8,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[2]},{"bufferView":1,"componentType":5126,"count":2,"type":"VEC3"}],"nodes":[{"name":"Root"}],"scenes":[{"nodes":[0]}],"scene":0,"animations":[{"name":"Motion","samplers":[{"input":0,"output":1,"interpolation":"LINEAR"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}}]}]})"; }
        modelPath = (root/"motion.gltf").string();
    }
    ~Fixture() { std::filesystem::remove_all(root); }
    void Tick(float dt) { system.Update(app.scene.GetRegistry(), dt); }
    void AddLayer()
    {
        owner.AddComponent<ModelAnimation>();
        AnimatorLayer layer; layer.name="Upper"; layer.entryState="Idle";
        layer.states=controller->states;
        for(auto& state:layer.states) { state.modelPath=modelPath; state.clip={}; }
        layer.maskRoots={"Root"}; layer.maskBones={"Hand"}; layer.maskWeights={{"Root",.5f}};
        layer.blendMode=AnimatorLayerBlendMode::ADDITIVE;
        controller->layers.push_back(layer);
    }
};

static void PublicAPI()
{
    Animator a;
    a.Create();
    Check(!a.GetParameter("missing"), "missing parameter");
    Near(a.GetFloat("missing", 7),7,"float fallback");
    Check(a.GetInt("missing",8)==8 && a.GetBool("missing",true),"typed fallbacks");
    a.SetFloat("speed",2.5f); a.SetInt("mode",3); a.SetBool("grounded",true);
    Near(a.GetFloat("speed"),2.5f,"set/get float");
    Check(a.GetInt("mode")==3 && a.GetBool("grounded"),"set/get int/bool");
    a.SetFloat("speed",4); Check(a.parameters.size()==3,"setter must update existing parameter");
    const Animator& c=a; Check(c.GetParameter("mode") && !c.GetParameter("absent"),"const lookup");
    a.GetOrCreateParameter("extra"); a.GetOrCreateParameter("extra");
    Check(a.parameters.size()==4,"create parameter is idempotent");
    a.SetTrigger("fire"); Check(a.GetBool("fire") && a.GetParameter("fire")->triggerActive,"set trigger");
    a.ResetTrigger("fire"); Check(!a.GetBool("fire") && !a.GetParameter("fire")->triggerActive,"reset trigger");
    a.ResetTrigger("unknown");
    auto count=a.parameters.size(); a.SetFloat("",1); a.SetInt("",1); a.SetBool("",true); a.SetTrigger("");
    Check(a.parameters.size()==count,"empty parameter names ignored");
    a.Play(""); Check(a.currentState.empty(),"empty play ignored");
    a.SetLayerWeight("Upper",2); Near(a.layers[0].weightOverride,1,"weight upper clamp");
    a.SetLayerWeight("Upper",-2); Near(a.layers[0].weightOverride,0,"weight lower clamp");
    a.SetLayerWeight("Upper",NaN); a.SetLayerWeight("",.4f); a.SetLayerWeight("Base Layer",.4f);
    Check(a.layers.size()==1 && a.layers[0].weightOverride==0,"invalid weights/layers ignored");
}

static void Playback()
{
    Fixture f; auto& a=*f.animator;
    a.SetFloat("Speed",3); f.Tick(0); Near(a.GetFloat("Speed"),3,"defaults preserve script-set values");
    Check(a.currentState=="Idle" && a.parametersInitialized,"entry state and defaults");
    a.Play("Run","Base Layer",.5f,true); f.Tick(0); Near(a.time,1,"base normalized start");
    a.Play("Run","",.1f); f.Tick(.2f); Near(a.time,1.2f,"same-state play preserves phase");
    a.Play("Run","",.25f,true); f.Tick(0); Near(a.time,.5f,"explicit restart seeks");
    a.playing=false; f.Tick(.5f); Near(a.time,.5f,"pause time");
    a.playing=true; f.Tick(Inf); f.Tick(NaN); f.Tick(-1); Near(a.time,.5f,"invalid delta ignored");
    f.controller->states[1].speed=-1; f.Tick(1); Near(a.time,1.5f,"reverse loop wraps");
    f.controller->states[1].speed=1; f.Tick(1000000); Near(a.time,1.5f,"large step constant-time wrap");
    f.controller->states[1].speed=Inf; f.Tick(1); Near(a.time,1.5f,"invalid speed holds time");
    f.controller->states[1].speed=1; f.controller->states[1].loop=false;
    a.Play("Run","",2,true); f.Tick(.5f); Near(a.time,2,"nonloop normalized clamp and endpoint");
    a.Play("Run","",NaN,true); f.Tick(0); Near(a.time,0,"invalid normalized start");
    f.controller->states[1].speedParameter="Speed"; a.SetFloat("Speed",2);
    f.Tick(.25f); Near(a.time,.5f,"speed parameter multiplier");
    a.SetFloat("Speed",0); f.Tick(1); Near(a.time,.5f,"zero speed holds");
    f.owner.Active()=false; f.Tick(.5f); Near(a.time,.5f,"inactive entity not advanced");
}

static AnimatorTransition Transition(const std::string& target, AnimatorConditionMode mode, AnimationValue value)
{
    AnimatorTransition t; t.toState=target; t.conditions.push_back({"p",mode,value}); return t;
}
static void ConditionsAndTransitions()
{
    for(auto mode : {AnimatorConditionMode::GREATER,AnimatorConditionMode::LESS,AnimatorConditionMode::EQUAL,
                    AnimatorConditionMode::NOT_EQUAL,AnimatorConditionMode::IF_TRUE,AnimatorConditionMode::IF_FALSE,
                    AnimatorConditionMode::TRIGGERED})
    {
        Fixture f; auto& a=*f.animator;
        const bool less=mode==AnimatorConditionMode::LESS;
        const bool eq=mode==AnimatorConditionMode::EQUAL;
        auto t=Transition("Run",mode,AnimationValue::Float(less?3.f:eq?2.f:1.f));
        f.controller->states[0].transitions={t};
        f.Tick(0); Check(a.currentState=="Idle","missing parameter cannot match");
        a.SetFloat("p",2);
        if(mode==AnimatorConditionMode::IF_FALSE) a.SetBool("p",false);
        if(mode==AnimatorConditionMode::TRIGGERED) a.SetTrigger("p");
        a.playing=false; f.Tick(.2f); Check(a.currentState=="Idle","paused base transition");
        a.playing=true; f.Tick(0); Check(a.currentState=="Run","condition matched");
        if(mode==AnimatorConditionMode::TRIGGERED) Check(!a.GetParameter("p")->triggerActive,"trigger consumed once");
    }
    Fixture f; auto& a=*f.animator;
    a.SetTrigger("p");
    auto bad=Transition("Absent",AnimatorConditionMode::TRIGGERED,{});
    f.controller->states[0].transitions={bad}; f.Tick(.1f);
    Check(a.currentState=="Idle" && a.GetParameter("p")->triggerActive,"bad target preserves state and trigger");
    auto good=bad; good.toState="Run"; good.hasExitTime=true; good.exitTimeNormalized=1;
    f.controller->states[0].transitions.push_back(good);
    a.Play("Idle","",.95f,true); f.Tick(.2f);
    Check(a.currentState=="Run" && !a.GetParameter("p")->triggerActive,"exit at loop boundary, skip invalid priority");
    // Every condition must match, with integer and boolean equality branches.
    good.hasExitTime=false; good.conditions={{"mode",AnimatorConditionMode::EQUAL,AnimationValue::Int(2)},
                                            {"ready",AnimatorConditionMode::EQUAL,AnimationValue::Bool(true)}};
    f.controller->states[0].transitions={good}; a.Play("Idle","",0,true);
    a.SetInt("mode",2); a.SetBool("ready",false); f.Tick(0);
    Check(a.currentState=="Idle","AND conditions reject partial match");
    a.SetBool("ready",true); f.Tick(0); Check(a.currentState=="Run","integer and boolean equality");
    good.conditions={{"mode",AnimatorConditionMode::NOT_EQUAL,AnimationValue::Int(2)},
                     {"ready",AnimatorConditionMode::NOT_EQUAL,AnimationValue::Bool(true)}};
    f.controller->states[0].transitions={good}; a.Play("Idle","",0,true); f.Tick(0);
    Check(a.currentState=="Idle","inequality rejects equal values");
    a.SetInt("mode",3); a.SetBool("ready",false); f.Tick(0); Check(a.currentState=="Run","typed inequality");
    a.Play("Idle","",.25f,true); good.hasExitTime=false; good.conditions.clear(); good.preserveNormalizedTime=true;
    f.controller->states[0].transitions={good}; f.Tick(0); Near(a.time,.5f,"property clip transition preserves phase");
}

static void Layers()
{
    Fixture f; f.AddLayer(); auto& a=*f.animator;
    a.Play("Run","Upper",.5f,true); f.Tick(0);
    auto& layer=a.layers[0]; Near(layer.time,1,"layer normalized start");
    auto& samples=f.owner.GetComponent<ModelAnimation>().layers;
    Check(samples.size()==1 && samples[0].additive && samples[0].maskRoots.size()==1 &&
          samples[0].maskBones.size()==1 && samples[0].maskWeights[0].weight==.5f,"layer mode and masks delivered");
    Near(samples[0].animationTime,1,"layer sample time");
    f.controller->layers[0].blendMode=AnimatorLayerBlendMode::OVERRIDE; f.Tick(0);
    Check(!samples[0].additive,"override layer sample");
    a.Play("Run","Upper",.1f); f.Tick(.2f); Near(layer.time,1.2f,"same layer state retains time");
    a.Play("Missing","Upper",0,true); f.Tick(0); Check(layer.currentState=="Run","bad play request preserves layer");
    a.Play("Run","Upper",.25f,true); f.Tick(0); Near(layer.time,.5f,"layer restart");
    a.SetLayerWeight("Upper",.3f); f.Tick(0); Near(samples[0].weight,.3f,"weight override delivered");
    layer.weightOverride=-1; f.controller->layers[0].weightParameter="weight";
    a.SetFloat("weight",.7f); f.Tick(0); Near(samples[0].weight,.7f,"parameter weight");
    a.SetFloat("weight",NaN); f.Tick(0); Near(samples[0].weight,0,"invalid weight sanitized");
    auto t=Transition("Idle",AnimatorConditionMode::TRIGGERED,{}); t.duration=1; t.preserveNormalizedTime=true;
    auto bad=t; bad.toState="Missing";
    f.controller->layers[0].states[1].transitions={bad,t}; a.SetTrigger("p"); a.playing=false; f.Tick(.4f);
    Check(layer.currentState=="Run" && a.GetParameter("p")->triggerActive,"paused layer keeps trigger and state");
    Near(layer.time,.5f,"paused layer holds time");
    a.playing=true; f.Tick(0); Check(layer.currentState=="Idle","layer skips invalid target");
    Near(layer.time,.5f,"layer transition preserves normalized phase");
    f.Tick(.2f); Near(samples[0].transitionTargetWeight,.2f,"layer blend progresses");
    a.playing=false; f.Tick(.4f); Near(samples[0].transitionTargetWeight,.2f,"paused layer blend holds");
    a.playing=true; f.Tick(1); Check(layer.transitionSourceModelId==-1,"layer blend completes");
    auto exit=Transition("Run",AnimatorConditionMode::IF_TRUE,{}); exit.conditions.clear();
    exit.hasExitTime=true; exit.exitTimeNormalized=1;
    f.controller->layers[0].states[0].transitions={exit};
    a.Play("Idle","Upper",.95f,true); f.Tick(.2f);
    Check(layer.currentState=="Run","layer exit at loop boundary");
    // Imported base animation gets the same requested phase as property clips.
    f.owner.AddComponent<Model>()->modelId=AssetManager::LoadModel(f.modelPath);
    f.controller->states[0].modelPath=f.modelPath;
    a.Play("Idle","",.75f,true); f.Tick(0); Near(a.time,1.5f,"model base seek");
    auto& modelAnimation=f.owner.GetComponent<ModelAnimation>();
    Near(modelAnimation.animationTime,1.5f,"model sample seek");
    ModelAnimation3DSystem poses; poses.scene=&f.app.scene; poses.Ready();
    poses.Update(f.app.scene.GetRegistry(),0);
    Check(modelAnimation.poseInitialized && !modelAnimation.pose.localNodeMatrices.empty(),"imported pose sampled");
    // Test the base transition clock through the actual pose update system.
    f.controller->layers.clear(); f.controller->states[1].modelPath=f.modelPath;
    auto blend=Transition("Run",AnimatorConditionMode::TRIGGERED,{}); blend.duration=1;
    f.controller->states[0].transitions={blend}; a.SetTrigger("p"); f.Tick(0);
    poses.Update(f.app.scene.GetRegistry(),.2f); Near(modelAnimation.transitionElapsed,.2f,"base model blend progresses");
    a.playing=false; f.Tick(.4f); poses.Update(f.app.scene.GetRegistry(),.4f);
    Near(modelAnimation.transitionElapsed,.2f,"base model blend paused");
    a.playing=true; f.Tick(.2f); poses.Update(f.app.scene.GetRegistry(),.2f);
    Near(modelAnimation.transitionElapsed,.4f,"base model blend resumes");
}
int main()
{
    try { PublicAPI(); Playback(); ConditionsAndTransitions(); Layers(); }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
    std::cout<<"Animator API, playback, conditions, transitions and layer runtime passed\n";
}
