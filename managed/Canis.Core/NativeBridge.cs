using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;

namespace Canis;

/// <summary>Typed generated bindings use this checked native dispatcher. Main thread only.</summary>
public static unsafe class NativeBridge
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct Value { public int Kind, Length; public ulong Data; public double X, Y, Z, W; }
    internal static delegate* unmanaged[Cdecl]<byte*, int, Value*, int, Value*, byte*, int, int> Dispatch;
    internal static int MainThread;
    internal static bool MetadataMode;
    internal static Func<string, object[], object?>? BrowserDispatch;
    private static readonly Dictionary<string, byte[]> names = [];

    public static void Call(string name, params object[] arguments) => Invoke(name, arguments);
    public static T Call<T>(string name, params object[] arguments) => (T)Invoke(name, arguments)!;
    private static object? Invoke(string name, object[] arguments)
    {
        if (MetadataMode) throw new InvalidOperationException("Constructors and field initializers cannot call engine APIs; use Awake/OnCreate.");
        if (Environment.CurrentManagedThreadId != MainThread) throw new InvalidOperationException("Canis engine access is main-thread only.");
        if (arguments.Length > 32) throw new ArgumentException("Too many native arguments.");
        if (BrowserDispatch is not null) return BrowserDispatch(name, arguments);
        if (Dispatch == null) throw new InvalidOperationException("Native bindings are unavailable.");
        if (!names.TryGetValue(name, out var bytes)) names[name] = bytes = Encoding.UTF8.GetBytes(name);
        Span<Value> values = stackalloc Value[arguments.Length]; values.Clear();
        Span<byte> error = stackalloc byte[2048]; error.Clear();
        try
        {
            for (int i = 0; i < arguments.Length; ++i) values[i] = Encode(arguments[i]);
            Value result;
            fixed (byte* binding = bytes, message = error)
            fixed (Value* args = values)
                if (Dispatch(binding, bytes.Length, args, values.Length, &result, message, error.Length) != 0)
                    throw new InvalidOperationException($"{name}: {Marshal.PtrToStringUTF8((nint)message)}");
            return result.Kind switch
            {
                0 => null, 1 => result.X != 0, 2 => (int)result.X, 3 => (float)result.X, 4 => result.X,
                5 => result.Data, 6 => new Vector3((float)result.X,(float)result.Y,(float)result.Z),
                7 => new Quaternion((float)result.X,(float)result.Y,(float)result.Z,(float)result.W),
                8 => new Vector4((float)result.X,(float)result.Y,(float)result.Z,(float)result.W),
                9 => Marshal.PtrToStringUTF8((nint)result.Data, result.Length) ?? "",
                _ => throw new InvalidOperationException("Unknown native result type.")
            };
        }
        finally { foreach (var value in values) if (value.Kind == 9) Marshal.FreeCoTaskMem((nint)value.Data); }
    }
    private static Value Encode(object argument) => argument switch
    {
        bool v => new() { Kind=1, X=v ? 1 : 0 }, int v => new() { Kind=2, X=v },
        float v => new() { Kind=3, X=v }, double v => new() { Kind=4, X=v }, ulong v => new() { Kind=5, Data=v },
        Vector3 v => new() { Kind=6, X=v.X, Y=v.Y, Z=v.Z }, Quaternion v => new() { Kind=7, X=v.X, Y=v.Y, Z=v.Z, W=v.W },
        Vector4 v => new() { Kind=8, X=v.X, Y=v.Y, Z=v.Z, W=v.W },
        string v => new() { Kind=9, Data=(ulong)Marshal.StringToCoTaskMemUTF8(v), Length=Encoding.UTF8.GetByteCount(v) },
        _ => throw new ArgumentException($"Unsupported native argument: {argument?.GetType().FullName ?? "null"}")
    };
}
