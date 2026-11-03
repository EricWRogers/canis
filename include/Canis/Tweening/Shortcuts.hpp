#pragma once
#include <Canis/Tweening/Tween.hpp>
namespace Canis {
struct Model;
struct Text;
struct Sprite2D;
struct Material;
struct RectLayout;
struct AudioSource;
struct Camera;
struct PointLight;
struct DirectionalLight;
} // namespace Canis
namespace Canis::Tweening {
// Values are explicit: scale is local; rotation is quaternion, never Euler
// degrees.
enum class Property {
  Move,
  LocalMove,
  Scale,
  RotateQuaternion,
  LocalRotateQuaternion,
  ModelColor,
  ModelFade,
  TextColor,
  TextFade,
  SpriteColor,
  SpriteFade,
  AnchorPos,
  AudioVolume,
  FieldOfView,
  PointIntensity,
  DirectionalIntensity,
  MaterialColor,
  MaterialFade
};
Tween PropertyTween(Entity, Property, Value, double);
Tween TweenMove(Entity, Vector3, double);
Tween TweenLocalMove(Entity, Vector3, double);
Tween TweenScale(Entity, Vector3, double);
Tween TweenRotateQuaternion(Entity, Quaternion, double);
Tween TweenLocalRotateQuaternion(Entity, Quaternion, double);
Tween TweenColor(Model &, Vector4, double);
Tween TweenFade(Model &, float, double);
Tween TweenColor(Text &, Vector4, double);
Tween TweenFade(Text &, float, double);
Tween TweenColor(Sprite2D &, Vector4, double);
Tween TweenFade(Sprite2D &, float, double);
Tween TweenColor(Material &, Vector4, double);
Tween TweenFade(Material &, float, double);
Tween TweenAnchorPos(RectLayout &, Vector2, double);
Tween TweenFade(AudioSource &, float, double);
Tween TweenFieldOfView(Camera &, float, double);
Tween TweenIntensity(PointLight &, float, double);
Tween TweenIntensity(DirectionalLight &, float, double);
// Select a supported component's property with PropertyTween for ambiguous
// entities.
void TweenKill(Entity);
void DisconnectShortcuts(TweenWorld &);
} // namespace Canis::Tweening
