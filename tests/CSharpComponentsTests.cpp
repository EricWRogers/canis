#include <Canis/Scripting/CSharpRuntime.hpp>
#include <Canis/Scripting/SceneBindings.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/App.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Components.hpp>
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
        auto encoded=app.scene.EncodeEntity(*entities[0]);
        Check(encoded["Canis::ManagedScripts"][0]["fields"]["count"].as<int>()==13,"Serialized field lost");
        auto trace=root/"trace.txt",file=root/"assets/Probe.cs";
        auto source=[&](std::string name,std::string field){return std::string(R"(using Canis;using System.IO;
[ScriptId("test.data")] public class Counter:Component { public int Value=5; }
[ScriptId("test.probe")] public class )")+name+R"(:ScriptableEntity {
[SerializeField,FormerlySerializedAs("count")] private int )"+field+R"(=7;
public Entity? Target;
[NonSerialized]public int Transient;
void Emit(string s)=>File.AppendAllText(@")"+trace.string()+R"(",s+"\n");
public override void Awake(){Emit("awake:"+)"+field+R"(+":"+Target?.Name);}
public override void OnEnable()=>Emit("enable");
public override void Start(){
 Emit("start");if(GetComponent<Counter>() is null)AddComponent<Counter>();
 if(Entity.NativeComponentTypes.Length<33)throw new Exception("Native registry coverage missing");
 foreach(var name in Entity.NativeComponentTypes.Where(n=>n.StartsWith("Canis::")))
  if(typeof(Component).Assembly.GetType("Canis."+name.Split("::")[1]) is null)throw new Exception("Missing native wrapper: "+name);
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
            auto log=Read(trace);Check(log.find("awake:7:")==std::string::npos,"Renamed field reset to initializer during live reload");
            Check(log.find("destroy")!=std::string::npos,"Old component was not destroyed on reload");
            auto before=log.size();Write(file,"invalid C#");runtime.RefreshSources();Wait(runtime,[&]{return runtime.HasBuildError();},true);
            runtime.Tick(true,false,.01f);Check(Read(trace).size()>before,"Failed candidate stopped active component");
            runtime.StopSession();
            // Missing types remain serialized and can be restored later.
            Check(app.scene.EncodeEntity(*entities[0])["Canis::ManagedScripts"].size()==2,"Managed membership not mirrored to native scene");
        }
        YAML::Node duplicateNodes(YAML::NodeType::Sequence);
        for(auto* e:app.scene.GetEntities())duplicateNodes.push_back(app.scene.EncodeEntity(*e));
        // Duplication remaps declared entity references through native load fixups.
        auto copied=app.scene.LoadEntityNodes(duplicateNodes,false);
        auto copyData=EncodeAttachments(*copied[0]);
        auto referenced=copyData[0]["fields"]["Target"]["entity"].get<std::string>();
        Check(referenced==std::to_string(static_cast<uint64_t>(copied[1]->GetUUID())),"Duplicate retained source entity reference");
        UnregisterSceneBindings();app.scene.Unload();fs::remove_all(root);SDL_Quit();std::cout<<"C# component lifecycle, persistence, class queries and reload passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\nFixtures: "<<root<<'\n';return 1;}
}
