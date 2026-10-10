namespace Canis;

/// <summary>Renderer-only level of detail. The authored model remains the collision source.</summary>
public sealed class LODGroup : NativeComponent
{
    public bool Enabled { get => GetField<bool>("enabled"); set => SetField("enabled", value); }
    public string ProfilePath { get => GetField<string>("profilePath"); set => SetField("profilePath", value); }
    public float Bias { get => GetField<float>("bias"); set => SetField("bias", value); }
    public float Hysteresis { get => GetField<float>("hysteresis"); set => SetField("hysteresis", value); }
    public float CullHeight { get => GetField<float>("cullHeight"); set => SetField("cullHeight", value); }
    public int ShadowBias { get => GetField<int>("shadowBias"); set => SetField("shadowBias", value); }
    public int ForcedLevel { get => GetField<int>("forcedLevel"); set => SetField("forcedLevel", value); }
    public bool DebugColors { get => GetField<bool>("debugColors"); set => SetField("debugColors", value); }
    public int ActiveLevel { get { Validate(); return NativeBridge.Call<int>("LODGroup.Level", Owner!.Handle); } }
    public float ScreenHeight { get { Validate(); return NativeBridge.Call<float>("LODGroup.ScreenHeight", Owner!.Handle); } }
}
