using System;
using System.Numerics;
using Canis.Tweening;
namespace Canis.Tests;

// Shared desktop/browser integration checks. Uses the real native scheduler.
public static class TweenApiChecks
{
    public static void Run()
    {
        double number = 1_000_000_000.25;
        var value = Tween.To(() => number, x => number = x, number + 10, 1)
            .SetEase(Ease.Linear).SetUpdate(UpdateType.Manual).SetAutoKill(false);
        Tween.ManualUpdate(.5, .5);
        Require(Math.Abs(number - 1_000_000_005.25) < 1e-6, "Native bridge preserves double precision");
        value.Pause(); Tween.ManualUpdate(.5, .5);
        Require(Math.Abs(number - 1_000_000_005.25) < 1e-6, "Managed Pause");
        value.Play(); Tween.ManualUpdate(.5, .5);
        Require(value.IsComplete, "Managed completion status"); value.Kill();

        Vector2 vector = Vector2.Zero;
        Tween.To(() => vector, x => vector = x, new Vector2(4, 8), 1).SetEase(Ease.Linear).SetUpdate(UpdateType.Manual);
        Tween.ManualUpdate(.5, .5); Require(vector == new Vector2(2, 4), "Vector2 typed transport");
        Tween.ManualUpdate(.5, .5);

        float amount = 0;
        string events = "";
        var sequence = Tween.Sequence()
            .Append(Tween.To(() => amount, x => amount = x, 10f, 1).SetEase(Ease.Linear))
            .Append(Tween.To(() => amount, x => amount = x, 20f, 1).SetEase(Ease.Linear))
            .AppendCallback(() => events += "C")
            .SetUpdate(UpdateType.Manual)
            .OnComplete(() => events += "D").OnKill(() => events += "K");
        Tween.ManualUpdate(1.5, 1.5);
        Require(Math.Abs(amount - 15) < .0001, "Sequence getter sees previous child write within the same step");
        Tween.ManualUpdate(.5, .5);
        Require(amount == 20 && events == "CDK" && !sequence.IsActive, "Managed sequence callbacks and cleanup");

        Entity target = Entity.Create("Tween temporary target");
        Transform transform = target.AddComponent<Transform>();
        var rotation = transform.TweenLocalRotateQuaternion(Quaternion.CreateFromAxisAngle(Vector3.UnitY, 1.5f), 1)
            .SetEase(Ease.Linear).SetUpdate(UpdateType.Manual);
        Tween.ManualUpdate(.5, .5);
        Require(Math.Abs(Angle(Quaternion.Identity, transform.LocalRotation) - .75f) < .001f, "Managed/native quaternion parity");
        target.RemoveComponent<Transform>(); target.AddComponent<Transform>();
        Tween.ManualUpdate(.5, .5);
        Require(!rotation.IsActive && target.Transform.LocalPosition == Vector3.Zero, "Removed component cannot be revived by tween");
        float linkedValue = 0;
        var linked = Tween.To(() => linkedValue, x => linkedValue = x, 1f, 1).SetLink(target).SetUpdate(UpdateType.Manual);
        target.Destroy(); Tween.ManualUpdate(.5, .5);
        Require(!linked.IsActive && linkedValue == 0, "Destroyed owner cancels managed delegate tween");

        Entity presentation = Entity.Create("Tween presentation properties");
        Model model = presentation.AddComponent<Model>();
        RectLayout rect = presentation.AddComponent<RectLayout>();
        AudioSource audio = presentation.AddComponent<AudioSource>();
        Camera camera = presentation.AddComponent<Camera>();
        PointLight light = presentation.AddComponent<PointLight>();
        model.TweenColor(new Vector4(.2f, .4f, .6f, .8f), 1).SetUpdate(UpdateType.Manual);
        rect.TweenAnchorPos(new Vector2(12, 24), 1).SetUpdate(UpdateType.Manual);
        audio.TweenFade(.3f, 1).SetUpdate(UpdateType.Manual);
        camera.TweenFieldOfView(90, 1).SetUpdate(UpdateType.Manual);
        light.TweenIntensity(2, 1).SetUpdate(UpdateType.Manual);
        Tween.ManualUpdate(1, 1);
        Require(Vector4.Distance(model.Color, new Vector4(.2f, .4f, .6f, .8f)) < .0001f, "Instance color shortcut");
        Require(rect.GetField<Vector2>("position") == new Vector2(12, 24), "UI anchored-position shortcut");
        Require(Math.Abs(audio.Volume - .3f) < .0001f && Math.Abs(camera.FovDegrees - 90) < .0001f && Math.Abs(light.Intensity - 2) < .0001f, "Audio/camera/light shortcuts");
        model.TweenFade(0, .2).SetUpdate(UpdateType.Manual); Tween.ManualUpdate(.2, .2);
        Require(model.Color.W == 0 && Math.Abs(model.Color.X - .2f) < .0001f, "Fade preserves RGB");
        presentation.Destroy();

        var samples = new float[1000];
        var handles = new Tween[1000];
        for (int i = 0; i < samples.Length; i++)
        {
            int index = i;
            handles[i] = Tween.To(() => samples[index], x => samples[index] = x, 1f, 100).SetUpdate(UpdateType.Manual);
        }
        Tween.ManualUpdate(.01, .01);
        long allocated = GC.GetAllocatedBytesForCurrentThread();
        var timer = System.Diagnostics.Stopwatch.StartNew();
        for (int i = 0; i < 60; i++) Tween.ManualUpdate(.01, .01);
        timer.Stop();
        Log.Info($"LDS_TWEEN_BENCHMARK managed1000 msPerUpdate={timer.Elapsed.TotalMilliseconds / 60:F3} bytesPerUpdate={(GC.GetAllocatedBytesForCurrentThread() - allocated) / 60}");
        foreach (var handle in handles) handle.Kill();
    }

    private static float Angle(Quaternion a, Quaternion b) => 2 * MathF.Acos(Math.Clamp(Math.Abs(Quaternion.Dot(Quaternion.Normalize(a), Quaternion.Normalize(b))), 0, 1));
    private static void Require(bool condition, string message) { if (!condition) throw new Exception("Tween API: " + message); }
}
