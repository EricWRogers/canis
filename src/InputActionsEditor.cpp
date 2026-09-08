#if CANIS_EDITOR
#include <Canis/InputActionsEditor.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/SteamInput.hpp>
#include <Canis/Canis.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Asset.hpp>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <filesystem>
#include <algorithm>

namespace Canis
{
namespace
{
struct InputEditorState
{
    InputDocument document;
    std::string path, saved, error, filter, capturePart;
    std::vector<InputDocument> undo, redo;
    uint32_t map=0, action=0, binding=0, captureBinding=0;
    int scheme=0;
};
void GlyphPreview(const InputPrompt& prompt)
{
    for(const auto& glyph:prompt.glyphs) {
        if(auto* asset=AssetManager::GetTexture(glyph.texture)) {
            const auto texture=asset->GetGLTexture();
            const float width=(glyph.uv.z-glyph.uv.x)*texture.width;
            const float height=std::max(1.0f,(glyph.uv.w-glyph.uv.y)*texture.height);
            ImGui::Image((ImTextureID)(intptr_t)texture.id,ImVec2(40*width/height,40),ImVec2(glyph.uv.x,glyph.uv.y),ImVec2(glyph.uv.z,glyph.uv.w));
            ImGui::SameLine();
        }
    }
    ImGui::TextUnformatted(prompt.text.c_str());
}
void Picker(const char* label,std::string& path,InputScheme scheme,ActionType type,const std::string& filter)
{
    if(ImGui::BeginCombo(label,path.c_str())) {
        for(const auto& control:InputControls()) {
            const std::string candidate=control.path;
            if(candidate.starts_with("Gamepad/")!=(scheme==InputScheme::Gamepad)) continue;
            if(control.type!=type && !(type==ActionType::Button && control.type==ActionType::Axis1D)) continue;
            if(!filter.empty() && candidate.find(filter)==std::string::npos) continue;
            if(ImGui::Selectable(control.path,candidate==path)) path=candidate;
        }
        ImGui::EndCombo();
    }
}
void Retire(InputDocument& d,const InputActionDefinition& a)
{
    d.reservedIds.push_back(a.id);
    for(const auto& b:a.bindings) d.reservedIds.push_back(b.id);
}
std::string UniqueName(const InputDocument& d,std::string base,bool map)
{
    std::string result=base; int suffix=2;
    auto used=[&](){if(map) return std::any_of(d.maps.begin(),d.maps.end(),[&](const auto& m){return m.name==result;});
        return std::any_of(d.actions.begin(),d.actions.end(),[&](const auto& a){return a.name==result;});};
    while(used()) result=base+std::to_string(suffix++);
    return result;
}
}
void DrawInputActionsEditor(InputManager& input,bool& open)
{
    static InputEditorState state;
    auto& runtime=input.Actions();
    const std::string configured=GetProjectConfig().inputAsset;
    // Source assets are authoritative when running the editor from the build's project directory.
    const std::string path=std::filesystem::exists("../game/CMakeLists.txt") ? "../"+configured : configured;
    if(state.path!=path) {
        state={}; state.path=path;
        if(LoadInputDocument(path,state.document,state.error)) state.saved=SerializeInputDocument(state.document);
        if(!state.document.maps.empty()) state.map=state.document.maps.front().id;
    }
    const auto* viewport=ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(1100.0f,viewport->WorkSize.x*0.9f),std::min(700.0f,viewport->WorkSize.y*0.9f)),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Input Actions",&open)) {if(!open) runtime.CancelCapture();ImGui::End(); return;}
    if(!open) runtime.CancelCapture();
    const auto before=state.document;
    const auto beforeText=SerializeInputDocument(before);
    bool historyOperation=false;
    if(ImGui::Button("Save")) {
        if(SaveInputDocument(state.path,state.document,state.error)) {
            state.saved=SerializeInputDocument(state.document);
            if(state.path!=configured) SaveInputDocument(configured,state.document,state.error);
            runtime.QueueDocument(state.document,state.error);
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Revert")) {
        InputDocument next;
        if(LoadInputDocument(state.path,next,state.error)) {state.undo.push_back(state.document);state.redo.clear();state.document=next;state.saved=SerializeInputDocument(next);historyOperation=true;}
    }
    ImGui::SameLine(); ImGui::BeginDisabled(state.undo.empty());
    if(ImGui::Button("Undo")) {state.redo.push_back(state.document);state.document=state.undo.back();state.undo.pop_back();historyOperation=true;}
    ImGui::EndDisabled(); ImGui::SameLine(); ImGui::BeginDisabled(state.redo.empty());
    if(ImGui::Button("Redo")) {state.undo.push_back(state.document);state.document=state.redo.back();state.redo.pop_back();historyOperation=true;}
    ImGui::EndDisabled(); ImGui::SameLine();
    if(beforeText!=state.saved) ImGui::TextUnformatted("Unsaved changes");
    ImGui::Combo("Scheme",&state.scheme,"All\0KeyboardMouse\0Gamepad\0");
    ImGui::InputText("Find binding",&state.filter);
    if(runtime.IsCapturing()) {
        ImGui::TextUnformatted("Release held controls, then press an input. Escape cancels; timeout 10 seconds.");
        if(ImGui::Button("Cancel capture")) runtime.CancelCapture();
    }
    if(auto captured=runtime.TakeCapturedControl())
        for(auto& a:state.document.actions) for(auto& b:a.bindings) if(b.id==state.captureBinding) {if(state.capturePart.empty()) {b.path=*captured;b.parts.clear();} else b.parts[state.capturePart]=*captured;}
    if(ImGui::BeginTable("InputAuthoring",3,ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Maps",ImGuiTableColumnFlags_WidthFixed,150);
        ImGui::TableSetupColumn("Actions / Bindings",ImGuiTableColumnFlags_WidthFixed,260);
        ImGui::TableSetupColumn("Properties"); ImGui::TableHeadersRow(); ImGui::TableNextRow(); ImGui::TableNextColumn();
        const float paneHeight=std::max(180.0f,ImGui::GetContentRegionAvail().y-65.0f);
        ImGui::BeginChild("Map pane",ImVec2(0,paneHeight));
        for(const auto& map:state.document.maps) if(ImGui::Selectable((map.name+"##map"+std::to_string(map.id)).c_str(),state.map==map.id)) {state.map=map.id;state.action=state.binding=0;}
        if(ImGui::Button("+ Map")) {auto id=AllocateInputId(state.document);state.document.maps.push_back({id,UniqueName(state.document,"NewMap",true)});state.map=id;}
        auto map=std::find_if(state.document.maps.begin(),state.document.maps.end(),[&](const auto& m){return m.id==state.map;});
        if(map!=state.document.maps.end()) {
            ImGui::InputText("Map name",&map->name);
            if(ImGui::Button("Duplicate map")) {
                const uint32_t original=map->id;
                auto id=AllocateInputId(state.document); const auto name=UniqueName(state.document,map->name+"Copy",true);
                state.document.maps.push_back({id,name});
                std::vector<InputActionDefinition> copies;
                for(const auto& a:state.document.actions) if(a.map==original) copies.push_back(a);
                for(auto a:copies) {
                    a.id=AllocateInputId(state.document);a.map=id;a.name=UniqueName(state.document,a.name+"Copy",false);
                    auto bindings=a.bindings;a.bindings.clear();state.document.actions.push_back(a);
                    for(auto b:bindings) {b.id=AllocateInputId(state.document);state.document.actions.back().bindings.push_back(b);}
                }
                state.map=id;
            }
            if(ImGui::Button("Delete map")) {
                state.document.reservedIds.push_back(state.map);
                for(const auto& a:state.document.actions) if(a.map==state.map) Retire(state.document,a);
                std::erase_if(state.document.actions,[&](const auto& a){return a.map==state.map;});
                std::erase_if(state.document.maps,[&](const auto& m){return m.id==state.map;});state.map=0;
            }
        }
        ImGui::EndChild();ImGui::TableNextColumn();
        ImGui::BeginChild("Binding pane",ImVec2(0,paneHeight));
        for(const auto& a:state.document.actions) if(a.map==state.map) {
            if(ImGui::Selectable((a.name+"##action"+std::to_string(a.id)).c_str(),state.action==a.id && !state.binding)) {state.action=a.id;state.binding=0;}
            for(const auto& b:a.bindings) {
                if(state.scheme && (state.scheme==2)!=(b.scheme==InputScheme::Gamepad)) continue;
                if(!state.filter.empty() && b.path.find(state.filter)==std::string::npos) continue;
                ImGui::Indent();
                if(ImGui::Selectable((b.path+"##binding"+std::to_string(b.id)).c_str(),state.binding==b.id)) {state.action=a.id;state.binding=b.id;}
                ImGui::Unindent();
            }
        }
        if(state.map && ImGui::Button("+ Action")) {
            InputActionDefinition a; a.id=AllocateInputId(state.document);a.map=state.map;a.name=UniqueName(state.document,"NewAction",false);
            state.document.actions.push_back(a);state.action=a.id;state.binding=0;
        }
        ImGui::EndChild();ImGui::TableNextColumn();
        ImGui::BeginChild("Properties pane",ImVec2(0,paneHeight));
        auto action=std::find_if(state.document.actions.begin(),state.document.actions.end(),[&](const auto& a){return a.id==state.action;});
        if(action!=state.document.actions.end()) {
            ImGui::Text("Action ID: %u",action->id);
            ImGui::InputText("Name",&action->name);
            int type=static_cast<int>(action->type);
            if(ImGui::Combo("Type",&type,"Button\0Axis1D\0Axis2D\0")) action->type=static_cast<ActionType>(type);
            ImGui::TextDisabled("Changing names, maps or types requires a game-code rebuild.");
            if(ImGui::Button("Duplicate action")) {
                auto copy=*action;copy.id=AllocateInputId(state.document);copy.name=UniqueName(state.document,copy.name+"Copy",false);
                auto bindings=copy.bindings;copy.bindings.clear();state.document.actions.push_back(copy);
                for(auto b:bindings) {b.id=AllocateInputId(state.document);state.document.actions.back().bindings.push_back(b);}
                state.action=copy.id;action=std::prev(state.document.actions.end());
            }
            ImGui::SameLine();
            if(ImGui::Button("Delete action")) {Retire(state.document,*action);state.document.actions.erase(action);state.action=state.binding=0;}
            else {
                if(ImGui::Button("+ Binding")) {
                    InputBinding b;b.id=AllocateInputId(state.document);
                    b.path=action->type==ActionType::Button ? "Keyboard/Space" : action->type==ActionType::Axis1D ? "Mouse/Wheel" : "Mouse/Delta";
                    action->bindings.push_back(b);state.binding=b.id;
                }
                if(action->type!=ActionType::Button) {
                    ImGui::SameLine();
                    if(ImGui::Button("+ Composite")) {
                        InputBinding b;b.id=AllocateInputId(state.document);
                        if(action->type==ActionType::Axis1D) {b.path="Composite/Axis1D";b.parts={{"positive","Keyboard/D"},{"negative","Keyboard/A"}};}
                        else {b.path="Composite/Axis2D";b.parts={{"up","Keyboard/W"},{"down","Keyboard/S"},{"left","Keyboard/A"},{"right","Keyboard/D"}};}
                        action->bindings.push_back(b);state.binding=b.id;
                    }
                }
                auto binding=std::find_if(action->bindings.begin(),action->bindings.end(),[&](const auto& b){return b.id==state.binding;});
                if(binding!=action->bindings.end()) {
                    ImGui::Separator();ImGui::Text("Binding ID: %u",binding->id);
                    if(ImGui::Checkbox("Primary prompt",&binding->primaryPrompt) && binding->primaryPrompt)
                        for(auto& b:action->bindings) if(b.id!=binding->id && b.scheme==binding->scheme) b.primaryPrompt=false;
                    ImGui::InputText("Icon override (texture path)",&binding->iconOverride);
                    int scheme=binding->scheme==InputScheme::Gamepad ? 1 : 0;
                    if(ImGui::Combo("Binding scheme",&scheme,"KeyboardMouse\0Gamepad\0")) binding->scheme=static_cast<InputScheme>(scheme);
                    if(binding->parts.empty()) Picker("Control",binding->path,binding->scheme,action->type,state.filter);
                    else {
                        const std::vector<std::string> order=binding->path=="Composite/Axis2D" ? std::vector<std::string>{"up","down","left","right"} : std::vector<std::string>{"positive","negative"};
                        for(const auto& part:order) {
                            ImGui::PushID(part.c_str());
                            Picker(part.c_str(),binding->parts[part],binding->scheme,ActionType::Button,state.filter);
                            ImGui::SameLine();
                            if(ImGui::SmallButton("Listen")) {
                                state.captureBinding=binding->id;state.capturePart=part;
                                if(binding->scheme==InputScheme::Gamepad && runtime.HasNativeInput())
                                    state.error=SteamInputPlatform::ShowBindingPanel() ? "Steam controller configuration opened." : "Open controller configuration in Steam.";
                                else runtime.BeginCapture(binding->scheme,ActionType::Button);
                            }
                            ImGui::PopID();
                        }
                    }
                    if(ImGui::Button("Listen...")) {
                        state.captureBinding=binding->id;state.capturePart.clear();
                        if(binding->scheme==InputScheme::Gamepad && runtime.HasNativeInput())
                            state.error=SteamInputPlatform::ShowBindingPanel() ? "Steam controller configuration opened." : "Open controller configuration in Steam.";
                        else runtime.BeginCapture(binding->scheme,action->type);
                    }
                    ImGui::SliderFloat("Dead zone",&binding->deadZone,0,0.95f);
                    ImGui::DragFloat("Scale",&binding->scale,0.05f,-10,10);ImGui::Checkbox("Invert",&binding->invert);
                    if(action->type==ActionType::Button) {
                        ImGui::SliderFloat("Press threshold",&binding->pressThreshold,0.01f,1);
                        ImGui::SliderFloat("Release threshold",&binding->releaseThreshold,0,0.99f);
                    }
                    if(ImGui::Button("Duplicate binding")) {auto copy=*binding;copy.id=AllocateInputId(state.document);copy.primaryPrompt=false;action->bindings.push_back(copy);state.binding=copy.id;}
                    ImGui::SameLine();
                    if(ImGui::Button("Delete binding")) {
                        state.document.reservedIds.push_back(state.binding);
                        std::erase_if(action->bindings,[&](const auto& b){return b.id==state.binding;});state.binding=0;
                    }
                }
                const auto live=input.Action(ActionId{action->id});
                ImGui::Separator();
                ImGui::Text("Live: down=%d pressed=%d released=%d canceled=%d (%d)",live.down,live.pressed,live.released,live.canceled,static_cast<int>(live.cancellation));
                ImGui::Text("Value: %.3f, %.3f | Map: %s | %s | %s",live.value.x,live.value.y,runtime.IsMapEnabled({action->map})?"enabled":"disabled",SteamInputPlatform::Status(),runtime.PromptScheme()==InputScheme::Gamepad?"Gamepad":"Keyboard / Mouse");
                GlyphPreview(runtime.Prompt({action->id},state.scheme==2 ? InputScheme::Gamepad : InputScheme::KeyboardMouse));
            }
        }
        ImGui::EndChild();ImGui::EndTable();
    }
    const auto afterText=SerializeInputDocument(state.document);
    if(beforeText!=afterText) {
        if(!historyOperation) {state.undo.push_back(before);state.redo.clear();if(state.undo.size()>128) state.undo.erase(state.undo.begin());}
        if(ValidateInputDocument(state.document,state.error)) runtime.QueueDocument(state.document,state.error);
    }
    if(!state.error.empty()) ImGui::TextWrapped("%s",state.error.c_str());
    ImGui::End();
}
}
#endif
