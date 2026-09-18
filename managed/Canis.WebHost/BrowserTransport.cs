using System.Globalization;
using System.Numerics;
using System.Text.Json.Nodes;

namespace Canis.WebHost;

internal static class BrowserTransport
{
    internal static object? Invoke(string name, object[] arguments)
    {
        var args = new JsonArray();
        foreach (var argument in arguments) args.Add(Encode(argument));
        var request = new JsonObject { ["name"] = name, ["arguments"] = args };
        var response = JsonNode.Parse(Program.Dispatch(request.ToJsonString()))!;
        if (response["error"] is { } error) throw new InvalidOperationException($"{name}: {error.GetValue<string>()}");
        var value = response["result"]!;
        double Number(int i) => value["numbers"]![i]!.GetValue<double>();
        return value["kind"]!.GetValue<int>() switch
        {
            0 => null, 1 => Number(0) != 0, 2 => checked((int)Number(0)),
            3 => (float)Number(0), 4 => Number(0),
            5 => ulong.Parse(value["handle"]!.GetValue<string>(), CultureInfo.InvariantCulture),
            6 => new Vector3((float)Number(0), (float)Number(1), (float)Number(2)),
            7 => new Quaternion((float)Number(0), (float)Number(1), (float)Number(2), (float)Number(3)),
            8 => new Vector4((float)Number(0), (float)Number(1), (float)Number(2), (float)Number(3)),
            9 => value["text"]!.GetValue<string>(),
            _ => throw new InvalidOperationException("Unknown native web result kind.")
        };
    }

    private static JsonObject Numeric(int kind, params double[] values)
    {
        var numbers = new JsonArray();
        foreach (var value in values)
        {
            if (!double.IsFinite(value)) throw new ArgumentException("Native web values must be finite.");
            numbers.Add(value);
        }
        return new JsonObject { ["kind"] = kind, ["numbers"] = numbers };
    }

    private static JsonObject Encode(object argument) => argument switch
    {
        bool v => Numeric(1, v ? 1 : 0), int v => Numeric(2, v),
        float v => Numeric(3, v), double v => Numeric(4, v),
        ulong v => new() { ["kind"] = 5, ["handle"] = v.ToString(CultureInfo.InvariantCulture) },
        Vector3 v => Numeric(6, v.X, v.Y, v.Z),
        Quaternion v => Numeric(7, v.X, v.Y, v.Z, v.W),
        Vector4 v => Numeric(8, v.X, v.Y, v.Z, v.W),
        string v => new() { ["kind"] = 9, ["text"] = v },
        _ => throw new ArgumentException($"Unsupported native argument: {argument?.GetType().FullName ?? "null"}")
    };
}
