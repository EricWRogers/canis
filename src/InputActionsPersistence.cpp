#include <Canis/InputActions.hpp>
#include <Canis/Debug.hpp>
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <cmath>
#include <Canis/Canis.hpp>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace Canis
{
namespace
{
const char* TypeName(ActionType type) { return type == ActionType::Button ? "Button" : type == ActionType::Axis1D ? "Axis1D" : "Axis2D"; }
YAML::Node BindingNode(const InputBinding& b)
{
    YAML::Node n;
    n["id"] = b.id; n["scheme"] = b.scheme == InputScheme::Gamepad ? "Gamepad" : "KeyboardMouse";
    n["path"] = b.path; n["primaryPrompt"] = b.primaryPrompt;
    if(!b.iconOverride.empty()) n["iconOverride"] = b.iconOverride;
    std::vector<std::string> parts;
    for (const auto& [name,path] : b.parts) parts.push_back(name);
    std::sort(parts.begin(),parts.end());
    for (const auto& name : parts) n["parts"][name] = b.parts.at(name);
    n["deadZone"] = b.deadZone; n["scale"] = b.scale; n["invert"] = b.invert;
    n["pressThreshold"] = b.pressThreshold; n["releaseThreshold"] = b.releaseThreshold;
    return n;
}
bool WriteAtomic(const std::string& path, const std::string& text, std::string& error)
{
    namespace fs = std::filesystem;
    const fs::path target(path), temporary(path + ".tmp");
    try {
        if (!target.parent_path().empty()) fs::create_directories(target.parent_path());
        { std::ofstream file(temporary, std::ios::trunc); file << text; file.flush(); if (!file) throw std::runtime_error("Cannot write " + path); }
        std::error_code ec;
        fs::rename(temporary,target,ec);
        // Windows does not replace an existing destination with filesystem::rename.
        if (ec && fs::exists(target)) {
            const fs::path backup(path + ".bak");
            fs::copy_file(target,backup,fs::copy_options::overwrite_existing);
            fs::remove(target);
            fs::rename(temporary,target,ec);
            if (ec) { fs::rename(backup,target); throw std::runtime_error(ec.message()); }
            fs::remove(backup);
        } else if (ec) throw std::runtime_error(ec.message());
#ifdef __EMSCRIPTEN__
        EM_ASM({ FS.syncfs(false, function(error) { if(error) console.error('Input settings save failed', error); }); });
#endif
        error.clear(); return true;
    } catch (const std::exception& e) { error=e.what(); return false; }
}
}
std::string SerializeInputDocument(const InputDocument& document)
{
    YAML::Node root; root["version"] = 1;
    root["reservedIds"] = document.reservedIds;
    root["maps"] = YAML::Node(YAML::NodeType::Sequence);
    for (const auto& map : document.maps) {
        YAML::Node m; m["id"]=map.id; m["name"]=map.name;
        m["actions"] = YAML::Node(YAML::NodeType::Sequence);
        for (const auto& action : document.actions) if (action.map == map.id) {
            YAML::Node a; a["id"]=action.id; a["name"]=action.name; a["type"]=TypeName(action.type);
            a["bindings"] = YAML::Node(YAML::NodeType::Sequence);
            for (const auto& b : action.bindings) a["bindings"].push_back(BindingNode(b));
            m["actions"].push_back(a);
        }
        root["maps"].push_back(m);
    }
    YAML::Emitter emitter; emitter << root;
    return std::string(emitter.c_str()) + "\n";
}
bool SaveInputDocument(const std::string& path, const InputDocument& document, std::string& error)
{
    return ValidateInputDocument(document,error) && WriteAtomic(path,SerializeInputDocument(document),error);
}
std::string InputUserOverridesPath()
{
#ifdef __EMSCRIPTEN__
    return "/persistent/input-overrides.canis";
#else
    char* directory=SDL_GetPrefPath("Canis",GetProjectConfig().executableName.c_str());
    if(!directory) return {};
    std::string path=std::string(directory)+"input-overrides.canis"; SDL_free(directory); return path;
#endif
}
uint32_t AllocateInputId(const InputDocument& document)
{
    uint32_t maximum=0;
    for(auto id:document.reservedIds) maximum=std::max(maximum,id);
    for(const auto& map:document.maps) maximum=std::max(maximum,map.id);
    for(const auto& a:document.actions) {maximum=std::max(maximum,a.id); for(const auto& b:a.bindings) maximum=std::max(maximum,b.id);}
    if(maximum==UINT32_MAX) throw std::runtime_error("Input ID space exhausted");
    return maximum+1;
}
void InputActionSystem::BeginCapture(InputScheme scheme, ActionType type)
{
    m_captureScheme=scheme; m_captureType=type; m_capturing=true;
    m_captureArmed=false; m_captureResolved=false; m_captureResult.reset();
    m_captureStart=std::chrono::steady_clock::now(); Cancel(InputCancellation::Capture);
}
void InputActionSystem::CancelCapture()
{
    m_capturing=false; m_captureArmed=false; m_captureResolved=false; m_captureResult.reset();
    Cancel(InputCancellation::Capture);
}
std::optional<std::string> InputActionSystem::TakeCapturedControl()
{
    auto result=m_captureResult; m_captureResult.reset(); return result;
}
bool InputActionSystem::CaptureNeutral() const
{
    for(const auto& [sourceId,source]:m_sources) for(const auto& [path,value]:source.controls) {
        if(path=="Mouse/Delta" || path=="Mouse/Wheel") continue;
        if(glm::length(value)>0.25f) return false;
    }
    return true;
}
void InputActionSystem::CaptureEvent(const Event& event)
{
    if(!m_capturing) return;
    if(event.path=="Keyboard/Escape" && event.value.x>0.5f) {CancelCapture(); return;}
    if(!m_captureArmed) {if(CaptureNeutral()) m_captureArmed=true; return;}
    if(m_captureResolved) return;
    const auto* control=FindInputControl(event.path);
    if(!control || glm::length(event.value)<(event.path=="Mouse/Delta" ? 5.0f : 0.6f)) return;
    if(event.path.starts_with("Gamepad/") != (m_captureScheme==InputScheme::Gamepad)) return;
    if(control->type!=m_captureType && !(m_captureType==ActionType::Button && control->type==ActionType::Axis1D)) return;
    m_captureResult=event.path; m_captureResolved=true;
}
bool InputActionSystem::Rebind(ActionId action, uint32_t binding, const InputBinding& replacement, std::string& error)
{
    if(m_nativeInput && replacement.scheme==InputScheme::Gamepad) {error="Steam controller bindings are edited in Steam's configurator";return false;}
    InputDocument next=m_pendingDocument ? *m_pendingDocument : m_document;
    bool found=false;
    for(auto& a:next.actions) if(a.id==action.value) for(auto& b:a.bindings) if(b.id==binding) {b=replacement; b.id=binding; found=true;}
    if(!found) {error="Unknown action or binding ID"; return false;}
    const auto defaults=m_defaults;
    bool valid=QueueDocument(next,error); m_defaults=defaults; return valid;
}
void InputActionSystem::ResetBindings(std::optional<ActionId> action)
{
    InputDocument next=m_pendingDocument ? *m_pendingDocument : m_document;
    for(auto& a:next.actions) if(!action || a.id==action->value)
        for(const auto& original:m_defaults.actions) if(a.id==original.id) a.bindings=original.bindings;
    m_pendingDocument=std::move(next);
}
std::vector<ActionId> InputActionSystem::Conflicts(ActionId action, const InputBinding& binding) const
{
    std::vector<ActionId> result;
    auto paths=[](const InputBinding& b) {
        std::set<std::string> result;
        if(b.parts.empty()) result.insert(b.path);
        else for(const auto& [part,path]:b.parts) result.insert(path);
        return result;
    };
    const auto proposed=paths(binding);
    for(const auto& a:m_document.actions) if(a.id!=action.value) {
        bool shared=false;
        for(const auto& b:a.bindings) if(b.scheme==binding.scheme)
            for(const auto& path:paths(b)) if(proposed.contains(path)) shared=true;
        if(shared) result.push_back({a.id});
    }
    return result;
}
bool InputActionSystem::SaveOverrides(const std::string& path, std::string& error) const
{
    YAML::Node root; root["version"]=1; root["overrides"]=YAML::Node(YAML::NodeType::Sequence);
    const auto& doc=m_pendingDocument ? *m_pendingDocument : m_document;
    for(const auto& a:doc.actions) for(const auto& b:a.bindings) {
        bool changed=false;
        for(const auto& original:m_defaults.actions) if(original.id==a.id)
            for(const auto& defaultBinding:original.bindings) if(defaultBinding.id==b.id)
                changed=YAML::Dump(BindingNode(b))!=YAML::Dump(BindingNode(defaultBinding));
        if(changed) {YAML::Node n; n["action"]=a.id; n["binding"]=BindingNode(b); root["overrides"].push_back(n);}
    }
    return WriteAtomic(path,YAML::Dump(root)+"\n",error);
}
bool InputActionSystem::LoadOverrides(const std::string& path, std::string& error)
{
    if(!std::filesystem::exists(path)) {error.clear(); return true;}
    try {
        const auto root=YAML::LoadFile(path);
        if(root["version"].as<int>()!=1 || !root["overrides"].IsSequence()) throw std::runtime_error("Invalid input overrides document");
        auto serialized=YAML::Load(SerializeInputDocument(m_defaults));
        std::set<uint32_t> seen;
        for(const auto& n:root["overrides"]) {
            const auto action=n["action"].as<uint32_t>(); const auto id=n["binding"]["id"].as<uint32_t>();
            if(!seen.insert(id).second) throw std::runtime_error("Duplicate override ID");
            bool found=false;
            for(auto map:serialized["maps"]) for(auto a:map["actions"]) if(a["id"].as<uint32_t>()==action)
                for(size_t i=0;i<a["bindings"].size();++i) if(a["bindings"][i]["id"].as<uint32_t>()==id) {a["bindings"][i]=n["binding"]; found=true;}
            if(!found) Debug::Warning("Ignoring obsolete input override: %u",id);
        }
        InputDocument next;
        if(!ParseInputDocument(YAML::Dump(serialized),next,error)) throw std::runtime_error(error);
        const auto defaults=m_defaults; bool valid=QueueDocument(next,error); m_defaults=defaults; return valid;
    } catch(const std::exception& e) {ResetBindings(); error=e.what(); return false;}
}
void InputActionSystem::SetGlyphCatalog(const std::string& path)
{
    m_glyphs.clear(); m_glyphLabels.clear();
    try {
        const auto root=YAML::LoadFile(path);
        if(root["version"].as<int>()!=1 || !root["glyphs"].IsSequence()) throw std::runtime_error("Invalid glyph catalog");
        for(const auto& n:root["glyphs"]) {
            InputGlyph glyph; glyph.texture=n["texture"].as<std::string>();
            for(int i=0;i<4;++i) {
                glyph.uv[i]=n["uv"][i].as<float>();
                if(!std::isfinite(glyph.uv[i]) || glyph.uv[i]<0 || glyph.uv[i]>1) throw std::runtime_error("Invalid glyph UV");
            }
            if(glyph.uv.z<=glyph.uv.x || glyph.uv.w<=glyph.uv.y) throw std::runtime_error("Empty glyph rectangle");
            const auto key=n["family"].as<std::string>()+":"+n["path"].as<std::string>();
            m_glyphs[key]=glyph;
            if(n["label"]) m_glyphLabels[key]=n["label"].as<std::string>();
        }
    } catch(const std::exception& e) {m_glyphs.clear();m_glyphLabels.clear();Debug::Warning("Input glyph catalog unavailable: %s",e.what());}
}
}
