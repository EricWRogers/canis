using System.Numerics;
namespace Canis;

/// <summary>Tracked input and a full orientation origin for seated vehicles.</summary>
public static class VRInput
{
    public static bool Available => NativeBridge.Call<bool>("VR.Available");
    public static bool Focused => NativeBridge.Call<bool>("VR.Focused");
    public static bool Simulated => NativeBridge.Call<bool>("VR.Simulated");
    public static Vector3 HeadTrackingPosition => NativeBridge.Call<Vector3>("VR.HeadTrackingPosition");
    public static Quaternion HeadTrackingRotation => NativeBridge.Call<Quaternion>("VR.HeadTrackingRotation");
    /// <summary>Current near/far depth range. An active scene Camera supplies this
    /// before rendering; set that component's clips for persistent scene changes.</summary>
    public static Vector2 ClipPlanes
    {
        get { var planes=NativeBridge.Call<Vector3>("VR.ClipPlanes");return new(planes.X,planes.Y); }
        set => NativeBridge.Call("VR.SetClipPlanes",value.X,value.Y);
    }
    public static bool Tracked(int hand) => NativeBridge.Call<bool>("VR.Tracked",hand);
    public static Vector3 Position(int hand) => NativeBridge.Call<Vector3>("VR.Position",hand);
    public static Quaternion Rotation(int hand) => NativeBridge.Call<Quaternion>("VR.Rotation",hand);
    public static float Grip(int hand) => NativeBridge.Call<float>("VR.Squeeze",hand);
    public static float Trigger(int hand) => NativeBridge.Call<float>("VR.Trigger",hand);
    public static bool Button(int hand,int button) => NativeBridge.Call<bool>("VR.Button",hand,button);
    public static bool GrappleToggle => Button(0,4) || Button(1,4);
    public static bool WeaponSwitch => Button(0,3) || Button(1,3);
    public static Vector3 Stick(int hand) => NativeBridge.Call<Vector3>("VR.Stick",hand);
    public static bool Interact => Button(0,0) || Button(1,2);
    public static void SetOrigin(Vector3 position,Quaternion rotation) => NativeBridge.Call("VR.SetOrigin",position,rotation);
    public static bool Haptic(int hand,float strength,float seconds) => NativeBridge.Call<bool>("VR.Haptic",hand,strength,seconds);
}
