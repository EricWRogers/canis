using System.Numerics;
namespace Canis;

/// <summary>Non-owning scene identity. Cached per generation; destruction uses ordinary null semantics.</summary>
public sealed class Entity : IEquatable<Entity>
{
    private static readonly Dictionary<ulong,WeakReference<Entity>> cache=[];
    public ulong Handle { get; }
    public ulong UUID => NativeBridge.Call<ulong>("Scene.UUID",Handle);
    private readonly Dictionary<Type,NativeComponent> components=[];
    private Entity(ulong handle)=>Handle=handle;
    internal static Entity? FromHandle(ulong handle) {
        if(handle==0)return null;
        if(cache.TryGetValue(handle,out var weak) && weak.TryGetTarget(out var entity))return entity;
        entity=new Entity(handle);cache[handle]=new(entity);return entity;
    }
    internal static void ClearCache()=>cache.Clear();
    public static IEnumerable<Entity> All() {
        var snapshot=System.Text.Json.Nodes.JsonNode.Parse(NativeBridge.Call<string>("Managed.Snapshot"))!.AsArray();
        foreach(var item in snapshot)yield return FromHandle(ulong.Parse(item!["handle"]!.GetValue<string>()))!;
    }
    public static Entity? Find(string name)=>FromHandle(NativeBridge.Call<ulong>("Scene.Find",name));
    public static Entity? FromUUID(ulong uuid)=>FromHandle(NativeBridge.Call<ulong>("Scene.FromUUID",uuid));
    public static Entity Create(string name="Entity")=>FromHandle(NativeBridge.Call<ulong>("Scene.Create",name))!;
    public void Destroy()=>NativeBridge.Call("Scene.Destroy",Handle);
    public bool IsValid=>NativeBridge.Call<bool>("Scene.IsValid",Handle);
    public string Name {get=>NativeBridge.Call<string>("Scene.Name",Handle);set=>NativeBridge.Call("Scene.SetName",Handle,value);}
    public bool Active {get=>NativeBridge.Call<bool>("Scene.Active",Handle);set=>NativeBridge.Call("Scene.SetActive",Handle,value);}
    /// <summary>All native component names registered by this engine build.</summary>
    public static string[] NativeComponentTypes => System.Text.Json.JsonSerializer.Deserialize<string[]>(NativeBridge.Call<string>("Component.Types"))!;
    public NativeComponent? GetNativeComponent(string name)
    {
        ulong token = NativeBridge.Call<ulong>("Component.Token", Handle, name);
        return token == 0 ? null : new RegisteredNativeComponent(name) { Owner = this, Attachment = token };
    }
    public NativeComponent AddNativeComponent(string name)
    {
        NativeBridge.Call<ulong>("Component.Add", Handle, name);
        return GetNativeComponent(name)!;
    }
    public bool RemoveNativeComponent(string name)
    {
        if (GetNativeComponent(name) is null) return false;
        NativeBridge.Call("Component.Remove", Handle, name);
        return true;
    }
    public Transform Transform=>GetComponent<Transform>()??throw new MissingObjectException("Entity has no Transform.");
    internal Component? GetComponent(Type type) {
        if(!typeof(NativeComponent).IsAssignableFrom(type))return ComponentStore.Get(this,type);
        if(components.TryGetValue(type,out var cached) && cached.IsValid)return cached;
        var token=NativeBridge.Call<ulong>("Component.Token",Handle,type.Name);
        if(token==0)return null;
        var component=(NativeComponent)Activator.CreateInstance(type)!;
        component.Owner=this;component.Attachment=token;components[type]=component;return component;
    }
    public T? GetComponent<T>() where T:Component {
        if(!IsValid)throw new MissingObjectException("Entity no longer exists.");
        if(!typeof(NativeComponent).IsAssignableFrom(typeof(T)))return ComponentStore.Get<T>(this);
        if(components.TryGetValue(typeof(T),out var component) && component.IsValid)return (T)(Component)component;
        ulong token=NativeBridge.Call<ulong>("Component.Token",Handle,typeof(T).Name);
        if(token==0)return null;
        component=(NativeComponent)Activator.CreateInstance(typeof(T))!;component.Owner=this;component.Attachment=token;components[typeof(T)]=component;return (T)(Component)component;
    }
    public bool TryGetComponent<T>(out T? component) where T:Component {component=GetComponent<T>();return component is not null;}
    public T[] GetComponents<T>() where T:Component=>typeof(NativeComponent).IsAssignableFrom(typeof(T))?(GetComponent<T>() is {} c?[c]:[]):ComponentStore.GetAll<T>(this);
    public T AddComponent<T>() where T:Component,new() {
        if(GetComponent<T>() is not null)throw new InvalidOperationException($"{typeof(T).Name} already attached.");
        if(typeof(NativeComponent).IsAssignableFrom(typeof(T))){NativeBridge.Call<ulong>("Component.Add",Handle,typeof(T).Name);return GetComponent<T>()!;}
        return ComponentStore.Add<T>(this);
    }
    public bool RemoveComponent<T>() where T:Component {
        if(!typeof(NativeComponent).IsAssignableFrom(typeof(T)))return ComponentStore.Remove<T>(this);
        var c=GetComponent<T>();if(c is null)return false;NativeBridge.Call("Component.Remove",Handle,typeof(T).Name);c.Detached=true;components.Remove(typeof(T));return true;
    }
    public void SetColor(Vector4 color)=>(GetComponent<Model>()??throw new MissingObjectException("Missing Model")).Color=color;
    public void SetText(string text)=>(GetComponent<Text>()??throw new MissingObjectException("Missing Text")).Value=text;
    public bool Equals(Entity? other)=>other?.Handle==Handle;
    public override bool Equals(object? obj)=>obj is Entity e && Equals(e);
    public override int GetHashCode()=>Handle.GetHashCode();
    public static bool operator==(Entity? a,Entity? b)=>ReferenceEquals(a,b) || a is not null && a.Equals(b);
    public static bool operator!=(Entity? a,Entity? b)=>!(a==b);
}
public abstract class NativeComponent : Component
{
    internal virtual string NativeName => GetType().Name;
    public override bool IsValid => !Detached && Owner is not null && Owner.IsValid && NativeBridge.Call<ulong>("Component.Token", Owner.Handle, NativeName) == Attachment;

