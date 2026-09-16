namespace Canis;

public abstract record Asset(ulong UUID)
{
    public string Path => UUID == 0 ? "" : NativeBridge.Call<string>("Assets.Path", UUID);
    public bool IsValid => UUID != 0 && NativeBridge.Call<string>("Assets.Kind", UUID) == Kind(GetType());
    public static implicit operator AssetReference(Asset? value) => new(value?.UUID ?? 0);
    internal static string Kind(Type type) => type == typeof(AudioClip) ? "audio" :
        type == typeof(SceneAsset) || type == typeof(PrefabAsset) ? "scene" :
        type == typeof(ModelAsset) ? "model" : type == typeof(MaterialAsset) ? "material" :
        type == typeof(TextureAsset) ? "texture" : throw new ArgumentException("Unsupported asset type", nameof(type));
}
public sealed record AudioClip(ulong UUID) : Asset(UUID);
public sealed record SceneAsset(ulong UUID) : Asset(UUID);
public sealed record PrefabAsset(ulong UUID) : Asset(UUID);
public sealed record ModelAsset(ulong UUID) : Asset(UUID);
public sealed record MaterialAsset(ulong UUID) : Asset(UUID);
public sealed record TextureAsset(ulong UUID) : Asset(UUID);

public static class Assets
{
    public static T Load<T>(string path) where T : Asset {
        ArgumentException.ThrowIfNullOrWhiteSpace(path);
        var id=NativeBridge.Call<ulong>("Assets.UUID",path);
        var asset=(T)Activator.CreateInstance(typeof(T),id)!;
        if(!asset.IsValid)throw new ArgumentException($"Asset '{path}' is not a {typeof(T).Name}",nameof(path));
        return asset;
    }
}

[AttributeUsage(AttributeTargets.Field)]
public sealed class TooltipAttribute(string text) : Attribute { public string Text { get; } = text; }
[AttributeUsage(AttributeTargets.Field)]
public sealed class HeaderAttribute(string text) : Attribute { public string Text { get; } = text; }
