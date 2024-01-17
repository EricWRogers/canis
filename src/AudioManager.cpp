#include <Canis/AudioManager.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Canis.hpp>

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
            Play(_path, 1.0f);
        }
        
        void Play(const std::string &_path, float _volume)
        {
            if (GetProjectConfig().mute)
                return;

            int id = AssetManager::LoadSound(_path);

            SoundAsset* asset = AssetManager::Get<SoundAsset>(id);

            asset->Play(_volume);
        }
        
    }
}