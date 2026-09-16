namespace Canis;

public enum AudioRolloffMode { Logarithmic, Linear, None }

/// <summary>The active listening point. With multiple enabled listeners, the lowest entity UUID wins.</summary>
public sealed class AudioListener : NativeComponent
{
    public bool Enabled { get => GetField<bool>("enabled"); set => SetField("enabled", value); }
    public float Volume { get => GetField<float>("volume"); set => SetField("volume", value); }
    public bool FollowHeadset { get => GetField<bool>("followHeadset"); set => SetField("followHeadset", value); }
}

/// <summary>A native, entity-owned sound emitter. Removing it stops all of its voices.</summary>
public sealed class AudioSource : NativeComponent
{
    public bool Enabled { get => GetField<bool>("enabled"); set => SetField("enabled", value); }
    public bool PlayOnAwake { get => GetField<bool>("playOnAwake"); set => SetField("playOnAwake", value); }
    public bool Loop { get => GetField<bool>("loop"); set => SetField("loop", value); }
    public bool Mute { get => GetField<bool>("mute"); set => SetField("mute", value); }
    public float Volume { get => GetField<float>("volume"); set => SetField("volume", value); }
    public float Pitch { get => GetField<float>("pitch"); set => SetField("pitch", value); }
    public float SpatialBlend { get => GetField<float>("spatialBlend"); set => SetField("spatialBlend", value); }
    public float MinDistance { get => GetField<float>("minDistance"); set => SetField("minDistance", value); }
    public float MaxDistance { get => GetField<float>("maxDistance"); set => SetField("maxDistance", value); }
    public AudioRolloffMode RolloffMode { get => (AudioRolloffMode)GetField<int>("rolloffMode"); set => SetField("rolloffMode", (int)value); }
    public AssetReference Clip
    {
        get { Validate(); return new(NativeBridge.Call<ulong>("AudioSource.ClipUUID", Owner!.Handle)); }
        set { Validate(); NativeBridge.Call("AudioSource.SetClipUUID", Owner!.Handle, value.UUID); }
    }
    public string ClipPath
    {
        get { Validate(); return NativeBridge.Call<string>("AudioSource.ClipPath", Owner!.Handle); }
        set { Validate(); NativeBridge.Call("AudioSource.SetClipPath", Owner!.Handle, value); }
    }
    public bool IsPlaying { get { Validate(); return NativeBridge.Call<bool>("AudioSource.IsPlaying", Owner!.Handle); } }
    public bool Play() { Validate(); return NativeBridge.Call<bool>("AudioSource.Play", Owner!.Handle); }
    public bool PlayOneShot(AssetReference clip, float volumeScale = 1) => PlayOneShot(clip.Path, volumeScale);
    public bool PlayOneShot(string path, float volumeScale = 1)
    {
        Validate(); return NativeBridge.Call<bool>("AudioSource.PlayOneShot", Owner!.Handle, path, volumeScale);
    }
    public void Stop() { Validate(); NativeBridge.Call("AudioSource.Stop", Owner!.Handle); }
    public void Pause() { Validate(); NativeBridge.Call("AudioSource.Pause", Owner!.Handle); }
    public void UnPause() { Validate(); NativeBridge.Call("AudioSource.UnPause", Owner!.Handle); }
}
