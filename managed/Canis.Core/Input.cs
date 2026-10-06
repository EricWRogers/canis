using System.Numerics;

namespace Canis;

public enum GamepadButton
{
    South = 1, East = 2, West = 4, North = 8, Back = 16, Guide = 32, Start = 64,
    LeftStickPress = 128, RightStickPress = 256, LeftShoulder = 512, RightShoulder = 1024,
    DpadUp = 2048, DpadDown = 4096, DpadLeft = 8192, DpadRight = 16384
}
public enum ActionType { Button, Axis1D, Axis2D }
public enum InputScheme { KeyboardMouse, Gamepad }
public enum InputCancellation { None, FocusLost, Capture, MapDisabled, Disconnected, Reload, BackendChanged }
public readonly record struct ActionId(uint Value);
public readonly record struct InputMapId(uint Value);

/// <summary>A copy of the native frame snapshot. Reading does not consume edges.</summary>
public readonly struct ActionSnapshot
{
    public readonly bool Down, Pressed, Released, Canceled;
    public readonly ActionType Type;
    public readonly InputCancellation Cancellation;
    public readonly Vector2 Value;

    internal ActionSnapshot(Vector4 packed)
    {
        int flags = (int)packed.Z;
        Down = (flags & 1) != 0; Pressed = (flags & 2) != 0;
        Released = (flags & 4) != 0; Canceled = (flags & 8) != 0;
        Type = (ActionType)((int)packed.W % 4);
        Cancellation = (InputCancellation)((int)packed.W / 4);
        Value = new(packed.X, packed.Y);
    }

    public T Read<T>() where T : struct
    {
        if (typeof(T) == typeof(float) && Type == ActionType.Axis1D) return (T)(object)Value.X;
        if (typeof(T) == typeof(Vector2) && Type == ActionType.Axis2D) return (T)(object)Value;
        throw new InvalidOperationException($"Cannot read {Type} input as {typeof(T).Name}.");
    }
}

/// <summary>Native input state, published once per frame. Main thread only.</summary>
public static class Input
{
    public static void CaptureMouse(bool capture) => NativeBridge.Call("Input.CaptureMouse", capture);
    public static bool MouseCaptureRequested => NativeBridge.Call<bool>("Input.MouseCaptureRequested");
    public static bool IsMouseCaptured => NativeBridge.Call<bool>("Input.MouseCaptured");

    public static bool GetKey(Key key) => KeyState(key, 0);
    public static bool JustPressedKey(Key key) => KeyState(key, 1);
    public static bool JustReleasedKey(Key key) => KeyState(key, 2);
    private static bool KeyState(Key key, int edge) => NativeBridge.Call<bool>("Input.GetKey", (int)key, edge);

    public static bool GetButton(GamepadButton button) => GetButton(-1, button);
    public static bool JustPressedButton(GamepadButton button) => JustPressedButton(-1, button);
    public static bool JustReleasedButton(GamepadButton button) => JustReleasedButton(-1, button);
    public static bool GetButton(int controllerIndex, GamepadButton button) => ButtonState(controllerIndex, button, 0);
    public static bool JustPressedButton(int controllerIndex, GamepadButton button) => ButtonState(controllerIndex, button, 1);
    public static bool JustReleasedButton(int controllerIndex, GamepadButton button) => ButtonState(controllerIndex, button, 2);
    private static bool ButtonState(int controllerIndex, GamepadButton button, int edge) =>
        NativeBridge.Call<bool>("Input.GetButton", controllerIndex, (int)button, edge);

    public static bool GetLeftClick() => MouseButton(false, 0);
    public static bool JustLeftClicked() => MouseButton(false, 1);
    public static bool LeftClickReleased() => MouseButton(false, 2);
    public static bool GetRightClick() => MouseButton(true, 0);
    public static bool JustRightClicked() => MouseButton(true, 1);
    public static bool RightClickReleased() => MouseButton(true, 2);
    private static bool MouseButton(bool right, int edge) => NativeBridge.Call<bool>("Input.MouseButton", right, edge);

    public static Vector2 MouseDelta => XY(NativeBridge.Call<Vector3>("Input.MouseDelta"));
    public static Vector2 MousePosition => XY(NativeBridge.Call<Vector3>("Input.MousePosition"));
    public static int VerticalScroll => NativeBridge.Call<int>("Input.Scroll");
    public static string TextInput => NativeBridge.Call<string>("Input.Text");
    public static Vector2 GetLeftStick(int controllerIndex = -1) => XY(NativeBridge.Call<Vector3>("Input.Stick", controllerIndex, false));
    public static Vector2 GetRightStick(int controllerIndex = -1) => XY(NativeBridge.Call<Vector3>("Input.Stick", controllerIndex, true));
    public static float GetLeftTrigger(int controllerIndex = -1) => NativeBridge.Call<float>("Input.Trigger", controllerIndex, false);
    public static float GetRightTrigger(int controllerIndex = -1) => NativeBridge.Call<float>("Input.Trigger", controllerIndex, true);
    private static Vector2 XY(Vector3 value) => new(value.X, value.Y);

    public static bool Focused => NativeBridge.Call<bool>("Input.Focused");
    public static bool GamepadConnected => NativeBridge.Call<bool>("Input.GamepadConnected");
    public static InputScheme CurrentScheme => (InputScheme)NativeBridge.Call<int>("Input.Scheme");
    public static ActionSnapshot Action(ActionId action, int controllerIndex = -1) =>
        new(NativeBridge.Call<Vector4>("Input.Action", (ulong)action.Value, controllerIndex));
    public static ActionSnapshot Action<T>(T action, int controllerIndex = -1) where T : struct, Enum =>
        Action(new ActionId(Convert.ToUInt32(action)), controllerIndex);
    public static string Prompt(ActionId action, InputScheme? scheme = null) =>
        NativeBridge.Call<string>("Input.Prompt", (ulong)action.Value, scheme.HasValue ? (int)scheme.Value : -1);
    public static string Prompt<T>(T action, InputScheme? scheme = null) where T : struct, Enum =>
        Prompt(new ActionId(Convert.ToUInt32(action)), scheme);

    /// <summary>Install this game's schema/bindings; replaces the current action document. Maps start disabled.</summary>
    public static void LoadActions(string assetPath) => NativeBridge.Call("Input.LoadActions", assetPath);
    public static void EnableMap(InputMapId map) => NativeBridge.Call("Input.Map", (ulong)map.Value, true);
    public static void DisableMap(InputMapId map) => NativeBridge.Call("Input.Map", (ulong)map.Value, false);
    public static bool IsMapEnabled(InputMapId map) => NativeBridge.Call<bool>("Input.MapEnabled", (ulong)map.Value);
    public static void EnableMap<T>(T map) where T : struct, Enum => EnableMap(new InputMapId(Convert.ToUInt32(map)));
    public static void DisableMap<T>(T map) where T : struct, Enum => DisableMap(new InputMapId(Convert.ToUInt32(map)));
    public static bool IsMapEnabled<T>(T map) where T : struct, Enum => IsMapEnabled(new InputMapId(Convert.ToUInt32(map)));
}
