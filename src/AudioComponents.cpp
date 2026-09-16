#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Audio.hpp>
#include <Canis/AudioComponents.hpp>
#include <Canis/Components.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Debug.hpp>
#include <Canis/VR/VRSystem.hpp>
#include <algorithm>
#include <cmath>

namespace Canis
{
struct AudioSourceState
{
    struct Sound
    {
        Audio::PlaybackHandle handle;
        float scale;
        bool oneShot;
    };
    std::vector<Sound> sounds;
    bool awakened = false;
    ~AudioSourceState()
    {
        for (auto &sound : sounds)
            Audio::Stop(sound.handle);
    }
};
namespace
{
ScriptConf listenerConf, sourceConf;
bool Active(Entity *e)
{
    if (!e || !e->IsValid() || !e->IsActive())
        return false;
    auto *t = e->TryGetComponent<Transform>();
    return t && t->IsActiveInHierarchy();
}
std::string Path(const AudioAssetHandle &clip)
{
    if (clip.uuid != UUID(0))
    {
        auto path = AssetManager::GetPath(clip.uuid);
        if (path != "Path was not found in AssetLibrary")
            return path;
    }
    return clip.path;
}
Audio::PlaybackSettings Settings(AudioSource &source, float scale, bool oneShot)
{
    Audio::PlaybackSettings p;
    p.volume = source.mute ? 0 : source.volume * scale;
    p.loops = source.loop && !oneShot ? -1 : 0;
    p.pitch = source.pitch;
    p.spatialBlend = source.spatialBlend;
    p.minDistance = source.minDistance;
    p.maxDistance = source.maxDistance;
    p.rolloffMode = source.rolloffMode;
    p.sceneOwned = true;
    if (source.entity)
        if (auto *t = source.entity->TryGetComponent<Transform>())
            p.position = t->GetGlobalPosition();
    return p;
}
} // namespace
AudioSource::AudioSource() = default;
AudioSource::~AudioSource() = default;
AudioSource::AudioSource(AudioSource &&) noexcept = default;
AudioSource &AudioSource::operator=(AudioSource &&) noexcept = default;
bool AudioSource::Play()
{
    if (!enabled || !Active(entity))
        return false;
    if (!m_state)
        m_state = std::make_unique<AudioSourceState>();
    m_state->awakened = true;
    for (auto &sound : m_state->sounds)
        if (!sound.oneShot)
            Audio::Stop(sound.handle);
    std::erase_if(m_state->sounds, [](auto &sound) { return !Audio::IsAlive(sound.handle); });
    auto path = Path(clip);
    if (path.empty())
        return false;
    auto *asset = AssetManager::GetSound(path);
    if (!asset || !asset->IsLoaded())
        return false;
    auto handle = Audio::Play(*asset, Settings(*this, 1, false));
    if (!handle.IsValid())
        return false;
    m_state->sounds.push_back({handle, 1, false});
    return true;
}
bool AudioSource::PlayOneShot(const AudioAssetHandle &clip, float volumeScale)
{
    if (!enabled || !Active(entity) || !std::isfinite(volumeScale))
        return false;
    if (!m_state)
        m_state = std::make_unique<AudioSourceState>();
    auto path = Path(clip);
    if (path.empty())
        return false;
    auto *asset = AssetManager::GetSound(path);
    if (!asset || !asset->IsLoaded())
        return false;
    volumeScale = std::clamp(volumeScale, 0.f, 1.f);
    auto handle = Audio::Play(*asset, Settings(*this, volumeScale, true));
    if (!handle.IsValid())
        return false;
    std::erase_if(m_state->sounds, [](auto &sound) { return !Audio::IsAlive(sound.handle); });
    m_state->sounds.push_back({handle, volumeScale, true});
    return true;
}
void AudioSource::Stop()
{
    if (m_state)
    {
        for (auto &sound : m_state->sounds)
            Audio::Stop(sound.handle);
        m_state->sounds.clear();
        m_state->awakened = true;
    }
}
void AudioSource::Pause()
{
    if (m_state)
        for (auto &sound : m_state->sounds)
            Audio::Pause(sound.handle, true);
}
void AudioSource::UnPause()
{
    if (m_state)
        for (auto &sound : m_state->sounds)
            Audio::Pause(sound.handle, false);
}
bool AudioSource::IsPlaying() const
{
    if (m_state)
        for (auto &sound : m_state->sounds)
            if (Audio::IsPlaying(sound.handle))
                return true;
    return false;
}
void AudioSource::Update(bool active, bool paused)
{
    if (!m_state)
        m_state = std::make_unique<AudioSourceState>();
    if (!active || !enabled)
    {
        Stop();
        m_state->awakened = false;
        return;
    }
    if (!paused && !m_state->awakened)
    {
        m_state->awakened = true;
        if (playOnAwake)
            Play();
    }
    std::erase_if(m_state->sounds, [](auto &sound) { return !Audio::IsAlive(sound.handle); });
    for (auto &sound : m_state->sounds)
        Audio::Configure(sound.handle, Settings(*this, sound.scale, sound.oneShot));
}
void UpdateSceneAudio(Scene &scene)
{
    AudioListener *selected = nullptr;
    uint64_t selectedID = UINT64_MAX;
    for (auto handle : scene.GetRegistry().view<AudioListener>())
    {
        auto &listener = scene.GetRegistry().get<AudioListener>(handle);
        if (!listener.enabled || !Active(listener.entity))
            continue;
        const auto id = static_cast<uint64_t>(listener.entity->GetUUID());
        if (!selected || id < selectedID)
        {
            selected = &listener;
            selectedID = id;
        }
    }
    Vector3 position(0);
    Quaternion rotation(1, 0, 0, 0);
    if (selected)
    {
        auto &transform = selected->entity->GetComponent<Transform>();
        position = transform.GetGlobalPosition();
        rotation = transform.GetGlobalRotation();
        if (selected->followHeadset && scene.app && scene.app->GetVR())
        {
            auto &vr = *scene.app->GetVR();
            if (vr.GetState().head.valid)
            {
                auto head = vr.WorldPose(vr.GetState().head);
                position = head.position;
                rotation = head.orientation;
            }
        }
    }
    Audio::SetListener(position, rotation, selected ? selected->volume : 1, selected != nullptr);
    Audio::SetScenePaused(scene.IsPaused());
    for (auto handle : scene.GetRegistry().view<AudioSource>())
    {
        auto &source = scene.GetRegistry().get<AudioSource>(handle);
        source.Update(Active(source.entity), scene.IsPaused());
    }
}
void RegisterAudioComponents(App &app)
{
    // Rebuild the static property registries so repeated headless App initialization is safe.
    listenerConf = {};
    sourceConf = {};
    REGISTER_PROPERTY(listenerConf, AudioListener, enabled);
    REGISTER_PROPERTY(listenerConf, AudioListener, volume);
    REGISTER_PROPERTY(listenerConf, AudioListener, followHeadset);
    DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(listenerConf, AudioListener, Transform);
    listenerConf.DrawInspector = [](Editor &editor, Entity &entity, const ScriptConf &conf) {
        DrawRegisteredProperties(editor, conf.registry, &entity.GetComponent<AudioListener>(), conf.name);
    };
    app.RegisterComponent(listenerConf);
    REGISTER_PROPERTY(sourceConf, AudioSource, enabled);
    REGISTER_PROPERTY(sourceConf, AudioSource, clip);
    REGISTER_PROPERTY(sourceConf, AudioSource, playOnAwake);
    REGISTER_PROPERTY(sourceConf, AudioSource, loop);
    REGISTER_PROPERTY(sourceConf, AudioSource, mute);
    REGISTER_PROPERTY(sourceConf, AudioSource, volume);
    REGISTER_PROPERTY(sourceConf, AudioSource, pitch);
    REGISTER_PROPERTY(sourceConf, AudioSource, spatialBlend);
    REGISTER_PROPERTY(sourceConf, AudioSource, minDistance);
    REGISTER_PROPERTY(sourceConf, AudioSource, maxDistance);
    REGISTER_PROPERTY(sourceConf, AudioSource, rolloffMode);
    DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(sourceConf, AudioSource, Transform);
    sourceConf.DrawInspector = [](Editor &editor, Entity &entity, const ScriptConf &conf) {
        DrawRegisteredProperties(editor, conf.registry, &entity.GetComponent<AudioSource>(), conf.name);
    };
    app.RegisterComponent(sourceConf);
}
} // namespace Canis
