#include <Canis/AudioManager.hpp>

#include <Canis/AssetManager.hpp>
#include <Canis/Audio.hpp>
#include <Canis/Canis.hpp>

#include <algorithm>
#include <cmath>

namespace Canis::AudioManager
{
    namespace
    {
        struct AudioManagerData
        {
            int musicHandle = -1;
        };

        AudioManagerData& GetAudioManagerData()
        {
            static AudioManagerData data = {};
            return data;
        }

        float ClampVolume(const float _volume)
        {
            if (!std::isfinite(_volume))
                return 1.0f;

            return std::clamp(_volume, 0.0f, 1.0f);
        }

        std::string ResolveAudioPath(const AudioAssetHandle &_handle)
        {
            if (_handle.uuid != UUID(0))
            {
                const std::string resolvedPath = AssetManager::GetPath(_handle.uuid);
                if (resolvedPath != "Path was not found in AssetLibrary")
                    return resolvedPath;
            }

            return _handle.path;
        }
    } // namespace

    bool Initialize()
    {
        return Audio::Initialize();
    }

    void Shutdown()
    {
        GetAudioManagerData().musicHandle = -1;
        Audio::StopAll();
        Audio::Shutdown();
    }

    bool IsInitialized()
    {
        return Audio::IsInitialized();
    }

    void RefreshMixLevels()
    {
        Audio::RefreshMixerFromProjectConfig();
    }

    void UpdateMusicVolume()
    {
        RefreshMixLevels();
    }

    void Mute()
    {
        GetProjectConfig().mute = true;
        Audio::SetMuted(true);
    }

    void UnMute()
    {
        GetProjectConfig().mute = false;
        Audio::RefreshMixerFromProjectConfig();
    }

    void PlayMusic(const std::string &_path)
    {
        PlayMusic(_path, 0, 1.0f);
    }

    void PlayMusic(const std::string &_path, int _loops)
    {
        PlayMusic(_path, _loops, 1.0f);
    }

    void PlayMusic(const std::string &_path, float _volume)
    {
        PlayMusic(_path, 0, _volume);
    }

    void PlayMusic(const AudioAssetHandle &_handle)
    {
        PlayMusic(_handle, 0, 1.0f);
    }

    void PlayMusic(const AudioAssetHandle &_handle, int _loops)
    {
        PlayMusic(_handle, _loops, 1.0f);
    }

    void PlayMusic(const AudioAssetHandle &_handle, float _volume)
    {
        PlayMusic(_handle, 0, _volume);
    }

    void PlayMusic(const AudioAssetHandle &_handle, int _loops, float _volume)
    {
        const std::string path = ResolveAudioPath(_handle);
        if (!path.empty())
            PlayMusic(path, _loops, _volume);
    }

    void PlayMusic(const std::string &_path, int _loops, float _volume)
    {
        if (!Initialize())
            return;

        MusicAsset* asset = AssetManager::GetMusic(_path);
        if (asset == nullptr || !asset->IsLoaded())
            return;

        StopMusic();

        const Audio::PlaybackHandle handle = Audio::Play(*asset, {
            .volume = ClampVolume(_volume),
            .loops = std::max(-1, _loops),
            .bus = Audio::Bus::Music,
        });

        GetAudioManagerData().musicHandle = handle.id;
    }

    void StopMusic()
    {
        AudioManagerData& data = GetAudioManagerData();
        if (data.musicHandle < 0)
            return;

        Audio::Stop({ data.musicHandle });
        data.musicHandle = -1;
    }

    int Play(const std::string &_path)
    {
        return Play(_path, 1.0f, false);
    }

    int Play(const std::string &_path, float _volume)
    {
        return Play(_path, _volume, false);
    }

    int Play(const AudioAssetHandle &_handle)
    {
        return Play(_handle, 1.0f, false);
    }

    int Play(const AudioAssetHandle &_handle, float _volume)
    {
        return Play(_handle, _volume, false);
    }

    int Play(const AudioAssetHandle &_handle, float _volume, bool _loop)
    {
        const std::string path = ResolveAudioPath(_handle);
        return path.empty() ? -1 : Play(path, _volume, _loop);
    }

    int Play(const std::string &_path, float _volume, bool _loop)
    {
        if (!Initialize())
            return -1;

        SoundAsset* asset = AssetManager::GetSound(_path);
        if (asset == nullptr || !asset->IsLoaded())
            return -1;

        const Audio::PlaybackHandle handle = Audio::Play(*asset, {
            .volume = ClampVolume(_volume),
            .loops = _loop ? -1 : 0,
            .bus = Audio::Bus::SFX,
        });

        return handle.id;
    }

    int PlaySFX(const std::string &_path)
    {
        return Play(_path);
    }

    int PlaySFX(const std::string &_path, float _volume)
    {
        return Play(_path, _volume);
    }

    int PlaySFX(const std::string &_path, float _volume, bool _loop)
    {
        return Play(_path, _volume, _loop);
    }

    int PlaySFX(const AudioAssetHandle &_handle)
    {
        return Play(_handle);
    }

    int PlaySFX(const AudioAssetHandle &_handle, float _volume)
    {
        return Play(_handle, _volume);
    }

    int PlaySFX(const AudioAssetHandle &_handle, float _volume, bool _loop)
    {
        return Play(_handle, _volume, _loop);
    }

    void SetVolume(int _channel, float _volume)
    {
        Audio::SetVolume({ _channel }, ClampVolume(_volume));
    }

    void StopSound(int _channel)
    {
        AudioManagerData& data = GetAudioManagerData();
        if (_channel == data.musicHandle)
            data.musicHandle = -1;

        Audio::Stop({ _channel });
    }

    void StopAllSounds()
    {
        Audio::StopBus(Audio::Bus::SFX);
    }
}
