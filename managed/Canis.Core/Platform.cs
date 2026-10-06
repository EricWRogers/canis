using System.Numerics;

namespace Canis;

public static class Application
{
    /// <summary>Runtime rendering limit. Zero disables pacing; simulation delta remains unchanged.</summary>
    public static float FrameLimit {get=>NativeBridge.Call<float>("Application.FrameLimit");set=>NativeBridge.Call("Application.SetFrameLimit",value);}
    public static void Quit() => NativeBridge.Call("Application.Quit");
}

public static class Window
{
    /// <summary>Actual fullscreen state; the window manager may apply changes asynchronously.</summary>
    public static bool Fullscreen
    {
        get => NativeBridge.Call<bool>("Window.Fullscreen");
        set => NativeBridge.Call("Window.SetFullscreen", value);
    }

    public static Vector2 RenderSize
    {
        get
        {
            var size = NativeBridge.Call<Vector3>("Window.RenderSize");
            return new(size.X, size.Y);
        }
    }
}

public static class Clipboard
{
    public static string Text
    {
        get => NativeBridge.Call<string>("Clipboard.Text");
        set => NativeBridge.Call("Clipboard.SetText", value);
    }
}

public static class Audio
{
    public static float MasterVolume
    {
        get => NativeBridge.Call<float>("Audio.MasterVolume");
        set => NativeBridge.Call("Audio.SetMasterVolume", value);
    }
}
