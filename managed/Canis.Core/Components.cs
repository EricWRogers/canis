using System.Numerics;
namespace Canis;

[AttributeUsage(AttributeTargets.Class, Inherited=false)]
public sealed class ScriptIdAttribute(string id) : Attribute { public string Id { get; }=id; }
[AttributeUsage(AttributeTargets.Class, AllowMultiple=true, Inherited=false)]
public sealed class ScriptAliasAttribute(string id) : Attribute { public string Id { get; }=id; }
[AttributeUsage(AttributeTargets.Field)] public sealed class SerializeFieldAttribute : Attribute { }
[AttributeUsage(AttributeTargets.Field, AllowMultiple=true)]
public sealed class FormerlySerializedAsAttribute(string name) : Attribute { public string Name { get; }=name; }
public readonly record struct AssetReference(ulong UUID)
{
    public string Path => UUID==0 ? "" : NativeBridge.Call<string>("Assets.Path",UUID);
}
public class MissingObjectException(string message) : InvalidOperationException(message);

public abstract class Component
{
    internal Entity? Owner;
    internal ulong Attachment;
    internal bool Detached;
    public bool RestoredFromReload {get;internal set;}
    public Entity Entity { get { Validate();return Owner!; } }
    public Transform Transform => Entity.Transform;
    public virtual bool IsValid => !Detached && Owner is not null && Owner.IsValid && NativeBridge.Call<bool>("Managed.Valid",Owner.Handle,Attachment);
    internal void Validate() { if(!IsValid)throw new MissingObjectException($"{GetType().Name} is detached or its entity no longer exists."); }
    public T? GetComponent<T>() where T:Component => Entity.GetComponent<T>();
    public T AddComponent<T>() where T:Component,new() => Entity.AddComponent<T>();
    public bool TryGetComponent<T>(out T? component) where T:Component {component=GetComponent<T>();return component is not null;}
}
public abstract class Behaviour : Component
{
    internal bool enabled=true;
    public bool Enabled {get {Validate();return enabled;} set {Validate();enabled=value;NativeBridge.Call("Managed.Enabled",Owner!.Handle,Attachment,value);} }
    public bool IsActiveAndEnabled => IsValid && enabled && Owner!.Active;
}
public abstract class ScriptableEntity : Behaviour
{
    public virtual void Awake() { }
    public virtual void OnCreate() { }
    public virtual void OnEnable() { }
    public virtual void Start() { }
    public virtual void Update(float deltaTime) { }
    public virtual void OnDisable() { }
    public virtual void OnDestroy() { }
}
public static class Time { public static float DeltaTime {get;internal set;} public static double Elapsed {get;internal set;} }
public static class World
{
    public static IEnumerable<T> Query<T>() where T:Component => ComponentStore.Query<T>();
    public static IEnumerable<(TFirst,TSecond)> Query<TFirst,TSecond>() where TFirst:Component where TSecond:Component
    { foreach(var first in Query<TFirst>())if(first.Entity.GetComponent<TSecond>() is {} second)yield return(first,second); }
}
