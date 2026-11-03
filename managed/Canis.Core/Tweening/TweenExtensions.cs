using System.Numerics;
namespace Canis.Tweening;

public static class TweenExtensions
{
    public static Tween TweenMove(this Transform target, Vector3 end, double seconds) => Tween.Property(target, 0, end, seconds);
    public static Tween TweenLocalMove(this Transform target, Vector3 end, double seconds) => Tween.Property(target, 1, end, seconds);
    public static Tween TweenScale(this Transform target, Vector3 end, double seconds) => Tween.Property(target, 2, end, seconds);
    public static Tween TweenRotateQuaternion(this Transform target, Quaternion end, double seconds) => Tween.Property(target, 3, end, seconds);
    public static Tween TweenLocalRotateQuaternion(this Transform target, Quaternion end, double seconds) => Tween.Property(target, 4, end, seconds);
    public static Tween TweenColor(this Model target, Vector4 end, double seconds) => Tween.Property(target, 5, end, seconds);
    public static Tween TweenFade(this Model target, float end, double seconds) => Tween.Property(target, 6, end, seconds);
    public static Tween TweenColor(this Text target, Vector4 end, double seconds) => Tween.Property(target, 7, end, seconds);
    public static Tween TweenFade(this Text target, float end, double seconds) => Tween.Property(target, 8, end, seconds);
    public static Tween TweenColor(this Sprite2D target, Vector4 end, double seconds) => Tween.Property(target, 9, end, seconds);
    public static Tween TweenFade(this Sprite2D target, float end, double seconds) => Tween.Property(target, 10, end, seconds);
    public static Tween TweenAnchorPos(this RectLayout target, Vector2 end, double seconds) => Tween.Property(target, 11, end, seconds);
    public static Tween TweenFade(this AudioSource target, float end, double seconds) => Tween.Property(target, 12, end, seconds);
    public static Tween TweenFieldOfView(this Camera target, float end, double seconds) => Tween.Property(target, 13, end, seconds);
    public static Tween TweenIntensity(this PointLight target, float end, double seconds) => Tween.Property(target, 14, end, seconds);
    public static Tween TweenIntensity(this DirectionalLight target, float end, double seconds) => Tween.Property(target, 15, end, seconds);
    public static Tween TweenColor(this Material target, Vector4 end, double seconds) => Tween.Property(target, 16, end, seconds);
    public static Tween TweenFade(this Material target, float end, double seconds) => Tween.Property(target, 17, end, seconds);
    public static void TweenKill(this NativeComponent target) { target.Validate(); Tween.Kill(target.Entity); }
}
