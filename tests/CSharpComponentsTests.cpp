#include <Canis/PlayerPrefs.hpp>
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
static void CheckScriptIdentityPersistence(Scene& scene) {
    // Scene saves may emit metadata UUIDs as bare YAML numbers. Read their
    // exact scalar spelling, including IDs above the signed 64-bit limit.
    for (const std::string id : {"2415903239614432105", "15598187761857189197", "18446744073709551615", "legacy.script.v1"}) {
        for (const std::string scalar : {id, "\"" + id + "\"", "!!str " + id}) {
            auto entity = scene.CreateEntity("Script identity regression");
            auto node = YAML::Load("Canis::ManagedScripts: [{type: " + scalar + ", enabled: false, fields: {count: 13}}]");
            for (int save = 0; save < 3; ++save) {
                DecodeManagedComponents(node, entity);
                const auto& attachment = *entity.GetComponent<ManagedComponents>().items.front();
                Check(attachment.type == id, "Script metadata UUID changed during scene loading");
                Check(!attachment.enabled && attachment.fields["count"] == 13, "Script fields or enabled state changed");
                YAML::Node encoded;
                EncodeManagedComponents(encoded, entity);
                // Exercise the file representation, not only in-memory YAML nodes.
                node = YAML::Load(YAML::Dump(encoded));
                entity.RemoveComponent<ManagedComponents>();
            }
            entity.Destroy();
        }
    }
}
int main(){
    SDL_Init(0);auto root=fs::temp_directory_path()/("canis-components-"+std::to_string(SDL_GetTicksNS()));fs::create_directories(root/"assets");
    try {
        PlayerPrefs::Init("Canis", "IntegrationTest", (root/"prefs.yaml").string());
        App app; Editor editor; app.RegisterDefaults(editor); app.scene.app=&app;RegisterSceneBindings(app);
        struct BindingCleanup { ~BindingCleanup() { UnregisterSceneBindings(); } } bindingCleanup;
        CheckScriptIdentityPersistence(app.scene);
        auto nodes=YAML::Load(R"(
- Entity: 101
  Name: Owner
  Canis::ManagedScripts:
  - type: test.probe
    enabled: true
    fields: {count: 13, Target: {entity: "102"}, Targets: [{entity: "102"}, {entity: "0"}], Transforms: [{entity: "102"}], Numbers: [3, 8], Modes: [First, Second]}
- {Entity: 102, Name: Target}
)");
        auto entities=app.scene.LoadEntityNodes(nodes);
        auto snapshot=[](const char* binding) { return ManagedJson::parse(Decode<std::string>(NativeBindingRegistry::Get().Invoke(binding,{}))); };
        Check(snapshot("Managed.Snapshot").size()==2,"Entity enumeration lost native-only entities");
        Check(snapshot("Managed.ScriptSnapshot").size()==1,"Script synchronization included native-only entities");
        entities[0]->AddComponent<Transform>();
        entities[1]->AddComponent<Transform>();
        auto prefab=root/"assets/Example.scene";
        Write(prefab,"Entities:\n  - Entity: 901\n    Name: Spawned\n    Canis::Transform: {position: [1, 0, 0], rotation: [0, 0, 0], scale: [1, 1, 1]}\n");
        const auto prefabId=static_cast<uint64_t>(AssetManager::GetMetaFile(prefab.string())->uuid);
        auto& initial=entities[0]->GetComponent<ManagedComponents>().items[0]->fields;
        initial["Prefab"]=std::to_string(prefabId);
        initial["Scene"]=std::to_string(prefabId);
        initial["Templates"]=ManagedJson::array({std::to_string(prefabId),"0"});
        for(auto [field,extension]:std::vector<std::pair<std::string,std::string>>{{"Audio","wav"},{"Mesh","glb"},{"Material","material"},{"Texture","png"}}) {
            auto path=root/"assets"/("fixture."+extension);Write(path,"metadata-only fixture");
            initial[field]=std::to_string(static_cast<uint64_t>(AssetManager::GetMetaFile(path.string())->uuid));
        }
        initial["TargetTransform"]={{"entity","102"}};
        initial["Palette"] = ManagedJson::array({ManagedJson::array({0.1f, 0.2f, 0.3f, 0.4f}), ManagedJson::array({0.5f, 0.6f, 0.7f, 0.8f})});
        initial["EmptyPalette"] = ManagedJson::array();
        initial["NullPalette"] = nullptr;
        auto encoded=app.scene.EncodeEntity(*entities[0]);
        Check(encoded["Canis::ManagedScripts"][0]["fields"]["count"].as<int>()==13,"Serialized field lost");
        auto savedPalette = YAML::Load(YAML::Dump(encoded))["Canis::ManagedScripts"][0]["fields"]["Palette"];
        Check(savedPalette.size() == 2 && std::abs(savedPalette[1][3].as<float>() - 0.8f) < 0.0001f,
              "Color array lost alpha or entries during scene save");
        auto trace=root/"trace.txt",file=root/"assets/Probe.cs";
        // The old textual attachment above must still load through the metadata alias.
        Write(file.string()+".meta", "FileType: CSHARP\nUUID: 16003035088673311249\nScriptAlias: test.probe\nname: Probe\nextension: cs\nsize: 0\nmodified: 0\n");
        auto source=[&](std::string name,std::string field){return std::string(R"(using Canis;using System.IO;using Canis.Tweening;
[ScriptId("test.data")] public class Counter:Component { public int Value=5; }
[ScriptId("test.remove")] public class RemoveButton:ScriptableEntity { public void RemoveOwner()=>Entity.Destroy(); }
[ScriptId("test.fault")] public class Fault:ScriptableEntity { public override void Update(float dt) { throw new Exception("EXPECTED_CALLBACK_FAILURE"); } }
public class )")+name+R"(:ScriptableEntity {
[SerializeField,FormerlySerializedAs("count")] private int )"+field+R"(=7;
public Entity? Target;
public Entity?[] Targets = [];
public Transform[] Transforms = [];
public int[] Numbers = [];
public enum Mode { First, Second }
public Mode[] Modes = [];
public PrefabAsset?[] Templates = [];
public Color Tint = new(.2f, .3f, .4f, .5f);
public Color[] Palette = { new(1, 0, 0) };
public Color[] EmptyPalette = { new(1, 1, 1) };
public Color[]? NullPalette = { new(1, 1, 1) };
[Header("References"),Tooltip("Spawn template")] public PrefabAsset? Prefab;
public Transform? TargetTransform;
public Counter? Data;
public AudioClip? Audio;
public SceneAsset? Scene;
public ModelAsset? Mesh;
public MaterialAsset? Material;
public TextureAsset? Texture;
[NonSerialized]public int Transient;
private float tweened;
public void ButtonClick()=>Emit("button:"+GetType().Name);
public void ButtonClick(int ignored)=>Emit("wrong-overload");
private void HiddenButton()=>Emit("hidden-button");
public int ReturningButton()=>42;
public void GenericButton<T>() { }
public static void StaticButton() { }
public async void AsyncButton(){await System.Threading.Tasks.Task.Yield();}
public void ThrowingButton()=>throw new Exception("EXPECTED_UI_CALLBACK_FAILURE");
void Emit(string s)=>File.AppendAllText(@")"+trace.string()+R"(",s+"\n");
public override void Awake(){Emit("awake:"+)"+field+R"(+":"+Target?.Name);}
public override void OnEnable()=>Emit("enable");
public override void Start(){
 PlayerPrefs.SetString("managed.text", "Chef é|hello\nworld");
 PlayerPrefs.SetInt("managed.int", 123);
 PlayerPrefs.SetFloat("managed.float", .003f);
 PlayerPrefs.SetBool("managed.bool", true);
 PlayerPrefs.Save();
 if (PlayerPrefs.GetString("managed.text") != "Chef é|hello\nworld" || PlayerPrefs.GetInt("managed.int") != 123 ||
     PlayerPrefs.GetFloat("managed.float") != .003f || !PlayerPrefs.GetBool("managed.bool"))
     throw new Exception("Managed PlayerPrefs roundtrip failed");
 PlayerPrefs.DeleteKey("managed.int");
 if (PlayerPrefs.HasKey("managed.int") || PlayerPrefs.GetInt("managed.int", 7) != 7)
     throw new Exception("Managed PlayerPrefs defaults/deletion failed");
 if (Palette.Length != 2 || Math.Abs(Palette[0].R - .1f) > .0001f || Math.Abs(Palette[1].A - .8f) > .0001f)
     throw new Exception("Color palette was not restored");
 if (EmptyPalette.Length != 0 || NullPalette != null || Tint.A != .5f)
     throw new Exception("Color defaults or empty/null arrays were not restored");
 System.Numerics.Vector4 rgba = Palette[1];
 Color roundTrip = rgba;
 if (roundTrip.G != Palette[1].G || rgba.W != Palette[1].A)
     throw new Exception("Color conversion failed");
 if(Targets.Length != 2 || Targets[0] != Target || Targets[1] is not null ||
    Transforms.Length != 1 || Transforms[0].Entity != Target || !Numbers.SequenceEqual(new[] {3,8}) ||
    !Modes.SequenceEqual(new[] {Mode.First,Mode.Second}) || Templates.Length != 2 || Templates[0]?.Path != Prefab?.Path || Templates[1] is not null)
     throw new Exception("Typed arrays were not restored before Start or after reload");
 Emit("arrays:" + RestoredFromReload);
 Emit("palette:" + RestoredFromReload);
 Tween.To(()=>tweened,v=>tweened=v,1f,100).SetLink(Entity).OnUpdate(()=>Emit("tween:"+GetType().Name));
 if(!Canis.Entity.All().Any(e=>e.Name=="Target"))throw new Exception("Entity.All lost native-only entities");
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
 if (parent.Descendants().Length != 1 || parent.Descendants()[0] != child || parent.Descendants(true).Length != 2)
     throw new Exception("Engine hierarchy traversal failed");
 var inputField = child.AddComponent<UIInputField>();
 inputField.MaxLength = 3;
 inputField.Text = "aé😀z";
 if (inputField.Text != "aé😀")
     throw new Exception("Managed input text length split UTF-8");
 inputField.Backspace();
 inputField.InsertText("b");
 if (inputField.Text != "aéb")
     throw new Exception("Managed input text editing failed");
 child.RemoveComponent<UIInputField>();
 try
 {
     inputField.InsertText("x");
     throw new Exception("Destroyed input wrapper accepted writes");
 }
 catch (MissingObjectException) { }
 try
 {
     Physics.Raycast(System.Numerics.Vector3.Zero, System.Numerics.Vector3.Zero, 1);
     throw new Exception("Invalid ray was accepted");
 }
 catch (InvalidOperationException) { }
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
 Emit("numbers:"+string.Join(",", Numbers));
 if(Transient++==0){var old=Transform;Entity.RemoveComponent<Transform>();Entity.AddComponent<Transform>();if(old.IsValid)throw new Exception("Old wrapper revived");Emit("wrapper-invalid");}
 ++)"+field+R"(;Emit("update:"+)"+field+R"();
 int value=World.Query<Counter>().Single().Value;if(value!=5 && value!=9)throw new Exception("Class query failed");Emit("data:"+value);
}
public override void OnDisable()=>Emit("disable");
public override void OnDestroy()=>Emit("destroy");
})";};
        Write(file,source("Probe","count"));
        {
            CSharpRuntime runtime(root/"assets",root/"cache");
            app.scene.managedUIAction=[&](uint64_t token,const std::string& action){return runtime.RunUIAction(token,action);};
            app.scene.managedTween=[&](int registration,int action){runtime.RunTween(registration,action);};
            Wait(runtime,[&]{return runtime.ReadyToPlay();});
            const auto uiOptions=GetManagedUIActionOptions(*entities[0]);
            Check(uiOptions.size()==1 && uiOptions[0].script=="CSharp:16003035088673311249", "C# button dropdown lost stable script identity");
            Check(uiOptions[0].actions==std::vector<std::string>({"ButtonClick","ThrowingButton"}), "C# button dropdown exposed unsupported methods or lifecycle hooks");
            const auto binding=uiOptions[0].script;
            auto click=[&](const std::string& action="ButtonClick"){return app.DispatchUIAction(*entities[0],binding,action,{});};
            Check(!click(), "C# button invoked outside Play");
            runtime.Tick(true,false,.01f);Check(Read(trace).find("awake:13:Target")!=std::string::npos,"Attachment/serialized references not restored before Awake");
            Check(Read(trace).find("palette:False") != std::string::npos, "Authored color array did not reach gameplay");
            Check(click() && Read(trace).find("button:Probe")!=std::string::npos, "Button did not call attached C# instance");
            for(const auto* invalid:{"Missing","HiddenButton","ReturningButton","GenericButton","StaticButton","AsyncButton","Start","OnDestroy"})
                Check(!click(invalid),"C# button called an unsupported method");
            entities[0]->SetActive(false);Check(!click(),"C# button called inactive owner");entities[0]->SetActive(true);
            entities[0]->GetComponent<ManagedComponents>().items[0]->enabled=false;
            Check(!click(),"C# button called disabled attachment before synchronization");
            entities[0]->GetComponent<ManagedComponents>().items[0]->enabled=true;
            runtime.Tick(true,true,.01f);Check(click(),"Paused menu could not invoke C# button");
            Check(Read(trace).find("wrapper-invalid")!=std::string::npos,"Native component lifetime guard not exercised");
            Check(Read(trace).find("gameplay-apis")!=std::string::npos,"Typed references, parenting, prefab or pooling failed");
            Check(app.scene.EncodeEntity(*entities[0])["Canis::ManagedScripts"][0]["type"].as<std::string>()=="16003035088673311249","Metadata UUID was not used when saving the old attachment");
            AssetManager::GetMetaFile(file.string())->Save();
            Check(YAML::LoadFile(file.string()+".meta")["ScriptAlias"].as<std::string>()=="test.probe","Saving metadata dropped the legacy script alias");
            Check(app.scene.tweens.ActiveCount()==1,"Managed value tween was not registered");
            app.scene.tweens.Update(.1,.1);
            Check(Read(trace).find("tween:Probe")!=std::string::npos,"Native scheduler did not deliver managed tween samples/events");
            auto& attachments=entities[0]->GetComponent<ManagedComponents>();
            attachments.items[0]->fields["Numbers"]=ManagedJson::array({5,13});
            runtime.Tick(true,false,.01f);
            Check(Read(trace).find("numbers:5,13")!=std::string::npos,"Inspector array edits were not applied during Play");
            attachments.items[0]->fields["Numbers"]=ManagedJson::array({3,8});
            attachments.items[1]->fields["Value"]=9;runtime.Tick(true,false,.01f);
            Check(Read(trace).find("data:9")!=std::string::npos,"Runtime-added data component ignored Inspector edit");
            attachments.items[0]->enabled=false;
            runtime.Tick(true,false,.01f);Check(Read(trace).find("disable")!=std::string::npos,"OnDisable missing");
            auto paused=Read(trace);runtime.Tick(true,true,.01f);Check(Read(trace)==paused,"Paused component updated");
            attachments.items[0]->enabled=true;attachments.items[0]->fields["count"]=21;runtime.Tick(true,false,.01f);
            Check(Read(trace).find("update:22")!=std::string::npos,"Inspector edits were not applied during Play");
            runtime.SetLiveReload(true);Write(file,source("RenamedProbe","renamedCount"));runtime.RefreshSources();
            Wait(runtime,[&]{return runtime.Status().find("stateful reload applied")!=std::string::npos;},true);
            Check(app.scene.tweens.ActiveCount()==1,"Reload retained old-generation tween delegates");
            app.scene.tweens.Update(.1,.1);
            Check(Read(trace).find("tween:RenamedProbe")!=std::string::npos,"Reloaded tween did not execute");
            Check(Read(trace).find("arrays:True") != std::string::npos, "Typed arrays did not survive live reload");
            Check(Read(trace).find("palette:True") != std::string::npos, "Color array did not survive live reload");
            Check(click() && Read(trace).find("button:RenamedProbe")!=std::string::npos, "C# button did not resolve reloaded script instance");
            Check(runtime.CollectRetiredContexts()==0,"Tween registry kept retired gameplay assembly alive");
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
            auto removable=app.scene.CreateEntity("Self-removing button target");
            DecodeManagedComponents(YAML::Load("Canis::ManagedScripts: [{type: test.remove, enabled: true, fields: {}}]"),*removable);
            runtime.Tick(true,false,.01f);
            const auto removedToken=removable->GetComponent<ManagedComponents>().items[0]->token;
            Check(app.DispatchUIAction(*removable,"CSharp:test.remove","RemoveOwner",{}) && !removable,"C# callback could not destroy its own target");
            Check(!runtime.RunUIAction(removedToken,"RemoveOwner"),"Destroyed C# target remained callable");
            Check(!click("ThrowingButton"),"C# button exception escaped or reported success");
            Check(!click(),"Faulted script continued receiving UI callbacks");
            auto uiErrors=Debug::GetEntries();
            Check(std::any_of(uiErrors.begin(),uiErrors.end(),[&](const auto& entry){return entry.file==file.string() && entry.line>0 && entry.message.find("EXPECTED_UI_CALLBACK_FAILURE")!=std::string::npos;}),"UI callback exception lost source diagnostics");
            runtime.StopSession();
            Check(!click(),"C# button invoked after Stop");
            Check(app.scene.tweens.ActiveCount()==0,"Stop retained managed tweens");
            app.scene.managedTween={};
            app.scene.managedUIAction={};
            // Missing types remain serialized and can be restored later.
            Check(app.scene.EncodeEntity(*entities[0])["Canis::ManagedScripts"].size()==2,"Managed membership not mirrored to native scene");
        }
        YAML::Node duplicateNodes(YAML::NodeType::Sequence);
        for(auto* e:app.scene.GetEntities())if(e && e->IsValid())duplicateNodes.push_back(app.scene.EncodeEntity(*e));
        duplicateNodes = YAML::Load(YAML::Dump(duplicateNodes));
        // Duplication remaps declared entity references through native load fixups.
        auto copied=app.scene.LoadEntityNodes(duplicateNodes,false);
        auto copyData=EncodeAttachments(*copied[0]);
        auto referenced=copyData[0]["fields"]["Target"]["entity"].get<std::string>();
        Check(copyData[0]["fields"]["Targets"][0]["entity"].get<std::string>()==referenced,"Duplicate retained source array entity reference");
        Check(copyData[0]["fields"]["Transforms"][0]["entity"].get<std::string>()==referenced,"Duplicate retained source array component reference");
        Check(copyData[0]["fields"]["Targets"][1]["entity"]=="0","Duplicate changed null array reference");
        Check(referenced==std::to_string(static_cast<uint64_t>(copied[1]->GetUUID())),"Duplicate retained source entity reference");
        UnregisterSceneBindings();app.scene.Unload();fs::remove_all(root);SDL_Quit();std::cout<<"C# component lifecycle, persistence, class queries and reload passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\nFixtures: "<<root<<'\n';return 1;}
}
