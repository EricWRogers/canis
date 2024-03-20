#pragma once
#include <string>

namespace Canis
{
    namespace AudioManager
    {
        void PlayMusic(const std::string &_path);
        void PlayMusic(const std::string &_path, int _loops);
        void PlayMusic(const std::string &_path, float _volume);
        void PlayMusic(const std::string &_path, int _loops, float _volume);

        void StopMusic();

        void Mute();
        void UnMute();

        void Play(const std::string &_path);
        void Play(const std::string &_path, float _volume);
        int Play(const std::string &_path, float _volume, bool _loop);

        void SetVolume(int _channel, float _volume);

        void StopSound(int _channel);
        void StopAllSounds();
    }
}