using System.Numerics;
using System.Text.Json;

namespace Canis;

public readonly record struct BonePose(int Index, string Name, int Parent, Vector3 Position, Quaternion Rotation);

/// <summary>Per-instance procedural poses. Unspecified bones retain their local animation transforms.</summary>
public static class Skeleton
{
    public static BonePose[] Sample(ModelAnimation animation, float advance = 0)
    {
        animation.Validate();
        using var document = JsonDocument.Parse(NativeBridge.Call<string>("Skeleton.Sample", animation.Entity.Handle, advance));
        return document.RootElement.EnumerateArray().Select((bone, index) =>
        {
            var p = bone.GetProperty("Position"); var q = bone.GetProperty("Rotation");
            return new BonePose(index, bone.GetProperty("Name").GetString()!, bone.GetProperty("Parent").GetInt32(),
                new(p[0].GetSingle(),p[1].GetSingle(),p[2].GetSingle()),
                new(q[0].GetSingle(),q[1].GetSingle(),q[2].GetSingle(),q[3].GetSingle()));
        }).ToArray();
    }
    public static void Apply(ModelAnimation animation, IEnumerable<BonePose> bones)
    {
        animation.Validate();
        string payload = JsonSerializer.Serialize(bones.Select(bone => new {
            bone.Index, Position = new[] { bone.Position.X, bone.Position.Y, bone.Position.Z },
            Rotation = new[] { bone.Rotation.X, bone.Rotation.Y, bone.Rotation.Z, bone.Rotation.W }
        }));
        NativeBridge.Call("Skeleton.Apply", animation.Entity.Handle, payload);
    }
    public static void Clear(ModelAnimation animation)
    {
        animation.Validate();
        NativeBridge.Call("Skeleton.Clear", animation.Entity.Handle);
    }
}
