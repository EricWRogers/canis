#include <Canis/AudioManager.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Canis.hpp>
#include <SDL_mixer.h>

namespace Canis
{
    namespace AudioManager
    {
        struct AudioManagerData
        {
            int musicId = -1;
        };

        AudioManagerData &GetAudioManagerData()
        {
            static AudioManagerData data = {};
            return data;
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
        
        void PlayMusic(const std::string &_path, int _loops, float _volume)
        {
            auto& audioManagerData = GetAudioManagerData();

            if (audioManagerData.musicId != -1)
            {
                
            }

            audioManagerData.musicId = AssetManager::LoadMusic(_path);

            MusicAsset* asset = AssetManager::Get<MusicAsset>(audioManagerData.musicId);

            asset->Play(_loops, _volume);
        }
        

        void StopMusic()
        {
            auto& audioManagerData = GetAudioManagerData();

            if (audioManagerData.musicId == -1)
                return;
            
            MusicAsset* asset = AssetManager::Get<MusicAsset>(audioManagerData.musicId);

            asset->Stop();
        }
        

        void Mute()
        {
            auto& audioManagerData = GetAudioManagerData();

            if (audioManagerData.musicId == -1)
                return;
            
            MusicAsset* asset = AssetManager::Get<MusicAsset>(audioManagerData.musicId);

            GetProjectConfig().mute = true;

            asset->Mute();
        }
        
        void UnMute()
        {
            auto& audioManagerData = GetAudioManagerData();

            if (audioManagerData.musicId == -1)
                return;
            
            MusicAsset* asset = AssetManager::Get<MusicAsset>(audioManagerData.musicId);

            GetProjectConfig().mute = false;

            asset->UnMute();
        }
        
        void Play(const std::string &_path)
        {
            int channel = Play(_path, 1.0f, false);
        }
        
        void Play(const std::string &_path, float _volume)
        {
            int channel = Play(_path, _volume, false);
        }

        int Play(const std::string &_path, float _volume, bool _loop)
        {
            if (GetProjectConfig().mute)
                return -1;

            int id = AssetManager::LoadSound(_path);

            SoundAsset* asset = AssetManager::Get<SoundAsset>(id);

            return asset->Play(_volume, _loop);
        }

        void StopSound(int _channel)
        {
            Mix_HaltChannel(_channel);
        }
        
        void StopAllSounds()
        {
            Mix_HaltChannel(-1);
        }
    }
}