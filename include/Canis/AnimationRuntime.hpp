#pragma once

#include <string>
#include <vector>

#include <Canis/Asset.hpp>
#include <Canis/ConfigData.hpp>

namespace Canis
{
    class App;
    class Entity;

    struct AnimationBindingTarget
    {
        Entity* entity = nullptr;
        const ScriptConf* conf = nullptr;
        void* component = nullptr;
    };

    std::vector<Entity*> GetAnimationChildren(Entity& _entity);
    Entity* FindAnimationPathEntity(Entity& _root, const std::string& _path);
    bool BuildAnimationRelativePath(Entity& _root, Entity& _target, std::string& _outPath);
    AnimationValue EvaluateAnimationTrack(const AnimationTrack& _track, float _time);
    bool ResolveAnimationTrackTarget(App& _app, Entity& _root, const AnimationTrack& _track, AnimationBindingTarget& _outTarget);
    bool CaptureAnimationTrackValue(App& _app, Entity& _root, const AnimationTrack& _track, AnimationValue& _outValue);
    bool ApplyAnimationTrack(App& _app, Entity& _root, const AnimationTrack& _track, float _time);
    bool ApplyAnimationClip(App& _app, Entity& _root, const AnimationClipAsset& _clip, float _time);
    bool DispatchAnimationEvent(App& _app, Entity& _root, const AnimationClipAsset& _clip, const std::string& _clipPath, const AnimationEvent& _event);
    void DispatchAnimationEventsBetween(
        App& _app,
        Entity& _root,
        const AnimationClipAsset& _clip,
        const std::string& _clipPath,
        float _previousTime,
        float _currentTime,
        bool _looped);
}
