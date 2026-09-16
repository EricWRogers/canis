#include <Canis/App.hpp>
#include <Canis/Asset.hpp>
#include <Canis/Audio.hpp>
#include <Canis/AudioComponents.hpp>
#include <Canis/Canis.hpp>
#include <Canis/Components.hpp>
#include <Canis/Editor.hpp>
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Canis;
namespace fs = std::filesystem;
void Check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}
void Number(std::ofstream &out, uint32_t value, int bytes)
{
    for (int i = 0; i < bytes; ++i)
        out.put(static_cast<char>(value >> (8 * i)));
}
void Wave(const fs::path &path)
{
    std::ofstream out(path, std::ios::binary);
    constexpr int frames = 8192, bytes = frames * 4;
    out.write("RIFF", 4);
    Number(out, 36 + bytes, 4);
    out.write("WAVEfmt ", 8);
    Number(out, 16, 4);
    Number(out, 1, 2);
    Number(out, 2, 2);
    Number(out, 48000, 4);
    Number(out, 192000, 4);
    Number(out, 4, 2);
    Number(out, 16, 2);
    out.write("data", 4);
    Number(out, bytes, 4);
    uint32_t random = 712367;
    for (int i = 0; i < frames; ++i)
    {
        random = random * 1664525 + 1013904223;
        int16_t sample = static_cast<int16_t>((random >> 18) - 8192);
        Number(out, static_cast<uint16_t>(sample), 2);
        Number(out, static_cast<uint16_t>(sample / 2), 2);
    }
}
using Buffer = std::array<float, 8192>;
Buffer Render()
{
    Buffer samples{};
    Check(Audio::RenderOffline(samples.data(), samples.size() / 2), "Offline render failed");
    for (float x : samples)
        Check(std::isfinite(x), "Nonfinite audio");
    return samples;
}
double Energy(const Buffer &b, int channel = -1)
{
    double e = 0;
    for (size_t i = channel < 0 ? 0 : channel; i < b.size(); i += channel < 0 ? 1 : 2)
        e += b[i] * b[i];
    return e;
}
int main()
{
    SDL_Init(0);
    auto root = fs::temp_directory_path() / ("canis-spatial-" + std::to_string(SDL_GetTicksNS()));
    fs::create_directories(root);
    auto path = root / "noise.wav";
    Wave(path);
    try
    {
        GetProjectConfig().volume = GetProjectConfig().sfxVolume = 1;
        GetProjectConfig().mute = false;
        Check(Audio::InitializeOffline(), "Spatial processor initialization failed");
        AudioClipAsset clip;
        Check(clip.Load(path.string()), "WAV decode failed");
        Audio::SetListener(Vector3(0), Quaternion(1, 0, 0, 0), 1, true);
        Audio::PlaybackSettings p;
        p.spatialBlend = 1;
        p.position = Vector3(2, 0, 0);
        p.minDistance = 2;
        p.loops = -1;
        auto h = Audio::Play(clip, p);
        Check(h.IsValid(), "Play failed");
        auto right = Render();
        Check(Energy(right, 1) > Energy(right, 0) * 1.1, "Source on right did not favor right ear");
        Audio::Stop(h);
        p.position = Vector3(-2, 0, 0);
        h = Audio::Play(clip, p);
        auto left = Render();
        Check(Energy(left, 0) > Energy(left, 1) * 1.1, "Source on left did not favor left ear");
        Audio::Stop(h);
        Audio::SetListener(Vector3(0), glm::angleAxis(3.14159265f, Vector3(0, 1, 0)), 1, true);
        h = Audio::Play(clip, p);
        auto turned = Render();
        Check(Energy(turned, 1) > Energy(turned, 0) * 1.1, "Listener rotation did not reverse ears");
        Audio::Stop(h);
        Audio::SetListener(Vector3(0), Quaternion(1, 0, 0, 0), 1, true);
        p.position = Vector3(-20, 0, 0);
        h = Audio::Play(clip, p);
        auto far = Render();
        Check(Energy(far) < Energy(left) * .03, "Distance rolloff failed");
        Audio::Stop(h);
        p.spatialBlend = 0;
        h = Audio::Play(clip, p);
        auto flat = Render();
        Check(std::abs(Energy(flat, 0) / Energy(flat, 1) - 4) < .01, "2D stereo preservation failed");
        Audio::Pause(h, true);
        Check(!Audio::IsPlaying(h) && Audio::IsAlive(h), "Pause discarded voice");
        Check(Energy(Render()) == 0, "Paused source emitted samples");
        Audio::Pause(h, false);
        Check(Energy(Render()) > 0, "UnPause failed");
        clip.Free();
        Check(Energy(Render()) > 0, "Freeing cached clip invalidated active PCM");
        Audio::Stop(h);
        Check(!Audio::IsAlive(h) && Energy(Render()) == 0, "Stop failed");
        Check(clip.Load(path.string()), "Reload WAV failed");
        p.loops = 0;
        p.pitch = 3;
        h = Audio::Play(clip, p);
        Render();
        Render();
        Check(!Audio::IsAlive(h), "Pitch/end-of-clip completion failed");
        p.pitch = 1;
        p.spatialBlend = 1;
        h = Audio::Play(clip, p);
        for (int i = 0; i < 6; ++i)
            Render();
        Check(!Audio::IsAlive(h), "HRTF tail never completed");
        p.loops = -1;
        p.sceneOwned = true;
        h = Audio::Play(clip, p);
        Audio::SetScenePaused(true);
        Check(Energy(Render()) == 0, "Scene pause failed");
        Audio::SetScenePaused(false);
        Check(Energy(Render()) > 0, "Scene resume failed");
        Audio::StopAll();
        p.sceneOwned = false;
        p.spatialBlend = 0;
        std::array<Audio::PlaybackHandle, 128> handles;
        for (auto &handle : handles)
        {
            handle = Audio::Play(clip, p);
            Check(handle.IsValid(), "Voice pool filled early");
        }
        Check(!Audio::Play(clip, p).IsValid(), "Voice pool was not bounded");
        Audio::StopAll();
        {
            App app;
            Editor editor;
            app.scene.app = &app;
            app.RegisterDefaults(editor);
            auto listener = app.scene.CreateEntity("Listener");
            listener->AddComponent<Transform>();
            listener->AddComponent<AudioListener>();
            auto emitter = app.scene.CreateEntity("Emitter");
            emitter->AddComponent<Transform>()->position = Vector3(2, 0, 0);
            auto &source = *emitter->AddComponent<AudioSource>();
            source.clip.path = path.string();
            source.loop = true;
            UpdateSceneAudio(app.scene);
            Check(source.IsPlaying() && Energy(Render()) > 0, "PlayOnAwake failed");
            auto serialized = app.scene.EncodeEntity(*emitter);
            Check(serialized["Canis::AudioSource"]["clip"].IsDefined(), "Audio clip not serialized");
            Check(!serialized["Canis::AudioSource"]["sounds"], "Runtime voice IDs leaked into scene");
            auto listenerNode = app.scene.EncodeEntity(*listener);
            listener->GetComponent<AudioListener>().enabled = false;
            UpdateSceneAudio(app.scene);
            Check(Energy(Render()) == 0, "Missing listener did not mute scene audio");
            listener->GetComponent<AudioListener>().enabled = true;
            UpdateSceneAudio(app.scene);
            Check(Energy(Render()) > 0, "Listener enable did not restore scene audio");
            source.enabled = false;
            UpdateSceneAudio(app.scene);
            Check(!source.IsPlaying() && Energy(Render()) == 0, "Disable failed to stop source");
            source.enabled = true;
            UpdateSceneAudio(app.scene);
            Check(source.IsPlaying(), "Reenable failed");
            emitter->RemoveComponent<AudioSource>();
            Check(Energy(Render()) == 0, "Component removal left a voice playing");
            auto &replacement = *emitter->AddComponent<AudioSource>();
            replacement.clip.path = path.string();
            replacement.loop = true;
            UpdateSceneAudio(app.scene);
            Check(Energy(Render()) > 0, "Replacement source failed");
            app.scene.Unload();
            Check(Energy(Render()) == 0, "Scene unload left voices playing");
            YAML::Node nodes(YAML::NodeType::Sequence);
            nodes.push_back(listenerNode);
            nodes.push_back(serialized);
            auto restored = app.scene.LoadEntityNodes(nodes);
            auto &restoredSource = restored[1]->GetComponent<AudioSource>();
            Check(restoredSource.loop && restoredSource.clip.path == path.string() && !restoredSource.IsPlaying(),
                  "Audio configuration did not round trip cleanly");
            UpdateSceneAudio(app.scene);
            Check(restoredSource.IsPlaying() && Energy(Render()) > 0, "Restored source did not start");
            app.scene.Unload();
        }
        Audio::Shutdown();
        Check(Audio::InitializeOffline(), "Audio restart failed");
        Check(!Audio::IsAlive(h), "Stale voice revived after restart");
        Audio::Shutdown();
        fs::remove_all(root);
        SDL_Quit();
        std::cout << "Spatial audio passed: " << Audio::SpatialBackend() << "\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        Audio::Shutdown();
        std::cerr << e.what() << "\nFixture: " << root << '\n';
        SDL_Quit();
        return 1;
    }
}