    /// <summary>Snapshot of all authored fields, using native scene field names and formats.</summary>
    public System.Text.Json.Nodes.JsonObject ReadFields()
    {
        Validate();
        return System.Text.Json.Nodes.JsonNode.Parse(NativeBridge.Call<string>("Component.Read", Owner!.Handle, NativeName))!.AsObject();
    }

    /// <summary>Apply authored configuration through the native scene codec; unrelated fields are retained.
    /// This is configuration editing, not a physics/animation command API.</summary>
    public void ApplyFields(System.Text.Json.Nodes.JsonObject fields)
    {
        Validate();
        NativeBridge.Call("Component.Write", Owner!.Handle, NativeName, fields.ToJsonString());
    }
    public T GetField<T>(string name)
    {
        var field = ReadFields()[name] ?? throw new KeyNotFoundException($"Serialized field '{name}' is absent on {NativeName}.");
        if (typeof(T) == typeof(string)) return (T)(object)field.ToString();
        string json = field.ToJsonString();
        if (typeof(T) == typeof(Vector2) || typeof(T) == typeof(Vector3) || typeof(T) == typeof(Vector4))
        {
            var v = System.Text.Json.JsonSerializer.Deserialize<float[]>(json)!;
            object value = typeof(T) == typeof(Vector2) ? (object)new Vector2(v[0], v[1]) :
                typeof(T) == typeof(Vector3) ? (object)new Vector3(v[0], v[1], v[2]) : new Vector4(v[0], v[1], v[2], v[3]);
            return (T)value;
        }
        return System.Text.Json.JsonSerializer.Deserialize<T>(json)!;
    }
    public void SetField<T>(string name, T value)
    {
        object? serialized = value switch {
            Vector2 v => new[] { v.X, v.Y },
            Vector3 v => new[] { v.X, v.Y, v.Z },
            Vector4 v => new[] { v.X, v.Y, v.Z, v.W },
            _ => value
        };
        ApplyFields(new() { [name] = System.Text.Json.JsonSerializer.SerializeToNode(serialized) });
    }
}
internal sealed class RegisteredNativeComponent(string name) : NativeComponent
{
    internal override string NativeName => name;
}
public sealed class Transform : NativeComponent
{
    public Entity? Parent { get {Validate();return Entity.FromHandle(NativeBridge.Call<ulong>("Transform.Parent",Owner!.Handle));} }
    public void SetParent(Entity? parent, bool worldPositionStays=true) {
        Validate();NativeBridge.Call("Transform.SetParent",Owner!.Handle,parent?.Handle??0,worldPositionStays);
    }
    public Vector3 Position {get {Validate();return NativeBridge.Call<Vector3>("Transform.Position",Owner!.Handle);}set {Validate();NativeBridge.Call("Transform.SetPosition",Owner!.Handle,value);}}
    public Quaternion Rotation {get {Validate();return NativeBridge.Call<Quaternion>("Transform.Rotation",Owner!.Handle);}set {Validate();NativeBridge.Call("Transform.SetRotation",Owner!.Handle,value);}}
    public Vector3 LocalScale {get {Validate();return NativeBridge.Call<Vector3>("Transform.Scale",Owner!.Handle);}set {Validate();NativeBridge.Call("Transform.SetScale",Owner!.Handle,value);}}
    public void Translate(Vector3 delta)=>Position+=delta;
}
public sealed class Model : NativeComponent
{
    public ModelAsset? Asset { set => ApplyFields(new() { ["ModelAsset"] = new System.Text.Json.Nodes.JsonObject { ["uuid"] = value?.UUID??0 } }); }
    public Vector4 Color {get {Validate();return NativeBridge.Call<Vector4>("Presentation.Color",Owner!.Handle);}set {Validate();NativeBridge.Call("Presentation.SetColor",Owner!.Handle,value);}}
}
public sealed class Text : NativeComponent
{
    public string Value {get {Validate();return NativeBridge.Call<string>("Presentation.Text",Owner!.Handle);}set {Validate();NativeBridge.Call("Presentation.SetText",Owner!.Handle,value);}}
}
