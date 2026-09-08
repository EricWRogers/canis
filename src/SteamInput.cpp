#include <Canis/SteamInput.hpp>
#include <Canis/InputActions.hpp>
#include <Canis/Debug.hpp>
#include <algorithm>
#include <filesystem>
#include <unordered_map>
#if CANIS_ENABLE_STEAM_INPUT
#include <steam/steam_api.h>
#endif

namespace Canis::SteamInputPlatform
{
namespace {
bool initialized=false;
uint64_t ownedController=0;
const char* status="SDL (Steam Input disabled)";
#if CANIS_ENABLE_STEAM_INPUT
std::unordered_map<std::string,uint64_t> handles;
std::unordered_map<int,InputGlyph> glyphs;
std::vector<uint32_t> activeMaps;
std::string schemaKey;
uint64_t Handle(const std::string& name,int kind)
{
    const std::string key=std::to_string(kind)+name;
    auto it=handles.find(key); if(it!=handles.end() && it->second) return it->second;
    const uint64_t handle=kind==0 ? SteamInput()->GetActionSetHandle(name.c_str()) : kind==1 ? SteamInput()->GetDigitalActionHandle(name.c_str()) : SteamInput()->GetAnalogActionHandle(name.c_str());
    handles[key]=handle;return handle;
}
InputPrompt OriginPrompt(EInputActionOrigin* origins,int count,const std::string& fallback)
{
    InputPrompt prompt;
    for(int i=0;i<count;++i) {
        if(!prompt.text.empty()) prompt.text+=" / ";
        const char* text=SteamInput()->GetStringForActionOrigin(origins[i]);
        prompt.text+=text && *text ? text : fallback;
        auto it=glyphs.find(static_cast<int>(origins[i]));
        if(it==glyphs.end()) {
            const char* path=SteamInput()->GetGlyphPNGForActionOrigin(origins[i],k_ESteamInputGlyphSize_Medium,ESteamInputGlyphStyle_Dark);
            InputGlyph glyph;
            if(path && *path && std::filesystem::is_regular_file(path)) {glyph.texture=path;glyph.uv=Vector4(0,0,1,1);}
            it=glyphs.emplace(static_cast<int>(origins[i]),std::move(glyph)).first;
        }
        if(it->second) prompt.glyphs.push_back(it->second);
    }
    if(prompt.text.empty()) prompt.text=fallback+" (Steam Input)";
    return prompt;
}
#endif
}
bool Initialize()
{
#if CANIS_ENABLE_STEAM_INPUT
    if(initialized) return true;
    if(!SteamAPI_Init()) {status="SDL (Steam unavailable)";return false;}
    if(!SteamInput() || !SteamInput()->Init(true)) {SteamAPI_Shutdown();status="SDL (Steam Input unavailable)";return false;}
    const auto manifest=std::filesystem::absolute("project_settings/steam/input_manifest.vdf").string();
    if(!SteamInput()->SetInputActionManifestFilePath(manifest.c_str()))
        Debug::Warning("Steam Input manifest unavailable; configure Steam partner actions or check %s",manifest.c_str());
    initialized=true;status="SDL (waiting for Steam controller configuration)";
#endif
    return initialized;
}
void Shutdown()
{
#if CANIS_ENABLE_STEAM_INPUT
    if(initialized) {SteamInput()->Shutdown();SteamAPI_Shutdown();}
    handles.clear();glyphs.clear();activeMaps.clear();
#endif
    initialized=false;ownedController=0;
}
bool OwnsController(uint64_t handle) {return handle!=0 && handle==ownedController;}
const char* Status() {return status;}
bool ShowBindingPanel()
{
#if CANIS_ENABLE_STEAM_INPUT
    return initialized && ownedController && SteamInput()->ShowBindingPanel(ownedController);
#else
    return false;
#endif
}
void Poll(InputActionSystem& actions,const std::vector<uint64_t>& sdlSteamHandles)
{
#if CANIS_ENABLE_STEAM_INPUT
    if(!initialized) return;
    std::string key;
    for (const auto& action : actions.Document().actions)
        key += std::to_string(action.id) + ":" + std::to_string(action.map) + ":" + std::to_string(static_cast<int>(action.type)) + ":" + action.name + ";";
    if (key != schemaKey) {
        schemaKey = key; handles.clear(); glyphs.clear(); activeMaps.clear();
        const auto manifest = std::filesystem::absolute("project_settings/steam/input_manifest.vdf").string();
        SteamInput()->SetInputActionManifestFilePath(manifest.c_str());
    }
    SteamAPI_RunCallbacks();SteamInput()->RunFrame();
    InputHandle_t controllers[STEAM_INPUT_MAX_COUNT] = {};
    const int count=SteamInput()->GetConnectedControllers(controllers);
    std::vector<uint32_t> maps;
    for(const auto& map:actions.Document().maps) if(actions.IsMapEnabled({map.id})) maps.push_back(map.id);
    std::sort(maps.begin(),maps.end());
    if(!count || maps.empty()) {
        ownedController=0;actions.SetNativeInput(false);status="SDL (no active Steam controller/map)";return;
    }
    InputHandle_t controller=controllers[0];
    if(ownedController && std::find(controllers,controllers+count,ownedController)!=controllers+count) controller=ownedController;
    // SDL's Steam handle is the public device identity bridge. If SDL sees pads
    // but cannot identify this controller, stay on SDL rather than double count.
    if(SteamInput()->GetGamepadIndexForController(controller)>=0 && !sdlSteamHandles.empty() && std::find(sdlSteamHandles.begin(),sdlSteamHandles.end(),controller)==sdlSteamHandles.end()) {
        ownedController=0;actions.SetNativeInput(false);status="SDL (Steam device identity unavailable)";return;
    }
    const uint32_t primary=maps.front();
    const auto set=Handle("map_"+std::to_string(primary),0);
    if(!set) {ownedController=0;actions.SetNativeInput(false);status="SDL (Steam actions not configured)";return;}
    if(controller!=ownedController || maps!=activeMaps) {
        SteamInput()->ActivateActionSet(controller,set);
        SteamInput()->DeactivateAllActionSetLayers(controller);
        for(auto map:maps) if(map!=primary) {
            const auto layer=Handle("layer_"+std::to_string(primary)+"_"+std::to_string(map),0);
            if(layer) SteamInput()->ActivateActionSetLayer(controller,layer);
        }
        glyphs.clear();activeMaps=maps;
    }
    struct Sample {uint32_t id;Vector2 value;InputPrompt prompt;};
    std::vector<Sample> samples;bool configurationActive=false;
    for(const auto& action:actions.Document().actions) {
        Sample sample{action.id,Vector2(0),{action.name+" (Steam Input)",{}}};
        if(std::find(maps.begin(),maps.end(),action.map)!=maps.end()) {
            const std::string suffix=action.map==primary ? "" : "_on_"+std::to_string(primary);
            const std::string name="action_"+std::to_string(action.id)+suffix;
            const auto actionHandle=Handle(name,action.type==ActionType::Button ? 1 : 2);
            EInputActionOrigin origins[STEAM_INPUT_MAX_ORIGINS] = {};
            const auto originSet=action.map==primary ? set : Handle("layer_"+std::to_string(primary)+"_"+std::to_string(action.map),0);
            int originsCount=0;
            if(actionHandle && action.type==ActionType::Button) {
                const auto data=SteamInput()->GetDigitalActionData(controller,actionHandle);
                configurationActive|=data.bActive;
                sample.value.x=data.bActive && data.bState ? 1.0f : 0.0f;
                originsCount=SteamInput()->GetDigitalActionOrigins(controller,originSet,actionHandle,origins);
            } else if(actionHandle) {
                const auto data=SteamInput()->GetAnalogActionData(controller,actionHandle);
                configurationActive|=data.bActive;
                if(data.bActive) sample.value=Vector2(data.x,action.type==ActionType::Axis2D ? data.y : 0);
                const bool mouseDelta=std::any_of(action.bindings.begin(),action.bindings.end(),[](const auto& b){return b.path=="Mouse/Delta";});
                if(mouseDelta) sample.value.y=-sample.value.y;
                originsCount=SteamInput()->GetAnalogActionOrigins(controller,originSet,actionHandle,origins);
            }
            sample.prompt=OriginPrompt(origins,std::clamp(originsCount,0,STEAM_INPUT_MAX_ORIGINS),action.name);
        }
        samples.push_back(std::move(sample));
    }
    if(!configurationActive) {ownedController=0;actions.SetNativeInput(false);status="SDL (Steam configuration still loading)";return;}
    if(ownedController && ownedController!=controller) actions.SetNativeInput(false);
    ownedController=controller;actions.SetNativeInput(true);status="Steam Input";
    for(auto& sample:samples) {actions.RecordNative({sample.id},sample.value);actions.SetNativePrompt({sample.id},std::move(sample.prompt));}
#else
    (void)actions;(void)sdlSteamHandles;
#endif
}
}
