using System.Reflection;
using System.Runtime.InteropServices.JavaScript;
using System.Runtime.Versioning;
using Canis;

[assembly: SupportedOSPlatform("browser")]

namespace Canis.WebHost;

public static partial class Program
{
    private sealed class Instance(GameSystem system) { public GameSystem System = system; public bool Failed; }
    private static readonly List<Instance> instances = [];
    private static Type[] systems = [];
    private static bool initialized, running;

    public static void Main() { }

    [JSImport("dispatch", "canis")]
    internal static partial string Dispatch(string request);

    [JSExport]
    public static string Command(int operation, double delta)
    {
        try
        {
            switch (operation)
            {
                case 0:
                    if (initialized) break;
                    NativeBridge.MainThread = Environment.CurrentManagedThreadId;
                    NativeBridge.BrowserDispatch = BrowserTransport.Invoke;
                    Log.Sink = Console.WriteLine;
                    var types = Assembly.Load("Game.Runtime").GetTypes();
                    systems = types.Where(t => !t.IsAbstract && typeof(GameSystem).IsAssignableFrom(t))
                        .OrderBy(t => t.FullName, StringComparer.Ordinal).ToArray();
                    foreach (var type in systems)
                        if (!type.IsPublic || type.ContainsGenericParameters || type.GetConstructor(Type.EmptyTypes) is null)
                            throw new InvalidOperationException($"{type.FullName} must be public, non-generic, with a public parameterless constructor.");
                    var descriptions = ComponentStore.Describe(types);
                    ComponentStore.Configure(descriptions, ComponentStore.Manifest(descriptions));
                    initialized = true;
                    Console.WriteLine("C#: browser scripts ready");
                    break;
                case 1:
                    if (!initialized) throw new InvalidOperationException("Browser scripts are not initialized.");
                    if (running) break;
                    running = true;
                    ComponentStore.Start();
                    foreach (var type in systems)
                    {
                        try
                        {
                            var instance = new Instance((GameSystem)Activator.CreateInstance(type)!);
                            instances.Add(instance);
                            Invoke(instance, s => s.Start());
                        }
                        catch (Exception error) { Log.ReportException(error, $"C# constructor {type.FullName}"); }
                    }
                    break;
                case 2:
                    if (!running) break;
                    ComponentStore.Update((float)delta);
                    foreach (var instance in instances)
                        if (!instance.Failed) Invoke(instance, s => s.Update((float)delta));
                    break;
                case 3:
                    ComponentStore.Stop();
                    foreach (var instance in instances) Invoke(instance, s => s.Destroy());
                    instances.Clear();
                    running = false;
                    break;
                default: throw new ArgumentOutOfRangeException(nameof(operation));
            }
            return "";
        }
        catch (Exception error) { Console.Error.WriteLine(error); return error.ToString(); }
    }

    private static void Invoke(Instance instance, Action<GameSystem> callback)
    {
        try { callback(instance.System); }
        catch (Exception error)
        {
            instance.Failed = true;
            Log.ReportException(error, $"C# system {instance.System.GetType().FullName}");
        }
    }
}
