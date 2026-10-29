#pragma once
#include <Canis/Asset.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Canis::AnimationTimeline
{
    constexpr float FramesPerSecond = 60.0f;
    constexpr float KeyEpsilon = .0001f;
    inline int Frame(float time)
    {
        if (!std::isfinite(time) || time <= 0) return 0;
        return int(std::min(std::round(double(time) * FramesPerSecond), double(std::numeric_limits<int>::max())));
    }
    inline float Time(int frame) { return std::max(frame, 0) / FramesPerSecond; }
    inline float Normalize(float time, float length, bool loop)
    {
        if (!std::isfinite(time) || !std::isfinite(length) || length <= 0) return 0;
        if (!loop) return std::clamp(time, 0.0f, length);
        float result = std::fmod(time, length);
        return result < 0 ? result + length : result;
    }
    // The display frame is derived from this clock, never written back on redraw.
    inline void Advance(float& time, bool& playing, float delta, float length, float speed, bool loop)
    {
        if (playing && std::isfinite(delta) && std::isfinite(speed))
            time += std::max(delta, 0.0f) * std::max(speed, 0.0f);
        if (playing && !loop && time >= length) playing = false;
        time = Normalize(time, length, playing && loop);
    }
    inline float TimeAtFraction(float fraction, float start, float span, bool snap)
    {
        if (!std::isfinite(fraction) || !std::isfinite(start) || !std::isfinite(span)) return 0;
        const float time = std::max(0.0f, start + std::clamp(fraction, 0.0f, 1.0f) * std::max(span, 0.0f));
        return snap ? Time(Frame(time)) : time;
    }
    inline float AdjacentKey(const AnimationClipAsset& clip, float time, bool next, int selectedTrack = -1)
    {
        float result = next ? clip.length : 0.0f;
        auto visit = [&](float key) {
            if (!std::isfinite(key)) return;
            if (next && key > time + KeyEpsilon) result = std::min(result, key);
            if (!next && key < time - KeyEpsilon) result = std::max(result, key);
        };
        if (selectedTrack >= 0 && selectedTrack < int(clip.tracks.size()))
            for (const auto& key : clip.tracks[selectedTrack].keys) visit(key.time);
        else
        {
            for (const auto& track : clip.tracks) for (const auto& key : track.keys) visit(key.time);
            for (const auto& event : clip.events) visit(event.time);
        }
        return Normalize(result, clip.length, false);
    }
    inline void UpsertKey(AnimationTrack& track, float time, const AnimationValue& value)
    {
        if (!std::isfinite(time)) return;
        time = std::max(time, 0.0f);
        auto found = std::find_if(track.keys.begin(), track.keys.end(), [&](const auto& key) {
            return std::abs(key.time - time) <= KeyEpsilon;
        });
        if (found != track.keys.end()) found->value = value;
        else track.keys.push_back({time, value});
        std::stable_sort(track.keys.begin(), track.keys.end(), [](const auto& a, const auto& b) { return a.time < b.time; });
    }
}
