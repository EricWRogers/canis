using System.Text.Json;
namespace Canis;

public static class Prefabs
{
    public static Entity[] Instantiate(PrefabAsset prefab, Entity? parent=null) {
        ArgumentNullException.ThrowIfNull(prefab);
        var handles=JsonSerializer.Deserialize<ulong[]>(NativeBridge.Call<string>("Scene.Instantiate",prefab.UUID))!;
        var entities=handles.Select(h=>Entity.FromHandle(h)!).ToArray();
        ComponentStore.SynchronizeSpawned();
        foreach(var entity in entities)if(entity.GetComponent<Transform>() is {} t && t.Parent is null && parent is not null)t.SetParent(parent,false);
        return entities;
    }
}

/// <summary>Owns its entities. Call Release once per rental and Dispose at owner teardown.</summary>
public sealed class EntityPool(Func<Entity> create, int capacity=16, Action<Entity>? onRent=null, Action<Entity>? onRelease=null) : IDisposable
{
    private readonly HashSet<Entity> owned=[];
    private readonly HashSet<Entity> rented=[];
    private readonly Stack<Entity> available=[];
    private bool disposed;
    public Entity Rent() {
        ObjectDisposedException.ThrowIf(disposed,this);
        Entity? entity=null;
        while(available.Count>0 && entity is null){var next=available.Pop();if(next.IsValid)entity=next;else owned.Remove(next);}
        if(entity is null){entity=create();if(entity is null || !entity.IsValid)throw new InvalidOperationException("Pool factory returned an invalid entity");if(!owned.Add(entity))throw new InvalidOperationException("Pool factory returned an entity already owned by this pool");}
        rented.Add(entity);entity.Active=true;
        try {onRent?.Invoke(entity);return entity;}catch{Release(entity);throw;}
    }
    public void Release(Entity entity) {
        ObjectDisposedException.ThrowIf(disposed,this);
        if(!rented.Remove(entity))throw new InvalidOperationException("Entity is not rented from this pool");
        if(!entity.IsValid){owned.Remove(entity);return;}
        try {onRelease?.Invoke(entity);}finally {
            if(entity.IsValid)entity.Active=false;
            if(entity.IsValid && available.Count<Math.Max(0,capacity))available.Push(entity);
            else {if(entity.IsValid)entity.Destroy();owned.Remove(entity);}
        }
    }
    public void Dispose(){if(disposed)return;disposed=true;foreach(var e in owned)if(e.IsValid)e.Destroy();owned.Clear();rented.Clear();available.Clear();}
}
