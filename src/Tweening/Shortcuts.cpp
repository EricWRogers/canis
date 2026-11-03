#include <Canis/AudioComponents.hpp>
#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Tweening/Shortcuts.hpp>
#include <Canis/Tweening/TweenWorld.hpp>

namespace Canis::Tweening {
namespace {
template <class T> void Connect(TweenWorld &w) {
  w.Owner()
      ->GetRegistry()
      .on_destroy<T>()
      .template connect<&TweenWorld::Removed<T>>(&w);
}
template <class T> void Disconnect(TweenWorld &w) {
  w.Owner()->GetRegistry().on_destroy<T>().disconnect(&w);
}
void Install(TweenWorld &w) {
  if (w.bindingsInstalled)
    return;
  Connect<Transform>(w);
  Connect<Model>(w);
  Connect<Text>(w);
  Connect<Sprite2D>(w);
  Connect<Material>(w);
  Connect<RectLayout>(w);
  Connect<AudioSource>(w);
  Connect<Camera>(w);
  Connect<PointLight>(w);
  Connect<DirectionalLight>(w);
  w.bindingsInstalled = true;
}
template <class T, class G, class S>
Tween Bind(Entity e, Value end, double duration, int channel, G get, S set) {
  if (!e.IsValid() || !e.HasComponent<T>())
    throw std::invalid_argument("Tween requires a live target component");
  auto &w = World(e.scene);
  Install(w);
  auto tween = Tween::Create(
      w, [e, get]() mutable { return Value(get(e.GetComponent<T>())); },
      [e, set](Value value) mutable { set(e.GetComponent<T>(), value); }, end,
      duration);
  w.Bind(tween, e, entt::type_hash<T>::value(), channel,
         [e] { return e.IsValid() && e.HasComponent<T>(); });
  return tween;
}
template <class T> Tween Color(Entity e, Value v, double seconds, bool alpha) {
  return Bind<T>(
      e, v, seconds, 0,
      [alpha](T &c) { return alpha ? Value(c.color.a) : Value(c.color); },
      [alpha](T &c, Value x) {
        if (alpha)
          c.color.a = x.As<float>();
        else
          c.color = x.As<Vector4>();
      });
}
} // namespace
void DisconnectShortcuts(TweenWorld &w) {
  Disconnect<Transform>(w);
  Disconnect<Model>(w);
  Disconnect<Text>(w);
  Disconnect<Sprite2D>(w);
  Disconnect<Material>(w);
  Disconnect<RectLayout>(w);
  Disconnect<AudioSource>(w);
  Disconnect<Camera>(w);
  Disconnect<PointLight>(w);
  Disconnect<DirectionalLight>(w);
  w.bindingsInstalled = false;
}
Tween PropertyTween(Entity e, Property property, Value value, double seconds) {
  ValueKind expected = ValueKind::Number;
  switch (property) {
  case Property::Move:
  case Property::LocalMove:
  case Property::Scale:
    expected = ValueKind::Vector3;
    break;
  case Property::RotateQuaternion:
  case Property::LocalRotateQuaternion:
    expected = ValueKind::Quaternion;
    break;
  case Property::ModelColor:
  case Property::TextColor:
  case Property::SpriteColor:
  case Property::MaterialColor:
    expected = ValueKind::Vector4;
    break;
  case Property::AnchorPos:
    expected = ValueKind::Vector2;
    break;
  default:
    break;
  }
  if (value.kind != expected)
    throw std::invalid_argument("Wrong tween property value type");
  switch (property) {
  case Property::Move:
    return Bind<Transform>(
        e, value, seconds, 0, [](auto &t) { return t.GetGlobalPosition(); },
        [](auto &t, Value v) {
          Matrix4 parent(1);
          if (auto *p = t.parent.template TryGetComponent<Transform>())
            parent = p->GetModelMatrix();
          if (t.useLocalMatrixPrefix)
            parent *= t.localMatrixPrefix;
          if (std::abs(glm::determinant(parent)) < 1e-8f)
            throw std::runtime_error("Singular tween parent transform");
          t.position =
              Vector3(glm::inverse(parent) * Vector4(v.As<Vector3>(), 1));
        });
  case Property::LocalMove:
    return Bind<Transform>(
        e, value, seconds, 0, [](auto &t) { return t.position; },
        [](auto &t, Value v) { t.position = v.As<Vector3>(); });
  case Property::Scale:
    return Bind<Transform>(
        e, value, seconds, 1, [](auto &t) { return t.scale; },
        [](auto &t, Value v) { t.scale = v.As<Vector3>(); });
  case Property::RotateQuaternion:
    return Bind<Transform>(
        e, value, seconds, 2, [](auto &t) { return t.GetGlobalRotation(); },
        [](auto &t, Value v) {
          if (t.useLocalMatrixPrefix)
            throw std::runtime_error(
                "Use local rotation for bone-space tweens");
          auto q = v.As<Quaternion>();
          if (auto *p = t.parent.template TryGetComponent<Transform>())
            q = glm::inverse(p->GetGlobalRotation()) * q;
          t.rotation = glm::normalize(q);
        });
  case Property::LocalRotateQuaternion:
    return Bind<Transform>(
        e, value, seconds, 2, [](auto &t) { return t.rotation; },
        [](auto &t, Value v) { t.rotation = v.As<Quaternion>(); });
  case Property::ModelColor:
  case Property::ModelFade:
    return Color<Model>(e, value, seconds, property == Property::ModelFade);
  case Property::TextColor:
  case Property::TextFade:
    return Color<Text>(e, value, seconds, property == Property::TextFade);
  case Property::SpriteColor:
  case Property::SpriteFade:
    return Color<Sprite2D>(e, value, seconds, property == Property::SpriteFade);
  case Property::MaterialColor:
  case Property::MaterialFade:
    return Color<Material>(e, value, seconds,
                           property == Property::MaterialFade);
  case Property::AnchorPos:
    return Bind<RectLayout>(
        e, value, seconds, 0, [](auto &t) { return t.GetLocalPosition(); },
        [](auto &t, Value v) { t.SetLocalPosition(v.As<Vector2>()); });
  case Property::AudioVolume:
    return Bind<AudioSource>(
        e, value, seconds, 0, [](auto &t) { return t.volume; },
        [](auto &t, Value v) { t.volume = std::max(0.f, v.As<float>()); });
  case Property::FieldOfView:
    return Bind<Camera>(
        e, value, seconds, 0, [](auto &t) { return t.fovDegrees; },
        [](auto &t, Value v) {
          t.fovDegrees = std::clamp(v.As<float>(), 1.f, 179.f);
        });
  case Property::PointIntensity:
    return Bind<PointLight>(
        e, value, seconds, 0, [](auto &t) { return t.intensity; },
        [](auto &t, Value v) { t.intensity = std::max(0.f, v.As<float>()); });
  case Property::DirectionalIntensity:
    return Bind<DirectionalLight>(
        e, value, seconds, 0, [](auto &t) { return t.intensity; },
        [](auto &t, Value v) { t.intensity = std::max(0.f, v.As<float>()); });
  }
  throw std::invalid_argument("Unknown tween property");
}
Tween TweenMove(Entity e, Vector3 v, double t) {
  return PropertyTween(e, Property::Move, v, t);
}
Tween TweenLocalMove(Entity e, Vector3 v, double t) {
  return PropertyTween(e, Property::LocalMove, v, t);
}
Tween TweenScale(Entity e, Vector3 v, double t) {
  return PropertyTween(e, Property::Scale, v, t);
}
Tween TweenRotateQuaternion(Entity e, Quaternion v, double t) {
  return PropertyTween(e, Property::RotateQuaternion, v, t);
}
Tween TweenLocalRotateQuaternion(Entity e, Quaternion v, double t) {
  return PropertyTween(e, Property::LocalRotateQuaternion, v, t);
}
Tween TweenColor(Model &target, Vector4 value, double seconds) {
  return PropertyTween(target.entity, Property::ModelColor, value, seconds);
}
Tween TweenFade(Model &target, float value, double seconds) {
  return PropertyTween(target.entity, Property::ModelFade, value, seconds);
}
Tween TweenColor(Text &target, Vector4 value, double seconds) {
  return PropertyTween(target.entity, Property::TextColor, value, seconds);
}
Tween TweenFade(Text &target, float value, double seconds) {
  return PropertyTween(target.entity, Property::TextFade, value, seconds);
}
Tween TweenColor(Sprite2D &target, Vector4 value, double seconds) {
  return PropertyTween(target.entity, Property::SpriteColor, value, seconds);
}
Tween TweenFade(Sprite2D &target, float value, double seconds) {
  return PropertyTween(target.entity, Property::SpriteFade, value, seconds);
}
Tween TweenColor(Material &target, Vector4 value, double seconds) {
  return PropertyTween(target.entity, Property::MaterialColor, value, seconds);
}
Tween TweenFade(Material &target, float value, double seconds) {
  return PropertyTween(target.entity, Property::MaterialFade, value, seconds);
}
Tween TweenAnchorPos(RectLayout &target, Vector2 value, double seconds) {
  return PropertyTween(target.entity, Property::AnchorPos, value, seconds);
}
Tween TweenFade(AudioSource &target, float value, double seconds) {
  return PropertyTween(target.entity, Property::AudioVolume, value, seconds);
}
Tween TweenFieldOfView(Camera &target, float value, double seconds) {
  return PropertyTween(target.entity, Property::FieldOfView, value, seconds);
}
Tween TweenIntensity(PointLight &target, float value, double seconds) {
  return PropertyTween(target.entity, Property::PointIntensity, value, seconds);
}
Tween TweenIntensity(DirectionalLight &target, float value, double seconds) {
  return PropertyTween(target.entity, Property::DirectionalIntensity, value,
                       seconds);
}
void TweenKill(Entity e) { Tween::Kill(e); }
} // namespace Canis::Tweening
