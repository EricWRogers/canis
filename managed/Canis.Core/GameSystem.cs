namespace Canis;

/// <summary>Scene-wide processor; use World.Query for managed component classes.</summary>
public abstract class GameSystem
{
    public virtual void Start() { }
    public virtual void Update(float deltaTime) { }
    public virtual void Destroy() { }
}

public static class Log
{
    // The stable host owns this callback. Gameplay must not retain callbacks across reloads.
    internal static Action<string>? Sink;
    public static void Info(string message) => Sink?.Invoke(message);
}
