#include <Canis/Scripting/CSharpRuntime.hpp>
#include <Canis/Scripting/SceneBindings.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/App.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Components.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
namespace fs=std::filesystem;
using namespace Canis;
using namespace Canis::Scripting;
static void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::string Read(const fs::path& p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
static void Write(const fs::path& p,const std::string& s){std::ofstream(p)<<s;}
template<class F> void Wait(CSharpRuntime& r,F predicate,bool play=false){for(int i=0;i<1000;++i){r.Tick(play,false,.01f);if(predicate())return;SDL_Delay(25);}throw std::runtime_error(r.Status()+"\n"+r.BuildOutput());}
int main(){
    SDL_Init(0);auto root=fs::temp_directory_path()/("canis-components-"+std::to_string(SDL_GetTicksNS()));fs::create_directories(root/"assets");
    try {
        App app; Editor editor; app.RegisterDefaults(editor); app.scene.app=&app;RegisterSceneBindings(app);
        struct BindingCleanup { ~BindingCleanup() { UnregisterSceneBindings(); } } bindingCleanup;
        auto nodes=YAML::Load(R"(
- Entity: 101
  Name: Owner
  Canis::ManagedScripts:
  - type: test.probe
    enabled: true
    fields: {count: 13, Target: {entity: "102"}}
- {Entity: 102, Name: Target}
)");
        auto entities=app.scene.LoadEntityNodes(nodes);
        entities[0]->AddComponent<Transform>();
        entities[1]->AddComponent<Transform>();
        auto prefab=root/"assets/Example.scene";
        Write(prefab,"Entities:\n  - Entity: 901\n    Name: Spawned\n    Canis::Transform: {position: [1, 0, 0], rotation: [0, 0, 0], scale: [1, 1, 1]}\n");
        const auto prefabId=static_cast<uint64_t>(AssetManager::GetMetaFile(prefab.string())->uuid);
        auto& initial=entities[0]->GetComponent<ManagedComponents>().items[0]->fields;
        initial["Prefab"]=std::to_string(prefabId);
        initial["Scene"]=std::to_string(prefabId);
        for(auto [field,extension]:std::vector<std::pair<std::string,std::string>>{{"Audio","wav"},{"Mesh","glb"},{"Material","material"},{"Texture","png"}}) {
            auto path=root/"assets"/("fixture."+extension);Write(path,"metadata-only fixture");
            initial[field]=std::to_string(static_cast<uint64_t>(AssetManager::GetMetaFile(path.string())->uuid));
        }
        initial["TargetTransform"]={{"entity","102"}};
        auto encoded=app.scene.EncodeEntity(*entities[0]);
        Check(encoded["Canis::ManagedScripts"][0]["fields"]["count"].as<int>()==13,"Serialized field lost");
        auto trace=root/"trace.txt",file=root/"assets/Probe.cs";
        auto source=[&](std::string name,std::string field){return std::string(R"(using Canis;using System.IO;
[ScriptId("test.data")] public class Counter:Component { public int Value=5; }
[ScriptId("test.fault")] public class Fault:ScriptableEntity { public override void Update(float dt) { throw new Exception("EXPECTED_CALLBACK_FAILURE"); } }
[ScriptId("test.probe")] public class )")+name+R"(:ScriptableEntity {
[SerializeField,FormerlySerializedAs("count")] private int )"+field+R"(=7;
public Entity? Target;
[Header("References"),Tooltip("Spawn template")] public PrefabAsset? Prefab;
public Transform? TargetTransform;
public Counter? Data;
public AudioClip? Audio;
public SceneAsset? Scene;
public ModelAsset? Mesh;
public MaterialAsset? Material;
public TextureAsset? Texture;
[NonSerialized]public int Transient;
void Emit(string s)=>File.AppendAllText(@")"+trace.string()+R"(",s+"\n");
public override void Awake(){Emit("awake:"+)"+field+R"(+":"+Target?.Name);}
public override void OnEnable()=>Emit("enable");
public override void Start(){
 Emit("start");if(GetComponent<Counter>() is null)AddComponent<Counter>();
 if(TargetTransform?.Entity!=Target || Prefab is null || !Prefab.IsValid)throw new Exception("Typed reference restore failed");
 if(Audio?.IsValid!=true || Scene?.IsValid!=true || Mesh?.IsValid!=true || Material?.IsValid!=true || Texture?.IsValid!=true)throw new Exception("Typed assets did not survive serialization");
 if(RestoredFromReload && Data!=GetComponent<Counter>())throw new Exception("Managed component reference lost during reload");
 Data=GetComponent<Counter>();
 var parent=Canis.Entity.Create("Parent");parent.AddComponent<Transform>().Position=new(10,0,0);
 var child=Canis.Entity.Create("Child");var childTransform=child.AddComponent<Transform>();childTransform.Position=new(2,0,0);
 childTransform.SetParent(parent);
 try{childTransform.SetParent(child);throw new Exception("Self parent allowed");}catch(InvalidOperationException){}
 try{parent.Transform.SetParent(child);throw new Exception("Hierarchy cycle allowed");}catch(InvalidOperationException){}
 if(childTransform.Parent!=parent || Math.Abs(childTransform.Position.X-2)>.001f)throw new Exception("World parenting failed");
 childTransform.SetParent(null);childTransform.SetParent(parent,false);
 if(Math.Abs(childTransform.Position.X-12)>.001f)throw new Exception("Local parenting failed");
 child.Destroy();parent.Destroy();
 var spawned=Prefabs.Instantiate(Prefab);
 if(spawned.Length!=1 || spawned[0].Name!="Spawned")throw new Exception("Prefab instantiate failed");
 spawned[0].Destroy();
 using(var pool=new EntityPool(()=>Canis.Entity.Create("Pooled"),1)) {
  var first=pool.Rent();pool.Release(first);if(first.Active)throw new Exception("Pool release did not deactivate");
  var again=pool.Rent();if(again!=first || !again.Active)throw new Exception("Pool did not reuse entity");
  pool.Release(again);try{pool.Release(again);throw new Exception("Double release allowed");}catch(InvalidOperationException){}
 }
 Emit("gameplay-apis");
 if(Entity.NativeComponentTypes.Length<35)throw new Exception("Native registry coverage missing");
 foreach(var name in Entity.NativeComponentTypes.Where(n=>n.StartsWith("Canis::")))
  if(typeof(Component).Assembly.GetType("Canis."+name.Split("::")[1]) is null)throw new Exception("Missing native wrapper: "+name);
 var audio=Entity.GetComponent<AudioSource>()??Entity.AddComponent<AudioSource>();
 audio.PlayOnAwake=false;audio.SpatialBlend=.75f;audio.ClipPath="";
 if(audio.SpatialBlend!=.75f || audio.Play() || audio.IsPlaying)throw new Exception("AudioSource facade failed");
 audio.Pause();audio.UnPause();audio.Stop();
 Entity.RemoveComponent<AudioSource>();if(audio.IsValid)throw new Exception("AudioSource wrapper survived removal");
 var listener=Entity.GetComponent<AudioListener>()??Entity.AddComponent<AudioListener>();listener.FollowHeadset=false;
 if(listener.FollowHeadset)throw new Exception("AudioListener facade failed");
 var light=Entity.GetComponent<PointLight>()??Entity.AddComponent<PointLight>();
 light.Intensity=2.5f;light.Range=7;light.Color=new System.Numerics.Vector4(.2f,.4f,.6f,1);
 if(light.Intensity!=2.5f || light.Range!=7 || light.Color.Y!=.4f)throw new Exception("Native fields failed round trip");
 try {light.SetField("typo",1);throw new Exception("Unknown field accepted");}catch(InvalidOperationException){}
 try {light.ApplyFields(new(){["intensity"]=8,["range"]="invalid"});throw new Exception("Malformed fields accepted");}catch(InvalidOperationException){}
 if(light.Intensity!=2.5f || light.Range!=7)throw new Exception("Failed configuration edit was not rolled back");
 Entity.RemoveComponent<PointLight>();Entity.AddComponent<PointLight>();
 if(light.IsValid)throw new Exception("Removed light wrapper revived");
 Emit("native-coverage");
}
public override void Update(float dt){
 if(Transient++==0){var old=Transform;Entity.RemoveComponent<Transform>();Entity.AddComponent<Transform>();if(old.IsValid)throw new Exception("Old wrapper revived");Emit("wrapper-invalid");}
 ++)"+field+R"(;Emit("update:"+)"+field+R"();
 int value=World.Query<Counter>().Single().Value;if(value!=5 && value!=9)throw new Exception("Class query failed");Emit("data:"+value);
}
public override void OnDisable()=>Emit("disable");
public override void OnDestroy()=>Emit("destroy");
})";};
        Write(file,source("Probe","count"));
        {
            CSharpRuntime runtime(root/"assets",root/"cache");Wait(runtime,[&]{return runtime.ReadyToPlay();});
            runtime.Tick(true,false,.01f);Check(Read(trace).find("awake:13:Target")!=std::string::npos,"Attachment/serialized references not restored before Awake");
            Check(Read(trace).find("wrapper-invalid")!=std::string::npos,"Native component lifetime guard not exercised");
            Check(Read(trace).find("gameplay-apis")!=std::string::npos,"Typed references, parenting, prefab or pooling failed");
            auto& attachments=entities[0]->GetComponent<ManagedComponents>();
            attachments.items[1]->fields["Value"]=9;runtime.Tick(true,false,.01f);
            Check(Read(trace).find("data:9")!=std::string::npos,"Runtime-added data component ignored Inspector edit");
            attachments.items[0]->enabled=false;
            runtime.Tick(true,false,.01f);Check(Read(trace).find("disable")!=std::string::npos,"OnDisable missing");
            auto paused=Read(trace);runtime.Tick(true,true,.01f);Check(Read(trace)==paused,"Paused component updated");
            attachments.items[0]->enabled=true;attachments.items[0]->fields["count"]=21;runtime.Tick(true,false,.01f);
            Check(Read(trace).find("update:22")!=std::string::npos,"Inspector edits were not applied during Play");
            runtime.SetLiveReload(true);Write(file,source("RenamedProbe","renamedCount"));runtime.RefreshSources();
            Wait(runtime,[&]{return runtime.Status().find("stateful reload applied")!=std::string::npos;},true);
            DecodeManagedComponents(YAML::Load("Canis::ManagedScripts: [{type: test.fault, enabled: true, fields: {}}]"),*entities[1]);
            runtime.Tick(true,false,.01f);
            auto diagnostics=Debug::GetEntries();
            Check(std::any_of(diagnostics.begin(),diagnostics.end(),[&](const auto& entry){return entry.file==file.string() && entry.line>0 && entry.message.find("Fault on Target (UUID 102)")!=std::string::npos;}),"Runtime error lost source location or entity ownership");
            auto log=Read(trace);Check(log.find("awake:7:")==std::string::npos,"Renamed field reset to initializer during live reload");
            Check(log.find("destroy")!=std::string::npos,"Old component was not destroyed on reload");
            Check(std::count(log.begin(),log.end(),'\n')>10,"Reload did not continue");
            auto badType=source("RenamedProbe","renamedCount");
            badType.replace(badType.find("public PrefabAsset? Prefab"),std::string("public PrefabAsset? Prefab").size(),"public TextureAsset? Prefab");
            badType.replace(badType.find("Prefabs.Instantiate(Prefab)"),std::string("Prefabs.Instantiate(Prefab)").size(),"Prefabs.Instantiate(new PrefabAsset(Prefab.UUID))");
            Write(file,badType);runtime.RefreshSources();
            Wait(runtime,[&]{return runtime.Status().find("live reload rejected")!=std::string::npos;},true);
            auto stillRunning=Read(trace).size();runtime.Tick(true,false,.01f);
            Check(Read(trace).size()>stillRunning,"Incompatible reference reload stopped active generation");
            runtime.SetLiveReload(true);Write(file,source("RenamedProbe","renamedCount"));runtime.RefreshSources();
            Wait(runtime,[&]{return runtime.Status().find("stateful reload applied")!=std::string::npos;},true);
            auto before=log.size();Write(file,"invalid C#");runtime.RefreshSources();Wait(runtime,[&]{return runtime.HasBuildError();},true);
            runtime.Tick(true,false,.01f);Check(Read(trace).size()>before,"Failed candidate stopped active component");
            runtime.StopSession();
            // Missing types remain serialized and can be restored later.
            Check(app.scene.EncodeEntity(*entities[0])["Canis::ManagedScripts"].size()==2,"Managed membership not mirrored to native scene");
        }
        YAML::Node duplicateNodes(YAML::NodeType::Sequence);
        for(auto* e:app.scene.GetEntities())if(e && e->IsValid())duplicateNodes.push_back(app.scene.EncodeEntity(*e));
        // Duplication remaps declared entity references through native load fixups.
        auto copied=app.scene.LoadEntityNodes(duplicateNodes,false);
        auto copyData=EncodeAttachments(*copied[0]);
        auto referenced=copyData[0]["fields"]["Target"]["entity"].get<std::string>();
        Check(referenced==std::to_string(static_cast<uint64_t>(copied[1]->GetUUID())),"Duplicate retained source entity reference");
        UnregisterSceneBindings();app.scene.Unload();fs::remove_all(root);SDL_Quit();std::cout<<"C# component lifecycle, persistence, class queries and reload passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\nFixtures: "<<root<<'\n';return 1;}
}
