#pragma once

#include <SDL3/SDL_audio.h>

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
        };

        const SDL_AudioSpec& GetMixSpec();

        bool Initialize();
        void Shutdown();
        bool IsInitialized();

        PlaybackHandle Play(AudioClipAsset &_clip, const PlaybackSettings &_settings = {});
        bool SetVolume(PlaybackHandle _handle, float _volume);
        void Stop(PlaybackHandle _handle);
        void StopBus(Bus _bus);
        void StopAll();

        void RefreshMixerFromProjectConfig();
        void SetMuted(bool _muted);
    }
}
