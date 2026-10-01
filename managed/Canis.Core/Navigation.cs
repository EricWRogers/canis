using System.Numerics;
using System.Text.Json;
namespace Canis;

/// <summary>Queries the native editor-authored navigation surface. Empty means no traversable route.</summary>
public static class Navigation
{
    public static Vector3[] FindPath(this NavMeshSurface surface, Vector3 start, Vector3 destination)
    {
        ArgumentNullException.ThrowIfNull(surface);
        surface.Validate();
        using var document = JsonDocument.Parse(NativeBridge.Call<string>("Navigation.FindPath", surface.Entity.Handle, start, destination));
        return document.RootElement.EnumerateArray().Select(point => new Vector3(point[0].GetSingle(), point[1].GetSingle(), point[2].GetSingle())).ToArray();
    }
}
