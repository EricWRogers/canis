#include <Canis/Asset.hpp>
#include <Canis/Audio.hpp>
#include <Canis/Canis.hpp>
#include <Canis/Debug.hpp>
#include <SDL3/SDL.h>
#if CANIS_STEAM_AUDIO
#include <phonon.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>

namespace Canis::Audio
{
namespace
{
constexpr int BlockSize = 512;
constexpr int MaxVoices = 128;
struct Voice
{
    int id = -1;
    std::shared_ptr<const std::vector<float>> samples;
    PlaybackSettings settings;
    double cursor = 0;
    int loops = 0;
    bool active = false, paused = false, inputEnded = false;
#if CANIS_STEAM_AUDIO
    IPLBinauralEffect effect = nullptr;
#endif
    std::array<float, BlockSize> mono{}, left{}, right{};
    std::array<float, BlockSize * 2> dry{};
};
struct AudioState
{
    std::mutex mutex;
    SDL_AudioStream *stream = nullptr;
    bool initialized = false, ownsSDL = false, offline = false;
    std::array<Voice, MaxVoices> voices;
    int nextId = 1; // Never reset: a handle must not refer to another sound after reinitialization.
    float master = 1, music = 1, sfx = 1, listenerVolume = 1;
    bool muted = false, scenePaused = false, listenerActive = false;
    Vector3 listenerPosition = Vector3(0);
    Quaternion listenerRotation = Quaternion(1, 0, 0, 0);
#if CANIS_STEAM_AUDIO
    IPLContext context = nullptr;
    IPLHRTF hrtf = nullptr;
#endif
};
AudioState &State()
{
    static AudioState state;
    return state;
}
float Clamp(float value, float lo, float hi, float fallback)
{
    return std::isfinite(value) ? std::clamp(value, lo, hi) : fallback;
}
PlaybackSettings Sanitize(PlaybackSettings s)
{
    s.volume = Clamp(s.volume, 0, 1, 1);
    s.pitch = Clamp(s.pitch, .01f, 3, 1);
    s.spatialBlend = Clamp(s.spatialBlend, 0, 1, 0);
    s.minDistance = Clamp(s.minDistance, .01f, 100000, 1);
    s.maxDistance = Clamp(s.maxDistance, s.minDistance + .01f, 100001, s.minDistance + 30);
    s.rolloffMode = std::clamp(s.rolloffMode, 0, 2);
    s.loops = std::max(-1, s.loops);
    if (!std::isfinite(s.position.x) || !std::isfinite(s.position.y) || !std::isfinite(s.position.z))
        s.position = Vector3(0);
    return s;
}
void MixLevels(AudioState &s)
{
    const auto &c = GetProjectConfig();
    s.master = Clamp(c.volume, 0, 1, 1);
    s.music = Clamp(c.musicVolume, 0, 1, 1);
    s.sfx = Clamp(c.sfxVolume, 0, 1, 1);
    s.muted = c.mute;
}
void Release(Voice &v)
{
#if CANIS_STEAM_AUDIO
    if (v.effect)
        iplBinauralEffectRelease(&v.effect);
#endif
    v.samples.reset();
    v.active = false;
    v.id = -1;
}
bool CreateEffect(AudioState &s, Voice &v)
{
#if CANIS_STEAM_AUDIO
    if (s.hrtf && v.settings.spatialBlend > 0 && !v.effect)
    {
        IPLAudioSettings audio{48000, BlockSize};
        IPLBinauralEffectSettings effect{s.hrtf};
        if (iplBinauralEffectCreate(s.context, &audio, &effect, &v.effect) != IPL_STATUS_SUCCESS)
            return false;
    }
#endif
    return true;
}
bool InitializeProcessor(AudioState &s)
{
    MixLevels(s);
#if CANIS_STEAM_AUDIO
    IPLContextSettings context{};
    context.version = STEAMAUDIO_VERSION;
    context.simdLevel = IPL_SIMDLEVEL_SSE2;
    if (iplContextCreate(&context, &s.context) != IPL_STATUS_SUCCESS)
        return false;
    IPLAudioSettings audio{48000, BlockSize};
    IPLHRTFSettings hrtf{};
    hrtf.type = IPL_HRTFTYPE_DEFAULT;
    hrtf.volume = 1;
    hrtf.normType = IPL_HRTFNORMTYPE_RMS;
    if (iplHRTFCreate(s.context, &audio, &hrtf, &s.hrtf) != IPL_STATUS_SUCCESS)
    {
        iplContextRelease(&s.context);
        return false;
    }
#endif
    s.initialized = true;
    return true;
}
Voice *Find(AudioState &s, PlaybackHandle h)
{
    for (auto &v : s.voices)
        if (v.active && v.id == h.id)
            return &v;
    return nullptr;
}

// Called with the state lock held. All buffers/effects are created on the main thread.
// Finished voices keep their resources until reused/stopped, avoiding callback deallocation.
void MixBlock(AudioState &s, float *output)
{
    std::fill_n(output, BlockSize * 2, 0.f);
    for (auto &v : s.voices)
    {
        if (!v.active || v.paused || (v.settings.sceneOwned && s.scenePaused))
            continue;
        const auto &p = v.settings;
        v.dry.fill(0);
        v.mono.fill(0);
        const size_t frames = v.samples->size() / 2;
        bool hadInput = false;
        for (int i = 0; i < BlockSize && !v.inputEnded; ++i)
        {
            while (v.cursor >= frames)
            {
                if (v.loops == 0)
                {
                    v.inputEnded = true;
                    break;
                }
                if (v.loops > 0)
                    --v.loops;
                v.cursor -= frames;
            }
            if (v.inputEnded)
                break;
            const auto a = static_cast<size_t>(v.cursor);
            const auto b = a + 1 < frames ? a + 1 : (v.loops != 0 ? 0 : a);
            const float fraction = static_cast<float>(v.cursor - a);
            for (int c = 0; c < 2; ++c)
                v.dry[2 * i + c] = (*v.samples)[2 * a + c] * (1 - fraction) + (*v.samples)[2 * b + c] * fraction;
            v.mono[i] = .5f * (v.dry[2 * i] + v.dry[2 * i + 1]);
            v.cursor += p.pitch;
            hadInput = true;
        }
        const auto delta = p.position - s.listenerPosition;
        const float distance = glm::length(delta);
        const auto direction =
            distance > .0001f ? glm::inverse(s.listenerRotation) * (delta / distance) : Vector3(0, 0, -1);
        const float attenuation =
            s.listenerActive ? DistanceGain(distance, p.minDistance, p.maxDistance, p.rolloffMode) : 0;
        bool tail = false;
#if CANIS_STEAM_AUDIO
        if (v.effect && p.spatialBlend > 0)
        {
            float *inChannels[]{v.mono.data()};
            float *outChannels[]{v.left.data(), v.right.data()};
            IPLAudioBuffer in{1, BlockSize, inChannels}, out{2, BlockSize, outChannels};
            if (hadInput)
            {
                IPLBinauralEffectParams params{};
                params.direction = {direction.x, direction.y, direction.z};
                params.interpolation = IPL_HRTFINTERPOLATION_BILINEAR;
                params.spatialBlend = 1;
                params.hrtf = s.hrtf;
                tail = iplBinauralEffectApply(v.effect, &params, &in, &out) == IPL_AUDIOEFFECTSTATE_TAILREMAINING;
            }
            else
                tail = iplBinauralEffectGetTail(v.effect, &out) == IPL_AUDIOEFFECTSTATE_TAILREMAINING;
        }
        else
#endif
        {
            const float pan = std::clamp(direction.x, -1.f, 1.f);
            const float l = std::sqrt(.5f * (1 - pan)), r = std::sqrt(.5f * (1 + pan));
            for (int i = 0; i < BlockSize; ++i)
            {
                v.left[i] = v.mono[i] * l;
                v.right[i] = v.mono[i] * r;
            }
        }
        const float gain = (s.muted ? 0 : s.master) * (p.bus == Bus::Music ? s.music : s.sfx) * p.volume;
        const float listener = p.sceneOwned ? (s.listenerActive ? s.listenerVolume : 0) : 1;
        for (int i = 0; i < BlockSize; ++i)
        {
            output[2 * i] +=
                gain * listener * ((1 - p.spatialBlend) * v.dry[2 * i] + p.spatialBlend * attenuation * v.left[i]);
            output[2 * i + 1] +=
                gain * listener * ((1 - p.spatialBlend) * v.dry[2 * i + 1] + p.spatialBlend * attenuation * v.right[i]);
        }
        if (v.inputEnded && !tail)
            v.active = false;
    }
    for (int i = 0; i < BlockSize * 2; ++i)
        output[i] = std::clamp(output[i], -1.f, 1.f);
}
void SDLCALL Callback(void *data, SDL_AudioStream *stream, int additional, int)
{
    auto &s = *static_cast<AudioState *>(data);
    std::array<float, BlockSize * 2> output;
    while (additional > 0)
    {
        {
            std::lock_guard lock(s.mutex);
            MixBlock(s, output.data());
        }
        if (!SDL_PutAudioStreamData(stream, output.data(), sizeof(output)))
            break;
        additional -= sizeof(output);
    }
}
} // namespace
const SDL_AudioSpec &GetMixSpec()
{
    static const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 48000};
    return spec;
}
const char *SpatialBackend()
{
#if CANIS_STEAM_AUDIO
    return "Steam Audio 4.8.1 HRTF";
#else
    return "Stereo panning (Steam Audio disabled)";
#endif
}
bool InitializeOffline()
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    if (s.initialized)
        return s.offline;
    s.offline = true;
    return InitializeProcessor(s);
}
bool Initialize()
{
    auto &s = State();
    if (s.initialized)
        return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        return false;
    s.ownsSDL = true;
    if (!InitializeProcessor(s))
    {
        Shutdown();
        Debug::Warning("Spatial audio initialization failed");
        return false;
    }
    s.offline = false;
    s.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &GetMixSpec(), Callback, &s);
    if (!s.stream || !SDL_ResumeAudioStreamDevice(s.stream))
    {
        Debug::Warning("Audio device unavailable: %s", SDL_GetError());
        Shutdown();
        return false;
    }
    Debug::Log("Audio backend: %s", SpatialBackend());
    return true;
}
void Shutdown()
{
    auto &s = State();
    // Destroy joins the callback; never wait for it while holding the mixer mutex.
    if (s.stream)
    {
        SDL_DestroyAudioStream(s.stream);
        s.stream = nullptr;
    }
    std::lock_guard lock(s.mutex);
    for (auto &v : s.voices)
        Release(v);
#if CANIS_STEAM_AUDIO
    if (s.hrtf)
        iplHRTFRelease(&s.hrtf);
    if (s.context)
        iplContextRelease(&s.context);
#endif
    s.initialized = false;
    s.offline = false;
    s.scenePaused = false;
    s.listenerActive = false;
    if (s.ownsSDL)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s.ownsSDL = false;
    }
}
bool IsInitialized()
{
    return State().initialized;
}
bool RenderOffline(float *stereo, int frames)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    if (!s.initialized || !s.offline || !stereo || frames < 0 || frames % BlockSize != 0)
        return false;
    for (int i = 0; i < frames; i += BlockSize)
        MixBlock(s, stereo + 2 * i);
    return true;
}
float DistanceGain(float distance, float minimum, float maximum, int mode)
{
    if (!std::isfinite(distance))
        return 0;
    minimum = std::max(.01f, minimum);
    maximum = std::max(minimum + .01f, maximum);
    if (mode == 2 || distance <= minimum)
        return 1;
    distance = std::min(distance, maximum);
    return mode == 1 ? 1 - (distance - minimum) / (maximum - minimum) : minimum / distance;
}
PlaybackHandle Play(AudioClipAsset &clip, const PlaybackSettings &settings)
{
    if (!clip.IsLoaded() || !Initialize())
        return {};
    auto &s = State();
    std::lock_guard lock(s.mutex);
    for (auto &v : s.voices)
        if (!v.active)
        {
            Release(v);
            if (s.nextId == std::numeric_limits<int>::max())
                return {};
            v.id = s.nextId++;
            v.samples = clip.GetSampleData();
            v.settings = Sanitize(settings);
            v.loops = v.settings.loops;
            v.cursor = 0;
            v.paused = false;
            v.inputEnded = false;
            if (!CreateEffect(s, v))
            {
                Release(v);
                return {};
            }
            v.active = true;
            return {v.id};
        }
    return {}; // Bounded voice pool: no allocations or unbounded CPU work in the callback.
}
bool Configure(PlaybackHandle handle, const PlaybackSettings &settings)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    auto *v = Find(s, handle);
    if (!v)
        return false;
    const auto updated = Sanitize(settings);
    if (v->settings.loops != updated.loops)
        v->loops = updated.loops;
