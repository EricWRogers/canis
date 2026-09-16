#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/App.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Editor.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#if CANIS_EDITOR
#include <imgui.h>
#include <imgui_stdlib.h>
#endif
namespace Canis::Scripting {
namespace {
ManagedJson manifest = ManagedJson::array();
ManagedJson FromYaml(const YAML::Node& node) {
    if (!node || node.IsNull()) return nullptr;
    if (node.IsSequence()) { auto a=ManagedJson::array(); for(auto x:node) a.push_back(FromYaml(x)); return a; }
    if (node.IsMap()) { auto a=ManagedJson::object(); for(auto x:node) a[x.first.as<std::string>()]=FromYaml(x.second); return a; }
    if(node.Tag()=="!") return node.Scalar();
    try { return ManagedJson::parse(node.Scalar()); } catch (...) { return node.Scalar(); }
}
YAML::Node ToYaml(const ManagedJson& json) { return YAML::Load(json.dump()); }
std::string Canonical(const std::string& type) {
    for(auto& d:manifest)if(d["id"]==type || std::find(d["aliases"].begin(),d["aliases"].end(),type)!=d["aliases"].end())return d["id"].get<std::string>();
    return type;
}
ManagedAttachment& Add(Entity& entity, const std::string& type) {
    auto& data=entity.GetOrAddComponent<ManagedComponents>();
    for(auto& a:data.items) if(Canonical(a->type)==Canonical(type)) throw std::invalid_argument("Component type already attached");
    auto a=std::make_unique<ManagedAttachment>();a->type=type;a->token=NextManagedToken();
    data.items.push_back(std::move(a));return *data.items.back();
}
}
bool HasManagedScripts(Scene& scene) {
    auto view=scene.GetRegistry().view<ManagedComponents>();
    for(auto handle:view)if(!view.get<ManagedComponents>(handle).items.empty())return true;
    return false;
}
uint64_t NextManagedToken() { static uint64_t next=1;return next++; }
ManagedJson EncodeAttachments(Entity& entity) {
    auto list=ManagedJson::array();
    if(auto* data=entity.TryGetComponent<ManagedComponents>()) for(auto& a:data->items) {
        auto fields=a->fields;
        for(auto& [name,reference]:a->references) fields[name]={{"entity",std::to_string(static_cast<uint64_t>(reference.GetUUID()))}};
        list.push_back({{"type",a->type},{"token",std::to_string(a->token)},{"enabled",a->enabled},{"fields",fields}});
    }
    return list;
}
void EncodeManagedComponents(YAML::Node& node, Entity& entity) {
    if(!entity.HasComponent<ManagedComponents>())return;
    auto list=EncodeAttachments(entity);for(auto& a:list)a.erase("token");
    node["Canis::ManagedScripts"]=ToYaml(list);
}
void DecodeManagedComponents(const YAML::Node& node, Entity& entity) {
    if(!node["Canis::ManagedScripts"])return;
    auto list=FromYaml(node["Canis::ManagedScripts"]);
    if(!list.is_array())throw std::invalid_argument("ManagedScripts must be an array");
    for(auto& item:list) {
        auto& a=Add(entity,item.at("type").get<std::string>());
        a.enabled=item.value("enabled",true);a.fields=item.value("fields",ManagedJson::object());
        for(auto& [name,value]:a.fields.items())if(value.is_object() && value.contains("entity")) {
            auto id=value["entity"].is_string()?std::stoull(value["entity"].get<std::string>()):value["entity"].get<uint64_t>();
            entity.scene.GetEntityAfterLoad(UUID(id),a.references[name]);
        }
    }
}
void RegisterManagedBindings(App& app, std::function<uint64_t(Entity*)> handle, std::function<Entity(uint64_t)> resolve) {
    auto& r=NativeBindingRegistry::Get();manifest=ManagedJson::array();constexpr auto owner="Canis.Scene";
    r.Method(owner,"Managed","Publish",[](std::string json){manifest=ManagedJson::parse(json);});
    r.Method(owner,"Managed","Snapshot",[&app,handle]()->std::string {
        auto result=ManagedJson::array();
        for(auto* e:app.scene.GetEntities())if(e && e->IsValid())result.push_back({{"handle",std::to_string(handle(e))},{"uuid",std::to_string(static_cast<uint64_t>(e->GetUUID()))},{"active",e->IsActive()},{"scripts",EncodeAttachments(*e)}});
        return result.dump();
    });
    r.Method(owner,"Managed","Add",[resolve](uint64_t id,std::string type)->uint64_t {auto e=resolve(id);return Add(e,type).token;});
    r.Method(owner,"Managed","Remove",[resolve](uint64_t id,uint64_t token){auto e=resolve(id);if(auto* d=e.TryGetComponent<ManagedComponents>())std::erase_if(d->items,[&](auto& a){return a->token==token;});});
    r.Method(owner,"Managed","Enabled",[resolve](uint64_t id,uint64_t token,bool enabled){auto e=resolve(id);if(auto* d=e.TryGetComponent<ManagedComponents>())for(auto& a:d->items)if(a->token==token){a->enabled=enabled;return;}throw std::runtime_error("Component detached");});
    r.Method(owner,"Managed","Valid",[resolve](uint64_t id,uint64_t token)->bool {try {auto e=resolve(id);if(auto* d=e.TryGetComponent<ManagedComponents>())for(auto& a:d->items)if(a->token==token)return true;}catch(...){}return false;});
    r.Method(owner,"Scene","FromUUID",[&app,handle](uint64_t id)->uint64_t{return handle(app.scene.FindEntity(UUID(id)));});
    r.Method(owner,"Scene","UUID",[resolve](uint64_t id)->uint64_t{return static_cast<uint64_t>(resolve(id).GetUUID());});
    r.Method(owner,"Scene","Create",[&app,handle](std::string name)->uint64_t {return handle(app.scene.CreateEntity(name));});
    r.Method(owner,"Scene","Destroy",[resolve](uint64_t id){resolve(id).Destroy();});
    r.Method(owner,"Scene","SetName",[resolve](uint64_t id,std::string name){resolve(id).SetName(name);});
    r.Method(owner,"Assets","Path",[](uint64_t id)->std::string{return AssetManager::GetPath(UUID(id));});
}
void DrawManagedInspector(Editor& editor, Entity& entity) {
#if CANIS_EDITOR
    if(auto* data=entity.TryGetComponent<ManagedComponents>()) {
        for(size_t i=0;i<data->items.size();) {
            auto& a=*data->items[i];ImGui::PushID(static_cast<int>(a.token));
            const ManagedJson* description=nullptr;
            for(auto& d:manifest)if(d["id"]==a.type || std::find(d["aliases"].begin(),d["aliases"].end(),a.type)!=d["aliases"].end()){description=&d;break;}
            std::string title=description?(*description)["name"].get<std::string>():"Missing C# script: "+a.type;
            bool open=ImGui::CollapsingHeader(title.c_str(),ImGuiTreeNodeFlags_DefaultOpen);
            if(ImGui::BeginPopupContextItem("script")) {
                if(description && description->contains("source") && ImGui::MenuItem("Open C# script"))editor.OpenScriptDocument((*description)["source"].get<std::string>());
                if(ImGui::MenuItem("Remove")){data->items.erase(data->items.begin()+i);ImGui::EndPopup();ImGui::PopID();continue;}
                ImGui::EndPopup();
            }
            if(open) {
                ImGui::Checkbox("Enabled",&a.enabled);
                if(!description)ImGui::TextWrapped("Saved fields are preserved. Restore the type or declare its former ID with ScriptAlias.");
                else for(auto& field:(*description)["fields"]) {
                    auto name=field["name"].get<std::string>(),kind=field["kind"].get<std::string>();
                    auto value=a.fields.contains(name)?a.fields[name]:field["default"];
                    if(!a.fields.contains(name))for(auto& alias:field.value("aliases",ManagedJson::array()))if(a.fields.contains(alias.get<std::string>())){value=a.fields[alias.get<std::string>()];break;}
                    bool changed=false;ImGui::PushID(name.c_str());
                    if(kind=="bool"){bool v=value.is_boolean()?value.get<bool>():false;changed=ImGui::Checkbox(name.c_str(),&v);value=v;}
                    else if(kind=="int"){int v=value.is_number()?value.get<int>():0;changed=ImGui::InputInt(name.c_str(),&v);value=v;}
                    else if(kind=="float" || kind=="double"){double v=value.is_number()?value.get<double>():0;changed=ImGui::InputDouble(name.c_str(),&v);value=v;}
                    else if(kind=="enum") {std::string v=value.is_string()?value.get<std::string>():"";if(ImGui::BeginCombo(name.c_str(),v.c_str())){for(auto& option:field["options"]){auto text=option.get<std::string>();if(ImGui::Selectable(text.c_str(),text==v)){value=text;changed=true;}}ImGui::EndCombo();}}
                    else if(kind=="vector3" || kind=="vector4" || kind=="quaternion") {float v[4]={0,0,0,kind=="quaternion"?1.f:0.f};int n=kind=="vector3"?3:4;for(int j=0;j<n && value.is_array() && j<value.size();++j)v[j]=value[j].get<float>();changed=n==3?ImGui::InputFloat3(name.c_str(),v):ImGui::InputFloat4(name.c_str(),v);value=ManagedJson::array();for(int j=0;j<n;++j)value.push_back(v[j]);}
                    else if(kind=="entity") {
                        auto& ref=a.references[name];if(!ref && value.is_object() && value.contains("entity"))ref=entity.scene.FindEntity(UUID(std::stoull(value["entity"].get<std::string>())));
                        const char* preview=ref?ref.GetName().c_str():"None";
                        if(ImGui::BeginCombo(name.c_str(),preview)){if(ImGui::Selectable("None",!ref)){ref=nullptr;changed=true;}for(auto* e:entity.scene.GetEntities())if(e && e->IsValid()){ImGui::PushID(e);if(ImGui::Selectable(e->GetName().c_str(),ref==e)){ref=e;changed=true;}ImGui::PopID();}ImGui::EndCombo();}
                        value={{"entity",std::to_string(static_cast<uint64_t>(ref.GetUUID()))}};
                    }
                    else {std::string v=value.is_string()?value.get<std::string>():"";changed=ImGui::InputText(name.c_str(),&v);value=v;}
                    if((kind=="entity" || kind=="asset") && ImGui::BeginDragDropTarget()) {
                        if(kind=="entity")if(auto* payload=ImGui::AcceptDragDropPayload("ENTITY_DRAG")) {
                            auto id=*static_cast<const UUID*>(payload->Data);a.references[name]=entity.scene.FindEntity(id);value={{"entity",std::to_string(static_cast<uint64_t>(id))}};changed=true;
                        }
                        if(kind=="asset")if(auto* payload=ImGui::AcceptDragDropPayload("ASSET_DRAG")) {
                            auto& data=*static_cast<const AssetDragData*>(payload->Data);value=std::to_string(static_cast<uint64_t>(data.uuid));changed=true;
                        }
                        ImGui::EndDragDropTarget();
                    }
                    if(changed)a.fields[name]=value;ImGui::PopID();
                }
            }
            ImGui::PopID();++i;
        }
    }
    ImGui::Button("Drop C# script to attach");
    if(ImGui::BeginDragDropTarget()) {
        if(auto* payload=ImGui::AcceptDragDropPayload("ASSET_DRAG")) {
            auto& data=*static_cast<const AssetDragData*>(payload->Data);
            const auto dropped=std::filesystem::path(data.path).lexically_normal();
            for(auto& d:manifest) {
                const auto source=std::filesystem::path(d.value("source","")).lexically_normal();
                if(source.empty() || std::filesystem::absolute(source)!=std::filesystem::absolute(dropped))continue;
                auto id=d["id"].get<std::string>();bool exists=false;if(auto* components=entity.TryGetComponent<ManagedComponents>())for(auto& a:components->items)if(Canonical(a->type)==Canonical(id))exists=true;
                if(!exists){auto& a=Add(entity,id);for(auto& f:d["fields"])a.fields[f["name"].get<std::string>()]=f["default"];}
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::TextUnformatted("Add C# Component");
    ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##AddManagedComponent","Choose script")) {
        for(auto& d:manifest) {auto id=d["id"].get<std::string>();bool exists=false;if(auto* data=entity.TryGetComponent<ManagedComponents>())for(auto& a:data->items)if(Canonical(a->type)==Canonical(id))exists=true;
            if(!exists && ImGui::Selectable(d["name"].get<std::string>().c_str())) {auto& a=Add(entity,id);for(auto& f:d["fields"])a.fields[f["name"].get<std::string>()]=f["default"];}
        }
        ImGui::EndCombo();
    }
#endif
}
}
