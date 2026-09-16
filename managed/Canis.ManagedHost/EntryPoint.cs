using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Text;
using Canis;

namespace Canis.ManagedHost;

public static unsafe class EntryPoint
{
    // Version 1: kept in sync with CSharpRuntime.cpp; never passes C++/managed objects.
    [StructLayout(LayoutKind.Sequential)]
    public struct Command { public int Operation; public int Length; public nint Payload; public double Delta; }
    private sealed class GameContext(string path) : AssemblyLoadContext(isCollectible: true)
    {
        private readonly AssemblyDependencyResolver resolver = new(path);
        protected override Assembly? Load(AssemblyName name)
        {
            if (name.Name == typeof(GameSystem).Assembly.GetName().Name) return typeof(GameSystem).Assembly;
            var file = resolver.ResolveAssemblyToPath(name);
            return file is null ? null : LoadFromAssemblyPath(file);
        }
    }
    private sealed record Generation(GameContext Context, Type[] Types, string Path, ComponentStore.Description[] Components, string Manifest);
    private sealed class Instance(GameSystem system) { public GameSystem System = system; public bool Failed; }
    private static Generation? active, pending;
    private static readonly List<Instance> instances = [];
    private static readonly List<WeakReference> retired = [];
    private static delegate* unmanaged[Cdecl]<byte*, int, void> logger;
    private static bool running;

    private static void Write(string message)
    {
        var bytes = Encoding.UTF8.GetBytes(message);
        fixed (byte* data = bytes) { if (logger != null) logger(data, bytes.Length); }
    }

    [UnmanagedCallersOnly(CallConvs = [typeof(CallConvCdecl)])]
    public static int Dispatch(Command* command, int size)
    {
        if (command == null || size != sizeof(Command)) return -1;
        try
        {
            switch (command->Operation)
            {
                case 0:
                    logger = (delegate* unmanaged[Cdecl]<byte*, int, void>)command->Payload;
                    Log.Sink = Write;
                    return 0;
                case 9:
                    NativeBridge.Dispatch = (delegate* unmanaged[Cdecl]<byte*, int, NativeBridge.Value*, int, NativeBridge.Value*, byte*, int, int>)command->Payload;
                    NativeBridge.MainThread = Environment.CurrentManagedThreadId;
                    return 0;
                case 8:
                    Stop();
                    if (active != null && pending == null) Stage(active.Path);
                    return 0;
                case 1:
                    Stage(Encoding.UTF8.GetString((byte*)command->Payload, command->Length));
                    return 0;
                case 2:
                    if (running) throw new InvalidOperationException("Stop Play before replacing C# assemblies.");
                    if (pending is null) return 0;
                    ComponentStore.Clear(); Retire(active); active = pending; pending = null;
                    ComponentStore.Configure(active.Components,active.Manifest);
                    Write($"C#: loaded {active.Types.Length} GameSystem type(s).");
                    return 0;
                case 3:
                    if (running || active is null) return 0;
                    running = true;
                    ComponentStore.Start();
                    foreach (var type in active.Types)
                    {
                        try
                        {
                            var instance = new Instance((GameSystem)Activator.CreateInstance(type)!);
                            instances.Add(instance);
                            Invoke(instance, s => s.Start());
                        }
                        catch (Exception error) { Write($"C# {type.FullName} constructor: {error}"); }
                    }
                    return 0;
                case 4:
                    if (running) ComponentStore.Update((float)command->Delta);
                    if (running) foreach (var instance in instances)
                        if (!instance.Failed) Invoke(instance, s => s.Update((float)command->Delta));
                    return 0;
                case 10:
                    if(!running || active is null || pending is null)return -1;
                    var snapshot=ComponentStore.Capture();
                    ComponentStore.ValidateSnapshot(pending.Components,snapshot);
                    foreach(var instance in instances)Invoke(instance,s=>s.Destroy());instances.Clear();
                    ComponentStore.Stop();ComponentStore.Clear();Retire(active);active=pending;pending=null;
                    ComponentStore.Configure(active.Components,active.Manifest);ComponentStore.StartRestored(snapshot);
                    foreach(var type in active.Types){var instance=new Instance((GameSystem)Activator.CreateInstance(type)!);instances.Add(instance);Invoke(instance,s=>s.Start());}
                    Write("C#: stateful component reload committed; scene-wide systems restarted.");return 0;
                case 5: Stop(); return 0;
                case 6:
                    Stop(); Retire(pending); pending = null; Retire(active); active = null;
                    ComponentStore.Clear();
                    Log.Sink = null; logger = null; NativeBridge.Dispatch = null; return 0;
                case 7: // Explicit test/diagnostic only; never collect every frame.
                    GC.Collect(); GC.WaitForPendingFinalizers(); GC.Collect();
                    retired.RemoveAll(reference => !reference.IsAlive);
                    return retired.Count;
                default: return -1;
            }
        }
        catch (Exception error) { Write($"C# host: {error}"); return -1; }
    }

    private static void Stage(string path)
    {
        var context = new GameContext(path);
        try
        {
            var all = context.LoadFromAssemblyPath(path).GetTypes();
            var components=ComponentStore.Describe(all);
            var manifest=ComponentStore.Manifest(components);
            var types = all
                .Where(t => !t.IsAbstract && typeof(GameSystem).IsAssignableFrom(t))
                .OrderBy(t => t.FullName, StringComparer.Ordinal).ToArray();
            foreach (var type in types)
                if (!type.IsPublic || type.ContainsGenericParameters || type.GetConstructor(Type.EmptyTypes) is null)
                    throw new InvalidOperationException($"{type.FullName} must be public, non-generic, and have a public parameterless constructor.");
            Retire(pending); pending = new Generation(context, types, path,components,manifest);
        }
        catch { context.Unload(); throw; }
    }
    private static void Invoke(Instance instance, Action<GameSystem> callback)
    {
        try { callback(instance.System); }
        catch (Exception error)
        {
            instance.Failed = true;
            Write($"C# {instance.System.GetType().FullName}: {error}");
        }
    }
    private static void Stop()
    {
        ComponentStore.Stop();
        foreach (var instance in instances) Invoke(instance, s => s.Destroy());
        instances.Clear(); running = false;
    }
    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void Retire(Generation? generation)
    {
        if (generation is null) return;
        retired.Add(new WeakReference(generation.Context));
        generation.Context.Unload();
    }
}
