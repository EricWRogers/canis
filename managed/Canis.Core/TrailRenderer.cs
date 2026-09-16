using System.Numerics;
namespace Canis;

public enum TrailMode { Ribbon, Tube }

public sealed class TrailRenderer : NativeComponent
{
    public TrailMode Mode { get => (TrailMode)GetField<int>("mode"); set => SetField("mode", (int)value); }
    public bool Emitting { get => GetField<bool>("emitting"); set => SetField("emitting", value); }
    public float Width { get => GetField<float>("width"); set => SetField("width", value); }
    public float Lifetime { get => GetField<float>("lifetime"); set => SetField("lifetime", value); }
    public float MinDistance { get => GetField<float>("minDistance"); set => SetField("minDistance", value); }
    public float BreakDistance { get => GetField<float>("breakDistance"); set => SetField("breakDistance", value); }
    public int MaxPoints { get => GetField<int>("maxPoints"); set => SetField("maxPoints", value); }
    public int TubeSides { get => GetField<int>("tubeSides"); set => SetField("tubeSides", value); }
    public Vector4 Color { get => GetField<Vector4>("color"); set => SetField("color", value); }
    public void Clear() { Validate(); NativeBridge.Call("TrailRenderer.Clear", Owner!.Handle); }
}
