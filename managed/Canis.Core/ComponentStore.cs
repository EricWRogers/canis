using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Numerics;
using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;
namespace Canis;

// Stores belong to one active gameplay generation. Clear before unloading its ALC.
internal static class ComponentStore
{
    internal sealed record Description(Type Type,string Id,string[] Aliases,FieldInfo[] Fields);
    private sealed class State(Component component) {internal Component Component=component;internal bool Awoken,Active,Started,Failed,Restored;internal JsonObject Authored=[];}
    private static Dictionary<string,Description> types=[];
    private static readonly Dictionary<(ulong,ulong),State> instances=[];
    private static readonly HashSet<string> reportedMissing=[];
    internal static bool Running;
    internal static string Id(Type t)=>t.GetCustomAttribute<ScriptIdAttribute>()?.Id ?? t.FullName!;
    private static IEnumerable<string> ReferenceTypes(Type type) {
        for(Type? t=type;t is not null && typeof(Component).IsAssignableFrom(t);t=t.BaseType)yield return Id(t);
    }
    private static IEnumerable<FieldInfo> Fields(Type type)
    {
        for(Type? t=type;t is not null && t!=typeof(Component) && t!=typeof(Behaviour) && t!=typeof(ScriptableEntity);t=t.BaseType)
            foreach(var f in t.GetFields(BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Instance|BindingFlags.DeclaredOnly))
                if(!f.IsStatic && !f.IsInitOnly && f.GetCustomAttribute<NonSerializedAttribute>() is null && (f.IsPublic || f.IsDefined(typeof(SerializeFieldAttribute))))yield return f;
    }
    internal static Description[] Describe(IEnumerable<Type> all)
    {
        var result=all.Where(t=>t.IsSubclassOf(typeof(Component)) && !typeof(NativeComponent).IsAssignableFrom(t) && !t.IsAbstract).OrderBy(t=>t.FullName,StringComparer.Ordinal)
            .Select(t=>new Description(t,Id(t),t.GetCustomAttributes<ScriptAliasAttribute>().Select(a=>a.Id).ToArray(),Fields(t).ToArray())).ToArray();
        var ids=new HashSet<string>();
        foreach(var d in result) {
            if(!d.Type.IsPublic || d.Type.ContainsGenericParameters || d.Type.GetConstructor(Type.EmptyTypes) is null)throw new InvalidOperationException($"{d.Type} needs a public parameterless constructor.");
            foreach(var id in d.Aliases.Prepend(d.Id))if(string.IsNullOrWhiteSpace(id) || !ids.Add(id))throw new InvalidOperationException($"Duplicate or empty ScriptId/ScriptAlias: {id}");
            var fields=new HashSet<string>();foreach(var f in d.Fields){Kind(f.FieldType);if(!fields.Add(f.Name))throw new InvalidOperationException($"Hidden serialized field: {d.Type}.{f.Name}");}
        }
        return result;
    }
    internal static string Kind(Type t) => t==typeof(bool)?"bool":t==typeof(int)?"int":t==typeof(float)?"float":t==typeof(double)?"double":t==typeof(string)?"string":t.IsEnum?"enum":t==typeof(Vector3)?"vector3":t==typeof(Vector4)?"vector4":t==typeof(Quaternion)?"quaternion":t==typeof(Entity)?"entity":typeof(Component).IsAssignableFrom(t)?"component":t==typeof(AssetReference) || typeof(Asset).IsAssignableFrom(t)?"asset":throw new InvalidOperationException($"Unsupported serialized field type {t}. Mark runtime-only fields [NonSerialized].");
    internal static JsonNode? Encode(object? value,Type t)
    {
        if(t==typeof(Entity))return new JsonObject{{"entity",(value is Entity e && e.IsValid?e.UUID:0).ToString()}};
        if(typeof(Component).IsAssignableFrom(t))return new JsonObject{{"entity",(value is Component c && c.IsValid?c.Entity.UUID:0).ToString()}};
        if(typeof(Asset).IsAssignableFrom(t))return JsonValue.Create((value is Asset asset?asset.UUID:0).ToString());
        if(t==typeof(AssetReference))return JsonValue.Create(((AssetReference?)value)?.UUID.ToString()??"0");
        if(value is Vector3 v)return new JsonArray(v.X,v.Y,v.Z);
        if(value is Vector4 v4)return new JsonArray(v4.X,v4.Y,v4.Z,v4.W);
        if(value is Quaternion q)return new JsonArray(q.X,q.Y,q.Z,q.W);
        if(t.IsEnum)return JsonValue.Create(value?.ToString()??Enum.GetNames(t).FirstOrDefault()??"0");
        return JsonSerializer.SerializeToNode(value,t);
    }
    private static object? Decode(JsonNode? value,Type t, bool resolveReferences = true)
    {
        if(typeof(Component).IsAssignableFrom(t)) {
            var id=ulong.Parse(value?["entity"]?.ToString()??"0");
            return resolveReferences ? Entity.FromUUID(id)?.GetComponent(t) : null;
        }
        if(typeof(Asset).IsAssignableFrom(t)) {
            var id=ulong.Parse(value?.ToString()??"0");
            if(id==0)return null;
            var kind=NativeBridge.Call<string>("Assets.Kind",id);
            if(kind!="" && kind!=Asset.Kind(t))throw new InvalidOperationException($"Asset {id} is {kind}, expected {Asset.Kind(t)}");
            return Activator.CreateInstance(t,id);
        }
        if(t==typeof(Entity))return Entity.FromUUID(ulong.Parse(value?["entity"]?.GetValue<string>()??"0"));
        if(t==typeof(AssetReference))return new AssetReference(ulong.Parse(value?.GetValue<string>()??"0"));
        if(t==typeof(Vector3))return new Vector3(value![0]!.GetValue<float>(),value[1]!.GetValue<float>(),value[2]!.GetValue<float>());
        if(t==typeof(Vector4))return new Vector4(value![0]!.GetValue<float>(),value[1]!.GetValue<float>(),value[2]!.GetValue<float>(),value[3]!.GetValue<float>());
        if(t==typeof(Quaternion))return new Quaternion(value![0]!.GetValue<float>(),value[1]!.GetValue<float>(),value[2]!.GetValue<float>(),value[3]!.GetValue<float>());
        if(t.IsEnum)return Enum.Parse(t,value!.GetValue<string>());
        return value is null ? (t.IsValueType?Activator.CreateInstance(t):null) : value.Deserialize(t);
    }
    private static string Source(Type type)
    {
        var file=Path.ChangeExtension(type.Assembly.Location,".pdb");if(!File.Exists(file))return "";
        using var stream=File.OpenRead(file);using var provider=MetadataReaderProvider.FromPortablePdbStream(stream);var reader=provider.GetMetadataReader();
        foreach(var method in type.GetMethods(BindingFlags.Public|BindingFlags.NonPublic|BindingFlags.Instance|BindingFlags.DeclaredOnly).Cast<MethodBase>().Concat(type.GetConstructors())) {
            var info=reader.GetMethodDebugInformation(MetadataTokens.MethodDefinitionHandle(method.MetadataToken&0xffffff));
            foreach(var point in info.GetSequencePoints())if(!point.IsHidden){var doc=point.Document.IsNil?info.Document:point.Document;if(!doc.IsNil)return reader.GetString(reader.GetDocument(doc).Name);}
        }
        return "";
    }
    internal static string Manifest(Description[] descriptions)
    {
        var list=new JsonArray();
        foreach(var d in descriptions) {
            object defaults;
            NativeBridge.MetadataMode=true;try {defaults=Activator.CreateInstance(d.Type)!;}finally{NativeBridge.MetadataMode=false;}
            var fields=new JsonArray();
            foreach(var f in d.Fields)fields.Add(new JsonObject{{"name",f.Name},{"aliases",new JsonArray(f.GetCustomAttributes<FormerlySerializedAsAttribute>().Select(a=>(JsonNode?)JsonValue.Create(a.Name)).ToArray())},{"kind",Kind(f.FieldType)},{"assetType",typeof(Asset).IsAssignableFrom(f.FieldType)?Asset.Kind(f.FieldType):""},{"componentType",typeof(Component).IsAssignableFrom(f.FieldType)?(typeof(NativeComponent).IsAssignableFrom(f.FieldType)?"Canis::"+f.FieldType.Name:Id(f.FieldType)):""},{"header",f.GetCustomAttribute<HeaderAttribute>()?.Text??""},{"tooltip",f.GetCustomAttribute<TooltipAttribute>()?.Text??""},{"default",Encode(f.GetValue(defaults),f.FieldType)},{"options",new JsonArray((f.FieldType.IsEnum?Enum.GetNames(f.FieldType):[]).Select(n=>(JsonNode?)JsonValue.Create(n)).ToArray())}});
            list.Add(new JsonObject{{"id",d.Id},{"name",d.Type.FullName},{"source",Source(d.Type)},{"assignableTo",new JsonArray(ReferenceTypes(d.Type).Select(a=>(JsonNode?)JsonValue.Create(a)).ToArray())},{"aliases",new JsonArray(d.Aliases.Select(a=>(JsonNode?)JsonValue.Create(a)).ToArray())},{"fields",fields}});
        }
        return list.ToJsonString();
    }
    internal static void Configure(Description[] descriptions,string manifest)
    {
        Clear();types=[];foreach(var d in descriptions)foreach(var id in d.Aliases.Prepend(d.Id))types.Add(id,d);
        try {NativeBridge.Call("Managed.Publish",manifest);}
        catch(InvalidOperationException error) when(descriptions.Length==0 && error.Message.Contains("Native binding unavailable: Managed.Publish")) { }
    }
    internal static void Start(){Running=true;Time.Elapsed=0;Synchronize();}
    internal static void SynchronizeSpawned(){if(Running)Synchronize();}
    private static void Restore(Component c,Description d,JsonObject fields)
    {
        foreach(var f in d.Fields) {
            string? key=fields.ContainsKey(f.Name)?f.Name:f.GetCustomAttributes<FormerlySerializedAsAttribute>().Select(a=>a.Name).FirstOrDefault(fields.ContainsKey);
            if(key is not null)f.SetValue(c,Decode(fields[key],f.FieldType));
        }
    }
    internal static void Synchronize(JsonArray? overrides=null)
    {
        if(types.Count==0)return;
        var snapshot=overrides??JsonNode.Parse(NativeBridge.Call<string>("Managed.ScriptSnapshot"))!.AsArray();
        var seen=new HashSet<(ulong,ulong)>();var added=new List<(State,Description,JsonObject)>();
        foreach(var node in snapshot) {
            ulong entity=ulong.Parse(node!["handle"]!.GetValue<string>());
            foreach(var script in node["scripts"]!.AsArray()) {
                var id=script!["type"]!.GetValue<string>();if(!types.TryGetValue(id,out var d)){if(reportedMissing.Add(id))Log.Info($"C# missing script '{id}'; saved data retained.");continue;}
                var token=ulong.Parse(script["token"]!.GetValue<string>());var key=(entity,token);seen.Add(key);
                if(!instances.TryGetValue(key,out var state)) {
                    try {var c=(Component)Activator.CreateInstance(d.Type)!;c.Owner=Entity.FromHandle(entity);c.Attachment=token;state=new State(c);instances.Add(key,state);added.Add((state,d,script["fields"]!.AsObject()));}
                    catch(Exception error){Log.Info($"C# {d.Id} constructor: {error}");continue;}
                }
                var authored=script["fields"]!.AsObject();
                if(state.Restored) {
                    var changed=new JsonObject();foreach(var field in authored)if(!state.Authored.TryGetPropertyValue(field.Key,out var before) || !JsonNode.DeepEquals(before,field.Value))changed[field.Key]=field.Value?.DeepClone();
                    if(changed.Count>0)Try(state,()=>Restore(state.Component,d,changed));
                }
                state.Authored=(JsonObject)authored.DeepClone();
                if(state.Component is Behaviour b)b.enabled=script["enabled"]!.GetValue<bool>();
            }
        }
        foreach(var entry in instances.ToArray())if(!seen.Contains(entry.Key) || entry.Value.Component.Detached){Detach(entry.Value);instances.Remove(entry.Key);}
        foreach(var (s,d,fields) in added){Try(s,()=>Restore(s.Component,d,fields));s.Restored=true;}
    }
    internal static void Update(float dt)
    {
        Synchronize();Time.DeltaTime=dt;Time.Elapsed+=dt;
        foreach(var s in instances.Values.ToArray()) {
            if(s.Component is not ScriptableEntity b || !b.IsValid)continue;
            if(!s.Awoken){s.Awoken=true;Try(s,b.Awake);if(b.IsValid)Try(s,b.OnCreate);}
            bool active=b.IsActiveAndEnabled && !s.Failed;
            if(active!=s.Active){s.Active=active;Try(s,active?b.OnEnable:b.OnDisable);}
            if(!s.Active || s.Failed || !b.IsValid)continue;
            if(!s.Started){s.Started=true;Try(s,b.Start);}
            if(b.IsActiveAndEnabled && !s.Failed)Try(s,()=>b.Update(dt));
        }
    }
    private static void Try(State s,Action action){try{action();}catch(Exception error){
        s.Failed=true;
        string owner=s.Component.Owner is { IsValid:true } e ? $"{e.Name} (UUID {e.UUID})" : "destroyed entity";
        Log.ReportException(error,$"{s.Component.GetType().FullName} on {owner}");
    }}
    private static void Detach(State s){if(s.Component is ScriptableEntity b){if(s.Active)Try(s,b.OnDisable);Try(s,b.OnDestroy);}s.Component.Detached=true;}
    internal static void Stop(){foreach(var s in instances.Values.ToArray())Detach(s);instances.Clear();Running=false;Entity.ClearCache();}
    internal static void Clear(){Stop();types.Clear();reportedMissing.Clear();}
    internal static T? Get<T>(Entity owner) where T:Component
    {
        var matches=instances.Values.Select(s=>s.Component).OfType<T>().Where(c=>c.Owner==owner && c.IsValid).ToArray();
        if(matches.Length>1)throw new InvalidOperationException($"Ambiguous {typeof(T).Name}; use GetComponents<T>().");return matches.SingleOrDefault();
    }
    internal static T[] GetAll<T>(Entity owner) where T:Component=>instances.Values.Select(s=>s.Component).OfType<T>().Where(c=>c.Owner==owner && c.IsValid).ToArray();
    internal static Component? Get(Entity owner,Type type) => instances.Values.Select(s=>s.Component)
        .Where(c=>c.Owner==owner && c.IsValid && type.IsInstanceOfType(c)).SingleOrDefault();
    internal static T Add<T>(Entity owner) where T:Component,new()
    {
        if(!Running)throw new InvalidOperationException("Add components during gameplay, after attachment.");
        var id=Id(typeof(T));if(!types.ContainsKey(id))throw new InvalidOperationException($"Unregistered component {id}");
        var c=new T{Owner=owner,Attachment=NativeBridge.Call<ulong>("Managed.Add",owner.Handle,id)};
        instances.Add((owner.Handle,c.Attachment),new State(c){Restored=true});return c;
    }
    internal static bool Remove<T>(Entity owner) where T:Component
    {var c=Get<T>(owner);if(c is null)return false;NativeBridge.Call("Managed.Remove",owner.Handle,c.Attachment);c.Detached=true;return true;}
    internal static T[] Query<T>() where T:Component=>typeof(NativeComponent).IsAssignableFrom(typeof(T))?Entity.All().Select(e=>e.GetComponent<T>()).OfType<T>().ToArray():instances.Values.Select(s=>s.Component).OfType<T>().Where(c=>c.IsValid).ToArray();
    internal static JsonArray Capture()
    {
        var snapshot=JsonNode.Parse(NativeBridge.Call<string>("Managed.ScriptSnapshot"))!.AsArray();
        foreach(var e in snapshot)foreach(var a in e!["scripts"]!.AsArray()) {
            var key=(ulong.Parse(e["handle"]!.GetValue<string>()),ulong.Parse(a!["token"]!.GetValue<string>()));
            if(instances.TryGetValue(key,out var state) && types.TryGetValue(a["type"]!.GetValue<string>(),out var d)) {
                var fields=a["fields"]!.AsObject();foreach(var f in d.Fields)fields[f.Name]=Encode(f.GetValue(state.Component),f.FieldType);
            }
        }
        return snapshot;
    }
    internal static void ValidateSnapshot(Description[] candidate,JsonArray snapshot)
    {
        foreach(var e in snapshot)foreach(var a in e!["scripts"]!.AsArray()) {
            var id=a!["type"]!.GetValue<string>();var d=candidate.FirstOrDefault(d=>d.Id==id || d.Aliases.Contains(id));
            if(d is null)throw new InvalidOperationException($"Reload would remove attached script {id}; stop Play to change attachments.");
            var fields=a["fields"]!.AsObject();foreach(var f in d.Fields){var key=fields.ContainsKey(f.Name)?f.Name:f.GetCustomAttributes<FormerlySerializedAsAttribute>().Select(a=>a.Name).FirstOrDefault(fields.ContainsKey);if(key is not null)_ = Decode(fields[key],f.FieldType,false);}
        }
    }
    internal static void StartRestored(JsonArray snapshot){
        Running=true;Synchronize(snapshot);
        foreach(var state in instances.Values)state.Component.RestoredFromReload=true;
        var authored=JsonNode.Parse(NativeBridge.Call<string>("Managed.ScriptSnapshot"))!.AsArray();
        foreach(var e in authored)foreach(var a in e!["scripts"]!.AsArray())if(instances.TryGetValue((ulong.Parse(e["handle"]!.GetValue<string>()),ulong.Parse(a!["token"]!.GetValue<string>())),out var state))state.Authored=(JsonObject)a["fields"]!.DeepClone();
    }
}