#if CANIS_STEAM_AUDIO
    if (v->effect && v->settings.spatialBlend <= 0 && updated.spatialBlend > 0)
        iplBinauralEffectReset(v->effect);
#endif
    v->settings = updated;
    return CreateEffect(s, *v);
}
bool SetVolume(PlaybackHandle handle, float volume)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    auto *v = Find(s, handle);
    if (!v)
        return false;
    v->settings.volume = Clamp(volume, 0, 1, 1);
    return true;
}
bool IsPlaying(PlaybackHandle handle)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    auto *v = Find(s, handle);
    return v && !v->paused && !(v->settings.sceneOwned && s.scenePaused);
}
bool IsAlive(PlaybackHandle handle)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    return Find(s, handle) != nullptr;
}
void Pause(PlaybackHandle handle, bool paused)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    if (auto *v = Find(s, handle))
        v->paused = paused;
}
void Stop(PlaybackHandle handle)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    for (auto &v : s.voices)
        if (v.id == handle.id)
            Release(v);
}
void StopBus(Bus bus)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    for (auto &v : s.voices)
        if (v.settings.bus == bus)
            Release(v);
}
void StopAll()
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    for (auto &v : s.voices)
        Release(v);
}
void RefreshMixerFromProjectConfig()
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    MixLevels(s);
}
void SetMuted(bool muted)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    s.muted = muted;
}
void SetScenePaused(bool paused)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    s.scenePaused = paused;
}
void SetListener(Vector3 position, Quaternion orientation, float volume, bool active)
{
    auto &s = State();
    std::lock_guard lock(s.mutex);
    s.listenerPosition =
        std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z) ? position : Vector3(0);
    const auto length = glm::length(orientation);
    s.listenerRotation =
        std::isfinite(length) && length > .00001f ? glm::normalize(orientation) : Quaternion(1, 0, 0, 0);
    s.listenerVolume = Clamp(volume, 0, 1, 1);
    s.listenerActive = active;
}
} // namespace Canis::Audio
