#pragma once
#include <Canis/AssetHandle.hpp>
#include <memory>
namespace Canis
{
class Entity;
class Scene;
class App;
struct AudioSourceState;
struct AudioListener
{
    static constexpr const char *ScriptName = "Canis::AudioListener";
    Entity *entity = nullptr;
    bool enabled = true;
    float volume = 1.0f;
    bool followHeadset = true;
};
struct AudioSource
{
    static constexpr const char *ScriptName = "Canis::AudioSource";
    static constexpr bool in_place_delete = true;
    Entity *entity = nullptr;
    bool enabled = true;
    AudioAssetHandle clip;
    bool playOnAwake = true;
    bool loop = false;
    bool mute = false;
    float volume = 1.0f;
    float pitch = 1.0f;
    float spatialBlend = 1.0f;
    float minDistance = 1.0f;
    float maxDistance = 30.0f;
    int rolloffMode = 0; // inverse distance, linear, no attenuation

    AudioSource();
    ~AudioSource();
    AudioSource(AudioSource &&) noexcept;
    AudioSource &operator=(AudioSource &&) noexcept;
    AudioSource(const AudioSource &) = delete;
    AudioSource &operator=(const AudioSource &) = delete;
    bool Play();
    bool PlayOneShot(const AudioAssetHandle &audio, float volumeScale = 1.0f);
    void Pause();
    void UnPause();
    void Stop();
    bool IsPlaying() const;
    void Update(bool active, bool paused);

  private:
    std::unique_ptr<AudioSourceState> m_state;
};
void RegisterAudioComponents(App &app);
void UpdateSceneAudio(Scene &scene);
} // namespace Canis
