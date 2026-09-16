#pragma once

#include <SDL3/SDL_audio.h>
#include <Canis/Math.hpp>

namespace Canis
{
    class AudioClipAsset;

    namespace Audio
    {
        enum class Bus : int
        {
            Music = 0,
            SFX = 1,
        };

        struct PlaybackHandle
        {
            int id = -1;
            bool IsValid() const { return id >= 0; }
        };

        struct PlaybackSettings
        {
            float volume = 1.0f;
            int loops = 0;
            Bus bus = Bus::SFX;
            float pitch = 1.0f;
            float spatialBlend = 0.0f;
            Vector3 position = Vector3(0);
            float minDistance = 1.0f;
            float maxDistance = 30.0f;
            int rolloffMode = 0; // 0 inverse distance, 1 linear, 2 no attenuation
            bool sceneOwned = false;
        };

        const SDL_AudioSpec& GetMixSpec();

        bool Initialize();
        void Shutdown();
        bool IsInitialized();

        PlaybackHandle Play(AudioClipAsset &_clip, const PlaybackSettings &_settings = {});
        bool SetVolume(PlaybackHandle _handle, float _volume);
        bool Configure(PlaybackHandle handle, const PlaybackSettings& settings);
        bool IsPlaying(PlaybackHandle handle);
        bool IsAlive(PlaybackHandle handle);
        void Pause(PlaybackHandle handle, bool paused);
        void SetListener(Vector3 position, Quaternion orientation, float volume, bool active);
        void SetScenePaused(bool paused);
        const char* SpatialBackend();
        // Deterministic device-free rendering, also useful for export tools.
        bool InitializeOffline();
        bool RenderOffline(float* stereo, int frames); // frames must be a multiple of 512
        float DistanceGain(float distance, float minimum, float maximum, int mode);
        void Stop(PlaybackHandle _handle);
        void StopBus(Bus _bus);
        void StopAll();

        void RefreshMixerFromProjectConfig();
        void SetMuted(bool _muted);
    }
}
