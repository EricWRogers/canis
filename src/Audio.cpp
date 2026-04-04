#include <Canis/Audio.hpp>

#include <Canis/Asset.hpp>
#include <Canis/Canis.hpp>
#include <Canis/Debug.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Canis::Audio
{
    namespace
    {
        struct Voice
        {
            int id = -1;
            AudioClipAsset* clip = nullptr;
            Bus bus = Bus::SFX;
            float volume = 1.0f;
            int loops = 0;
            int frameCursor = 0;
            bool active = false;
        };

        struct AudioState
        {
            SDL_AudioStream* stream = nullptr;
            std::vector<Voice> voices = {};
            int nextVoiceId = 1;
            float masterVolume = 1.0f;
            float musicVolume = 1.0f;
            float sfxVolume = 1.0f;
            bool muted = false;
        };

        AudioState& GetAudioState()
        {
            static AudioState state = {};
            return state;
        }

        float ClampVolume(const float _volume)
        {
            if (!std::isfinite(_volume))
                return 1.0f;

            return std::clamp(_volume, 0.0f, 1.0f);
        }

        void SyncMixerFromProjectConfig(AudioState& _state)
        {
            const ProjectConfig& config = GetProjectConfig();
            _state.masterVolume = ClampVolume(config.volume);
            _state.musicVolume = ClampVolume(config.musicVolume);
            _state.sfxVolume = ClampVolume(config.sfxVolume);
            _state.muted = config.mute;
        }

        float GetBusVolume(const AudioState& _state, const Bus _bus)
        {
            return (_bus == Bus::Music) ? _state.musicVolume : _state.sfxVolume;
        }

        void SDLCALL AudioStreamCallback(void* _userdata, SDL_AudioStream* _stream, int _additionalAmount, int _totalAmount)
        {
            (void)_totalAmount;

            AudioState& state = *static_cast<AudioState*>(_userdata);

            if (_stream == nullptr || _additionalAmount <= 0)
                return;

            const SDL_AudioSpec& mixSpec = GetMixSpec();
            const int channels = mixSpec.channels;
            const int sampleCount = _additionalAmount / static_cast<int>(sizeof(float));
            const int frameCount = channels > 0 ? sampleCount / channels : 0;
            if (frameCount <= 0)
                return;

            std::vector<float> mixBuffer(static_cast<size_t>(sampleCount), 0.0f);
            const float masterVolume = state.muted ? 0.0f : state.masterVolume;

            for (Voice& voice : state.voices)
            {
                if (!voice.active || voice.clip == nullptr || !voice.clip->IsLoaded())
                {
                    voice.active = false;
                    continue;
                }

                const float finalVolume = masterVolume * GetBusVolume(state, voice.bus) * voice.volume;
                const float* samples = voice.clip->GetSamples();
                const int clipFrameCount = voice.clip->GetFrameCount();
                if (samples == nullptr || clipFrameCount <= 0)
                {
                    voice.active = false;
                    continue;
                }

                for (int frameIndex = 0; frameIndex < frameCount; ++frameIndex)
                {
                    if (voice.frameCursor >= clipFrameCount)
                    {
                        if (voice.loops == -1)
                        {
                            voice.frameCursor = 0;
                        }
                        else if (voice.loops > 0)
                        {
                            voice.loops--;
                            voice.frameCursor = 0;
                        }
                        else
                        {
                            voice.active = false;
                            break;
                        }
                    }

                    const int mixOffset = frameIndex * channels;
                    const int clipOffset = voice.frameCursor * channels;
                    for (int channel = 0; channel < channels; ++channel)
                        mixBuffer[static_cast<size_t>(mixOffset + channel)] += samples[clipOffset + channel] * finalVolume;

                    voice.frameCursor++;
                }
            }

            std::erase_if(state.voices, [](const Voice& _voice)
            {
                return !_voice.active;
            });

            for (float& sample : mixBuffer)
                sample = std::clamp(sample, -1.0f, 1.0f);

            if (!SDL_PutAudioStreamData(_stream, mixBuffer.data(), static_cast<int>(mixBuffer.size() * sizeof(float))))
                Debug::Warning("Failed to queue mixed audio: %s", SDL_GetError());
        }
    } // namespace

    const SDL_AudioSpec& GetMixSpec()
    {
        static SDL_AudioSpec spec = { SDL_AUDIO_F32, 2, 48000 };
        return spec;
    }

    bool Initialize()
    {
        AudioState& state = GetAudioState();
        if (state.stream != nullptr)
            return true;

        SyncMixerFromProjectConfig(state);

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            Debug::Warning("SDL audio init failed: %s", SDL_GetError());
            return false;
        }

        state.stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
            &GetMixSpec(),
            AudioStreamCallback,
            &state);

        if (state.stream == nullptr)
        {
            Debug::Warning("Failed to open audio device stream: %s", SDL_GetError());
            return false;
        }

        if (!SDL_ResumeAudioStreamDevice(state.stream))
        {
            Debug::Warning("Failed to resume audio device stream: %s", SDL_GetError());
            SDL_DestroyAudioStream(state.stream);
            state.stream = nullptr;
            return false;
        }

        return true;
    }

    void Shutdown()
    {
        AudioState& state = GetAudioState();

        if (state.stream != nullptr)
        {
            SDL_LockAudioStream(state.stream);
            state.voices.clear();
            SDL_UnlockAudioStream(state.stream);
            SDL_DestroyAudioStream(state.stream);
            state.stream = nullptr;
        }
        else
        {
            state.voices.clear();
        }

        state.nextVoiceId = 1;

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    bool IsInitialized()
    {
        return GetAudioState().stream != nullptr;
    }

    PlaybackHandle Play(AudioClipAsset &_clip, const PlaybackSettings &_settings)
    {
        AudioState& state = GetAudioState();
        if (!_clip.IsLoaded())
            return {};

        if (state.stream == nullptr && !Initialize())
            return {};

        Voice voice = {};
        voice.id = state.nextVoiceId++;
        voice.clip = &_clip;
        voice.bus = _settings.bus;
        voice.volume = ClampVolume(_settings.volume);
        voice.loops = std::max(-1, _settings.loops);
        voice.frameCursor = 0;
        voice.active = true;

        SDL_LockAudioStream(state.stream);
        state.voices.push_back(voice);
        SDL_UnlockAudioStream(state.stream);

        return { voice.id };
    }

    bool SetVolume(PlaybackHandle _handle, float _volume)
    {
        AudioState& state = GetAudioState();
        if (state.stream == nullptr || !_handle.IsValid())
            return false;

        const float clampedVolume = ClampVolume(_volume);

        SDL_LockAudioStream(state.stream);
        for (Voice& voice : state.voices)
        {
            if (voice.id == _handle.id)
            {
                voice.volume = clampedVolume;
                SDL_UnlockAudioStream(state.stream);
                return true;
            }
        }
        SDL_UnlockAudioStream(state.stream);

        return false;
    }

    void Stop(PlaybackHandle _handle)
    {
        AudioState& state = GetAudioState();
        if (state.stream == nullptr || !_handle.IsValid())
            return;

        SDL_LockAudioStream(state.stream);
        std::erase_if(state.voices, [_handle](const Voice& _voice)
        {
            return _voice.id == _handle.id;
        });
        SDL_UnlockAudioStream(state.stream);
    }

    void StopBus(Bus _bus)
    {
        AudioState& state = GetAudioState();
        if (state.stream == nullptr)
            return;

        SDL_LockAudioStream(state.stream);
        std::erase_if(state.voices, [_bus](const Voice& _voice)
        {
            return _voice.bus == _bus;
        });
        SDL_UnlockAudioStream(state.stream);
    }

    void StopAll()
    {
        AudioState& state = GetAudioState();
        if (state.stream == nullptr)
        {
            state.voices.clear();
            return;
        }

        SDL_LockAudioStream(state.stream);
        state.voices.clear();
        SDL_UnlockAudioStream(state.stream);
    }

    void RefreshMixerFromProjectConfig()
    {
        AudioState& state = GetAudioState();
        if (state.stream == nullptr)
        {
            SyncMixerFromProjectConfig(state);
            return;
        }

        SDL_LockAudioStream(state.stream);
        SyncMixerFromProjectConfig(state);
        SDL_UnlockAudioStream(state.stream);
    }

    void SetMuted(const bool _muted)
    {
        AudioState& state = GetAudioState();

        if (state.stream == nullptr)
        {
            state.muted = _muted;
            return;
        }

        SDL_LockAudioStream(state.stream);
        state.muted = _muted;
        SDL_UnlockAudioStream(state.stream);
    }
}
