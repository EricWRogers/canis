#pragma once

#include <Canis/AssetHandle.hpp>

#include <string>

namespace Canis::AudioManager
{
    bool Initialize();
    void Shutdown();
    bool IsInitialized();

    void RefreshMixLevels();
    void UpdateMusicVolume();

    void Mute();
    void UnMute();

    void PlayMusic(const std::string &_path);
    void PlayMusic(const std::string &_path, int _loops);
    void PlayMusic(const std::string &_path, float _volume);
    void PlayMusic(const std::string &_path, int _loops, float _volume);
    void PlayMusic(const AudioAssetHandle &_handle);
    void PlayMusic(const AudioAssetHandle &_handle, int _loops);
    void PlayMusic(const AudioAssetHandle &_handle, float _volume);
    void PlayMusic(const AudioAssetHandle &_handle, int _loops, float _volume);
    void StopMusic();

    int Play(const std::string &_path);
    int Play(const std::string &_path, float _volume);
    int Play(const std::string &_path, float _volume, bool _loop);
    int Play(const AudioAssetHandle &_handle);
    int Play(const AudioAssetHandle &_handle, float _volume);
    int Play(const AudioAssetHandle &_handle, float _volume, bool _loop);

    int PlaySFX(const std::string &_path);
    int PlaySFX(const std::string &_path, float _volume);
    int PlaySFX(const std::string &_path, float _volume, bool _loop);
    int PlaySFX(const AudioAssetHandle &_handle);
    int PlaySFX(const AudioAssetHandle &_handle, float _volume);
    int PlaySFX(const AudioAssetHandle &_handle, float _volume, bool _loop);

    void SetVolume(int _channel, float _volume);
    void StopSound(int _channel);
    void StopAllSounds();
}
