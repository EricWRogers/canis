#include <Canis/AnimationRuntime.hpp>

#include <algorithm>
#include <sstream>

#include <Canis/App.hpp>
#include <Canis/Entity.hpp>

namespace Canis
{
    namespace
    {
        void AppendUniqueAnimationChildren(std::vector<Entity*>& _children, Entity* _entity)
        {
            if (_entity == nullptr)
                return;

            if (_entity->HasComponent<Transform>())
            {
                for (Entity* child : _entity->GetComponent<Transform>().children)
                {
                    if (child != nullptr && std::find(_children.begin(), _children.end(), child) == _children.end())
                        _children.push_back(child);
                }
            }

            if (_entity->HasComponent<RectTransform>())
            {
                for (Entity* child : _entity->GetComponent<RectTransform>().children)
                {
                    if (child != nullptr && std::find(_children.begin(), _children.end(), child) == _children.end())
                        _children.push_back(child);
                }
            }
        }

        bool BuildAnimationRelativePathRecursive(Entity& _current, Entity& _target, std::string& _outPath)
        {
            if (&_current == &_target)
                return true;

            for (Entity* child : GetAnimationChildren(_current))
            {
                if (child == nullptr)
                    continue;

                std::string childPath = child->name;
                if (BuildAnimationRelativePathRecursive(*child, _target, childPath))
                {
                    _outPath = childPath;
                    return true;
                }
            }

            return false;
        }
    }

    std::vector<Entity*> GetAnimationChildren(Entity& _entity)
    {
        std::vector<Entity*> children = {};
        AppendUniqueAnimationChildren(children, &_entity);
        return children;
    }

    Entity* FindAnimationPathEntity(Entity& _root, const std::string& _path)
    {
        if (_path.empty())
            return &_root;

        Entity* current = &_root;
        std::stringstream stream(_path);
        std::string segment = {};
        while (std::getline(stream, segment, '/'))
        {
            if (segment.empty())
                continue;

            Entity* next = nullptr;
            for (Entity* child : GetAnimationChildren(*current))
            {
                if (child != nullptr && child->name == segment)
                {
                    next = child;
                    break;
                }
            }

            if (next == nullptr)
                return nullptr;

            current = next;
        }

        return current;
    }

    bool BuildAnimationRelativePath(Entity& _root, Entity& _target, std::string& _outPath)
    {
        _outPath.clear();
        return BuildAnimationRelativePathRecursive(_root, _target, _outPath);
    }

    AnimationValue EvaluateAnimationTrack(const AnimationTrack& _track, float _time)
    {
        if (_track.keys.empty())
            return {};

        if (_track.keys.size() == 1)
            return _track.keys.front().value;

        if (_time <= _track.keys.front().time)
            return _track.keys.front().value;

        if (_time >= _track.keys.back().time)
            return _track.keys.back().value;

        for (std::size_t i = 1; i < _track.keys.size(); ++i)
        {
            const AnimationKeyframe& next = _track.keys[i];
            if (_time > next.time)
                continue;

            const AnimationKeyframe& prev = _track.keys[i - 1];
            if (_track.interpolation == AnimationInterpolation::STEP || AnimationValueIsDiscrete(_track.type))
                return prev.value;

            const float duration = next.time - prev.time;
            if (duration <= 0.00001f)
                return next.value;

            const float t = (_time - prev.time) / duration;
            return LerpAnimationValue(prev.value, next.value, t);
        }

        return _track.keys.back().value;
    }

    bool ResolveAnimationTrackTarget(App& _app, Entity& _root, const AnimationTrack& _track, AnimationBindingTarget& _outTarget)
    {
        _outTarget = {};

        Entity* targetEntity = FindAnimationPathEntity(_root, _track.path);
        if (targetEntity == nullptr)
            return false;

        ScriptConf* conf = _app.GetScriptConf(_track.component);
        if (conf == nullptr || conf->Get == nullptr)
            return false;

        void* component = conf->Get(*targetEntity);
        if (component == nullptr)
            return false;

        if (!conf->registry.animationGetters.contains(_track.property) ||
            !conf->registry.animationSetters.contains(_track.property))
            return false;

        _outTarget.entity = targetEntity;
        _outTarget.conf = conf;
        _outTarget.component = component;
        return true;
    }

    bool CaptureAnimationTrackValue(App& _app, Entity& _root, const AnimationTrack& _track, AnimationValue& _outValue)
    {
        AnimationBindingTarget target = {};
        if (!ResolveAnimationTrackTarget(_app, _root, _track, target))
            return false;

        _outValue = target.conf->registry.animationGetters.at(_track.property)(target.component);
        return true;
    }

    bool ApplyAnimationTrack(App& _app, Entity& _root, const AnimationTrack& _track, float _time)
    {
        AnimationBindingTarget target = {};
        if (!ResolveAnimationTrackTarget(_app, _root, _track, target))
            return false;

        const AnimationValue value = EvaluateAnimationTrack(_track, _time);
        target.conf->registry.animationSetters.at(_track.property)(target.component, value);
        return true;
    }

    bool ApplyAnimationClip(App& _app, Entity& _root, const AnimationClipAsset& _clip, float _time)
    {
        bool appliedAny = false;

        for (const AnimationTrack& track : _clip.tracks)
            appliedAny = ApplyAnimationTrack(_app, _root, track, _time) || appliedAny;

        return appliedAny;
    }

    bool DispatchAnimationEvent(App& _app, Entity& _root, const AnimationClipAsset& _clip, const std::string& _clipPath, const AnimationEvent& _event)
    {
        (void)_clip;

        if (_event.name.empty())
            return false;

        Entity* targetEntity = FindAnimationPathEntity(_root, _event.path);
        if (targetEntity == nullptr)
            return false;

        AnimationEventContext context = {};
        context.sourceEntity = &_root;
        context.targetEntity = targetEntity;
        context.clipPath = _clipPath;
        context.eventName = _event.name;
        context.stringPayload = _event.stringPayload;
        context.floatPayload = _event.floatPayload;
        context.intPayload = _event.intPayload;
        return _app.DispatchAnimationEvent(*targetEntity, _event.script, _event.name, context);
    }

    void DispatchAnimationEventsBetween(
        App& _app,
        Entity& _root,
        const AnimationClipAsset& _clip,
        const std::string& _clipPath,
        float _previousTime,
        float _currentTime,
        bool _looped)
    {
        constexpr float kAnimationEventEpsilon = 0.0001f;

        if (_clip.events.empty())
            return;

        const auto dispatchRange = [&](float _startExclusive, float _endInclusive) -> void
        {
            for (const AnimationEvent& event : _clip.events)
            {
                if (event.time <= _startExclusive + kAnimationEventEpsilon)
                    continue;

                if (event.time > _endInclusive + kAnimationEventEpsilon)
                    continue;

                (void)DispatchAnimationEvent(_app, _root, _clip, _clipPath, event);
            }
        };

        if (!_looped || _currentTime >= _previousTime)
        {
            dispatchRange(_previousTime, _currentTime);
            return;
        }

        dispatchRange(_previousTime, std::max(_clip.length, _previousTime));
        dispatchRange(-kAnimationEventEpsilon, _currentTime);
    }
}
