#include <Canis/App.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Scene.hpp>
#include <Canis/VFX/Particles.hpp>
#include <Canis/WeatherTransition.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

using namespace Canis;

namespace
{
    void Check(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    }

    bool Near(float a, float b) { return std::abs(a - b) < 0.0001f; }

    void SurfaceWetnessValidation()
    {
        const SurfaceWetnessSettings defaults;
        Check(!defaults.enabled && Near(defaults.amount, 0) && Near(defaults.dryRoughness, .85f) &&
                  Near(defaults.wetRoughness, .22f) && Near(defaults.darkening, .22f),
              "Surface wetness changed the appearance of legacy scenes by default");
        Check(DeserializeSurfaceWetnessSettings(YAML::Node()) == defaults &&
                  DeserializeSurfaceWetnessSettings(YAML::Load("[]")) == defaults,
              "Missing or incompatible wetness settings did not retain safe defaults");
        SurfaceWetnessSettings settings;
        settings.enabled = true;
        settings.amount = -2;
        settings.dryRoughness = -3;
        settings.wetRoughness = 2;
        settings.darkening = 4;
        settings.Sanitize();
        Check(settings.enabled && Near(settings.amount, 0) && Near(settings.dryRoughness, .04f) &&
                  Near(settings.wetRoughness, 1) && Near(settings.darkening, 1),
              "Out-of-range wetness controls reached the renderer");
        settings.amount = std::numeric_limits<float>::quiet_NaN();
        settings.dryRoughness = std::numeric_limits<float>::infinity();
        settings.wetRoughness = -std::numeric_limits<float>::infinity();
        settings.darkening = std::numeric_limits<float>::quiet_NaN();
        settings.Sanitize();
        Check(Near(settings.amount, defaults.amount) && Near(settings.dryRoughness, defaults.dryRoughness) &&
                  Near(settings.wetRoughness, defaults.wetRoughness) && Near(settings.darkening, defaults.darkening),
              "Nonfinite wetness controls did not recover to finite defaults");
        auto decoded = DeserializeSurfaceWetnessSettings(YAML::Load(
            "{Enabled: true, Amount: 0.65, DryRoughness: 0.74, WetRoughness: 0.18, Darkening: 0.31}"));
        auto restored = DeserializeSurfaceWetnessSettings(YAML::Load(YAML::Dump(SerializeSurfaceWetnessSettings(decoded))));
        Check(decoded == restored && restored.enabled && Near(restored.amount, .65f) &&
                  Near(restored.dryRoughness, .74f) && Near(restored.wetRoughness, .18f) &&
                  Near(restored.darkening, .31f),
              "Surface wetness controls did not survive asset serialization");
    }

    void RainEnvelopeTiming()
    {
        WeatherRainEnvelope envelope;
        envelope.Begin(0, 1, 30, 10);
        envelope.Advance(29);
        Check(Near(envelope.Value(), 0) && !envelope.Finished(), "Rain started before its delay");
        envelope.Advance(6);
        Check(Near(envelope.Value(), .5f), "Rain build did not include time crossing the delay boundary");
        envelope.Advance(100, true);
        Check(Near(envelope.Value(), .5f) && Near(envelope.Elapsed(), 35), "Paused weather advanced");

        envelope.Begin(envelope.Value(), 0, 0, 4);
        Check(Near(envelope.Value(), .5f), "Reversing rain jumped to an endpoint");
        envelope.Advance(2);
        Check(Near(envelope.Value(), .25f), "Reversing rain did not fade from the current intensity");
        envelope.Advance(100);
        Check(Near(envelope.Value(), 0) && envelope.Finished(), "Rain reversal did not finish");

        envelope.Begin(0, 1, 0, 0);
        Check(Near(envelope.Value(), 1) && envelope.Finished(), "Instant transition required an extra frame");
        envelope.Begin(0, 1, 2, 0);
        envelope.Advance(1);
        Check(Near(envelope.Value(), 0), "Instant transition ignored its delay");
        envelope.Advance(1);
        Check(Near(envelope.Value(), 1) && envelope.Finished(), "Delayed instant transition missed its endpoint");

        envelope.Begin(.2f, .8f, 0, 1);
        envelope.Advance(std::numeric_limits<float>::quiet_NaN());
        envelope.Advance(-1);
        Check(Near(envelope.Value(), .2f), "Invalid time step corrupted rain intensity");
        envelope.Advance(.5f);
        Check(Near(envelope.Value(), .5f), "Valid time step failed after an invalid step");
    }

    void EmitterIntegration()
    {
        Scene scene;
        auto owner = scene.CreateEntity("Weather");
        auto rain = scene.CreateEntity("Rain");
        auto& emitter = *rain.AddComponent<ParticleEmitter>();
        emitter.playOnStart = false;
        emitter.looping = true;
        emitter.burstMode = false;
        emitter.destroyEntityWhenDone = false;
        emitter.spawnCount = 4;
        emitter.spawnInterval = .02f;
        const auto existingDrop = scene.GetRegistry().create();
        emitter.particles.push_back(existingDrop);
        auto* weather = owner.AddScript<WeatherTransition>();
        weather->rainEmitters = {rain};
        weather->rainDelay = 30;
        weather->rainBuildDuration = 10;
        Check(weather->Start(), "Valid continuous rain could not start");
        Check(!emitter.playing, "Delayed rain started immediately");
        weather->Update(35);
        Check(emitter.playing && Near(emitter.spawnInterval, .04f) && emitter.spawnCount == 4,
              "Rain did not scale its authored rate without changing the count");
        Check(!emitter.pendingDestroy, "Weather marked the rain entity for deletion");
        scene.SetPaused(true);
        weather->Update(2);
        Check(Near(emitter.spawnInterval, .04f), "Scene pause did not freeze rain transition");
        scene.SetPaused(false);

        weather->targetRainIntensity = 0;
        weather->rainDelay = 0;
        weather->rainBuildDuration = 4;
        Check(weather->Start(), "Rain could not reverse");
        Check(Near(weather->RainIntensity(), .5f), "Reversing component jumped from the current rate");
        weather->Update(2);
        Check(Near(weather->RainIntensity(), .25f) && Near(emitter.spawnInterval, .08f),
              "Rain reversal lost the original authored emission rate");
        weather->Update(2);
        Check(!emitter.playing && !emitter.pendingDestroy && !weather->IsRunning(),
              "Rain did not stop spawning cleanly at zero intensity");
        Check(scene.GetRegistry().valid(existingDrop) && emitter.particles.size() == 1,
              "Stopping rain destroyed existing drops instead of letting their lifetime finish");

        // Invalid settings must not partly restart the emitter or reset an
        // already running transition before the error is reported.
        weather->targetScenario = "Missing lighting scenario";
        const float before = emitter.spawnInterval;
        Check(!weather->Start() && !weather->LastError().empty() &&
              Near(emitter.spawnInterval, before) && !emitter.playing,
              "Invalid scenario mutated rain state");
        weather->targetScenario.clear();
        emitter.burstMode = true;
        Check(!weather->Start(), "Burst emitter was silently converted into continuous rain");
        emitter.burstMode = false;
        weather->rainEmitters.push_back(rain);
        Check(!weather->Start(), "Duplicate emitter was accepted");
        weather->rainEmitters.pop_back();

        owner.RemoveScript<WeatherTransition>();
        Check(Near(emitter.spawnInterval, .02f) && !emitter.playing && !emitter.initialized,
              "Removing weather did not restore original emitter settings");
    }

    void RegistrationRoundTrip()
    {
        App app;
        app.scene.app = &app;
        RegisterWeatherTransition(app);
        auto nodes = YAML::Load(R"(
- Entity: 101
  Name: Weather
  Canis::WeatherTransition:
    playOnStart: false
    targetScenario: dusk-stable-id
    lightingDuration: 30
    rainEmitters: [102]
    rainDelay: 30
    rainBuildDuration: 10
    targetRainIntensity: 0.75
- Entity: 102
  Name: Rain
)");
        auto loaded = app.scene.LoadEntityNodes(nodes);
        auto* weather = loaded.front()->GetScript<WeatherTransition>();
        Check(weather && weather->targetScenario == "dusk-stable-id" &&
              weather->rainEmitters.size() == 1 && weather->rainEmitters[0] == loaded[1] &&
              Near(weather->targetRainIntensity, .75f), "Weather settings or emitter reference did not decode");
        auto encoded = app.scene.EncodeEntity(*loaded.front());
        auto values = encoded[WeatherTransition::ScriptName];
        Check(values && values["targetScenario"].as<std::string>() == "dusk-stable-id" &&
              values["rainEmitters"][0].as<uint64_t>() == 102,
              "Weather settings did not serialize stable IDs and emitter references");
        app.scene.Unload();
        UnRegisterWeatherTransition(app);
    }

    void LightingScenarioTransitions()
    {
        const auto directory = std::filesystem::temp_directory_path() /
                               ("canis-weather-tests-" + std::to_string(uint64_t(UUID())));
        std::filesystem::create_directories(directory);
        const auto manifest = directory / "test.lighting";
        const auto geometry = directory / "shared-geometry.dat";
        std::ofstream(geometry) << "Shared geometry fixture";
        auto sharedGeometryPath = geometry;
        Scene scene;
        auto& clouds = scene.GetCloudVolumeSettings();
        clouds.enabled = true;
        clouds.baseHeight = 95;
        clouds.windVelocity = Vector2(3, -.5f);
        const auto authoredCloudVolume = clouds;
        LightingScenario dusk, night, storm;
        dusk.id = "stable-dusk"; dusk.name = "Dusk";
        night.id = "stable-night"; night.name = "Night";
        storm.id = "stable-storm"; storm.name = "Storm";
        dusk.cloudCoverage = .2f; dusk.cloudDensity = .6f;
        night.cloudCoverage = .7f; night.cloudDensity = 1.2f;
        storm.cloudCoverage = 1; storm.cloudDensity = 2;
        for (auto* scenario : {&dusk, &night, &storm})
        {
            scenario->manifestPath = manifest.string();
            scenario->sourceFingerprint = "fixture-source";
            scenario->settingsFingerprint = LightingScenarioSettingsFingerprint(*scenario);
        }
        scene.GetLightingScenarios() = {dusk, night, storm};
        auto writeManifest = [&](int probeCount = 0)
        {
            YAML::Node data;
            data["Version"] = 1;
            data["SourceFingerprint"] = "fixture-source";
            data["ProbeCount"] = probeCount;
            for (int i = 0; i < probeCount; ++i)
                data["ProbeEnabled"].push_back(true);
            for (const auto& scenario : scene.GetLightingScenarios())
                data["Scenarios"].push_back(SerializeLightingScenario(scenario));
            YAML::Node model;
            model["Entity"] = uint64_t(1);
            model["Path"] = sharedGeometryPath.string();
            data["Models"].push_back(model);
            std::ofstream(manifest) << data;
        };
        writeManifest();
        Check(scene.SetInitialLightingScenario(dusk.id), "Initial lighting scenario was rejected");
        scene.FindLightingScenario(dusk.id)->name = "Warm dusk";
        Check(scene.FindLightingScenario(dusk.id) && scene.FindLightingScenario("Warm dusk") &&
              !scene.FindLightingScenario("Dusk"), "Renaming lighting broke stable IDs or left a stale name lookup");

        auto definitions = [&]()
        {
            YAML::Node result(YAML::NodeType::Sequence);
            for (const auto& scenario : scene.GetLightingScenarios())
                result.push_back(SerializeLightingScenario(scenario));
            return YAML::Dump(result);
        };
        const std::string before = definitions();
        Check(scene.SetLightingScenarioPreview(dusk.id, night.id, .75f), "Valid lighting preview was rejected");
        auto blend = scene.GetLightingScenarioBlend();
        Check(blend.from->id == dusk.id && blend.to->id == night.id && Near(blend.blend, .75f),
              "Lighting preview did not expose its selected endpoints and value");
        Check(Near(blend.from->cloudCoverage, .2f) && Near(blend.to->cloudCoverage, .7f) &&
                  Near(blend.from->cloudDensity, .6f) && Near(blend.to->cloudDensity, 1.2f),
              "Lighting preview changed the authored cloud endpoints before rendering");
        Check(definitions() == before && scene.GetInitialLightingScenario() == dusk.id &&
                  scene.GetCloudVolumeSettings() == authoredCloudVolume,
              "Lighting preview changed saved scenario definitions or startup selection");
        Check(!scene.SetLightingScenarioPreview(dusk.id, "missing", .5f) &&
              !scene.SetLightingScenarioPreview(dusk.id, night.id, std::numeric_limits<float>::quiet_NaN()),
              "Invalid preview inputs were accepted");
        scene.ClearLightingScenarioPreview();
        blend = scene.GetLightingScenarioBlend();
        Check(blend.from->id == dusk.id && blend.to->id == dusk.id && Near(blend.blend, 0),
              "Clearing preview did not restore the startup lighting endpoint");

        Check(!scene.TransitionLightingScenario(night.id, -1) &&
              !scene.TransitionLightingScenario(night.id, std::numeric_limits<float>::infinity()),
              "Invalid lighting transition durations were accepted");
        scene.FindLightingScenario(night.id)->manifestPath = (directory / "missing.lighting").string();
        Check(!scene.TransitionLightingScenario(night.id, 10), "Missing lighting bake was accepted");
        scene.FindLightingScenario(night.id)->manifestPath = manifest.string();

        std::string readinessError;
        Check(LightingScenarioDataAvailable(*scene.FindLightingScenario(night.id), readinessError),
              "Complete minimal lighting fixture was rejected");
        std::filesystem::remove(geometry);
        Check(!LightingScenarioDataAvailable(*scene.FindLightingScenario(night.id), readinessError) &&
              !readinessError.empty() && !scene.TransitionLightingScenario(night.id, 10),
              "Lighting transition accepted a manifest whose shared model was missing");
        std::ofstream(geometry) << "Shared geometry fixture";

        sharedGeometryPath = directory / "shared.gltf";
        std::ofstream(sharedGeometryPath) << R"({"asset":{"version":"2.0"},"buffers":[{"uri":"shared.bin","byteLength":4}]})";
        writeManifest();
        Check(!LightingScenarioDataAvailable(*scene.FindLightingScenario(night.id), readinessError) &&
              !readinessError.empty() && !scene.TransitionLightingScenario(night.id, 10),
              "Lighting transition accepted geometry with a missing external vertex buffer");
        std::ofstream(directory / "shared.bin", std::ios::binary).write("\0\0\0\0", 4);
        Check(LightingScenarioDataAvailable(*scene.FindLightingScenario(night.id), readinessError),
              "Lighting validation rejected geometry after its external buffer was restored");
        sharedGeometryPath = geometry;
        writeManifest();

        auto* target = scene.FindLightingScenario(night.id);
        const float originalSunIntensity = target->sunIntensity;
        target->sunIntensity += 0.25f;
        Check(!LightingScenarioDataAvailable(*target, readinessError) && !readinessError.empty() &&
              !scene.TransitionLightingScenario(night.id, 10),
              "Lighting transition accepted changed settings without a matching bake");
        target->sunIntensity = originalSunIntensity;

        target->probeCachePaths = {(directory / "missing.probe").string()};
        writeManifest(1);
        Check(!LightingScenarioDataAvailable(*target, readinessError, true) && !readinessError.empty() &&
              !scene.TransitionLightingScenario(night.id, 10),
              "Lighting transition accepted a missing scenario probe capture");
        Check(LightingScenarioDataAvailable(*target, readinessError, false),
              "Surface-only lighting validation incorrectly required probe captures");
        target->probeCachePaths.clear();
        writeManifest();

        Check(scene.TransitionLightingScenario("Night", 10), "Named lighting target was rejected");
        scene.UpdateLightingScenarioTransition(0);
        scene.UpdateLightingScenarioTransition(-2);
        scene.UpdateLightingScenarioTransition(std::numeric_limits<float>::quiet_NaN());
        scene.UpdateLightingScenarioTransition(std::numeric_limits<float>::infinity());
        Check(Near(scene.GetLightingScenarioBlend().blend, 0), "Invalid time step moved the lighting blend");
        scene.SetPaused(true);
        scene.UpdateLightingScenarioTransition(5);
        Check(Near(scene.GetLightingScenarioBlend().blend, 0), "Scene pause did not freeze baked lighting");
        scene.SetPaused(false);
        scene.UpdateLightingScenarioTransition(5);
        Check(Near(scene.GetLightingScenarioBlend().blend, .5f), "Lighting midpoint was incorrect");

        Check(scene.TransitionLightingScenario(dusk.id, 4) &&
              Near(scene.GetLightingScenarioBlend().blend, .5f), "Reversing lighting caused a discontinuity");
        scene.UpdateLightingScenarioTransition(2);
        Check(Near(scene.GetLightingScenarioBlend().blend, .25f), "Lighting reversal did not use its current blend");
        scene.UpdateLightingScenarioTransition(2);
        blend = scene.GetLightingScenarioBlend();
        Check(!scene.IsLightingScenarioTransitioning() && blend.from->id == dusk.id &&
              blend.to->id == dusk.id, "Lighting reversal did not finish at dusk");
        Check(scene.TransitionLightingScenario(night.id, 2), "Lighting could not transition after reversal");
        scene.UpdateLightingScenarioTransition(2);
        Check(scene.GetLightingScenarioBlend().from->id == night.id &&
              !scene.IsLightingScenarioTransitioning(), "Lighting did not reach the second endpoint");
        Check(scene.TransitionLightingScenario(dusk.id, 0) &&
              scene.GetLightingScenarioBlend().from->id == dusk.id && !scene.IsLightingScenarioTransitioning(),
              "Instant lighting transition did not finish immediately");

        Check(scene.TransitionLightingScenario(night.id, 10), "Third-target setup failed");
        scene.UpdateLightingScenarioTransition(4);
        const float current = scene.GetLightingScenarioBlend().blend;
        Check(scene.TransitionLightingScenario(storm.id, 8) && scene.HasQueuedLightingScenarioTransition(),
              "Third lighting target did not queue");
        blend = scene.GetLightingScenarioBlend();
        Check(blend.from->id == dusk.id && blend.to->id == night.id && Near(blend.blend, current),
              "Third lighting target interrupted the active pair discontinuously");
        scene.UpdateLightingScenarioTransition(6);
        blend = scene.GetLightingScenarioBlend();
        Check(blend.from->id == night.id && blend.to->id == storm.id && Near(blend.blend, 0) &&
              !scene.HasQueuedLightingScenarioTransition() && scene.IsLightingScenarioTransitioning(),
              "Queued lighting did not start from the completed endpoint");
        scene.UpdateLightingScenarioTransition(4);
        Check(Near(scene.GetLightingScenarioBlend().blend, .5f), "Queued lighting midpoint was incorrect");
        scene.UpdateLightingScenarioTransition(4);
        Check(scene.GetLightingScenarioBlend().from->id == storm.id &&
              !scene.IsLightingScenarioTransitioning(), "Queued lighting did not reach its target");

        auto owner = scene.CreateEntity("Animated weather");
        auto rain = scene.CreateEntity("Animated rain");
        auto& emitter = *rain.AddComponent<ParticleEmitter>();
        emitter.playOnStart = false;
        emitter.looping = true;
        emitter.burstMode = false;
        emitter.destroyEntityWhenDone = false;
        emitter.spawnCount = 4;
        emitter.spawnInterval = .02f;
        const auto existingDrop = scene.GetRegistry().create();
        emitter.particles.push_back(existingDrop);
        auto* weather = owner.AddScript<WeatherTransition>();
        weather->rainEmitters = {rain};
        weather->targetScenario = night.id;
        Check(!scene.SetLightingScenarioBlend(.5f), "Manual weight was accepted before Begin");
        const auto originalFrom = scene.GetLightingScenarioBlend().from->id;
        Check(!weather->BeginManual("missing") && !weather->IsRunning() && !emitter.initialized &&
              scene.GetLightingScenarioBlend().from->id == originalFrom,
              "Invalid manual endpoint partly started the weather");
        scene.FindLightingScenario(dusk.id)->manifestPath = (directory / "missing.lighting").string();
        Check(!weather->BeginManual(dusk.id) && !emitter.initialized,
              "Manual lighting validated only the target instead of both endpoints");
        scene.FindLightingScenario(dusk.id)->manifestPath = manifest.string();
        Check(scene.SetLightingScenarioPreview(storm.id, dusk.id, .2f), "Manual preview setup failed");
        Check(weather->BeginManual(dusk.id) && weather->IsManual() &&
                  scene.IsLightingScenarioBlendManual() && !scene.HasLightingScenarioPreview() &&
                  !scene.IsLightingScenarioTransitioning() && !scene.HasQueuedLightingScenarioTransition(),
              "Animation-driven weather did not take sole ownership of transient lighting");
        Check(weather->ApplyManual(.5f, .25f) && Near(scene.GetLightingScenarioBlend().blend, .5f) &&
                  Near(weather->RainIntensity(), .25f) && Near(emitter.spawnInterval, .08f) &&
                  emitter.spawnCount == 4,
              "Animator values did not reach baked lighting and authored rain emission");
        weather->Update(100);
        scene.UpdateLightingScenarioTransition(100);
        Check(weather->IsRunning() && Near(weather->RainIntensity(), .25f) &&
                  Near(scene.GetLightingScenarioBlend().blend, .5f),
              "Manual weather was advanced by an independent duration timer");
        scene.SetPaused(true);
        Check(!weather->ApplyManual(.8f, .8f) && Near(weather->RainIntensity(), .25f) &&
                  Near(scene.GetLightingScenarioBlend().blend, .5f),
              "Paused animation application changed weather");
        scene.SetPaused(false);
        Check(!weather->ApplyManual(.8f, std::numeric_limits<float>::quiet_NaN()) &&
                  !weather->ApplyManual(std::numeric_limits<float>::infinity(), .8f) &&
                  Near(weather->RainIntensity(), .25f) && Near(scene.GetLightingScenarioBlend().blend, .5f),
              "Nonfinite animation channel partly mutated weather");
        // Once prepared, applying an animation key never reads the bake again.
        std::filesystem::remove(manifest);
        Check(weather->ApplyManual(2, 2) && Near(weather->RainIntensity(), 1) &&
                  Near(scene.GetLightingScenarioBlend().blend, 1) && Near(emitter.spawnInterval, .02f),
              "Manual application rescanned disk assets or failed to clamp an animation weight");
        writeManifest();
        weather->EndManual();
        Check(!weather->IsRunning() && !weather->IsManual() && !scene.IsLightingScenarioBlendManual() &&
                  scene.GetLightingScenarioBlend().from->id == night.id && emitter.playing,
              "Finishing animation did not preserve the night and rain endpoint");
        Check(!weather->ApplyManual(0, 0) && emitter.playing,
              "Stopped animation continued mutating weather");
        weather->targetScenario = dusk.id;
        Check(weather->BeginManual(night.id) && weather->ApplyManual(.4f, 0) &&
                  !emitter.playing && scene.GetRegistry().valid(existingDrop) && emitter.particles.size() == 1,
              "Reversed manual weather lost rain tails or failed to select the reverse pair");
        weather->EndManual();
        Check(Near(scene.GetLightingScenarioBlend().blend, .4f),
              "Stopping animation at an interior key jumped the lighting");
        weather->lightingDuration = 2;
        weather->rainDelay = 0;
        weather->rainBuildDuration = 2;
        Check(weather->Start() && !weather->IsManual() &&
                  Near(scene.GetLightingScenarioBlend().blend, .4f),
              "Timed Start could not take over a stopped animation without jumping");
        scene.UpdateLightingScenarioTransition(2);
        weather->Update(2);
        Check(scene.GetLightingScenarioBlend().from->id == dusk.id && Near(weather->RainIntensity(), 1) &&
                  !weather->IsRunning(), "Timed transition stopped working after manual control");
        Check(weather->BeginManual(night.id) && weather->ApplyManual(.6f, .5f), "Manual reset setup failed");
        scene.ResetLightingScenarioRuntime();
        Check(!scene.IsLightingScenarioBlendManual() && !weather->ApplyManual(.9f, .9f) &&
                  Near(weather->RainIntensity(), .5f), "Reset left manual lighting controls active");
        emitter.pendingDestroy = true;
        emitter.spawnAccumulator = .7f;
        weather->Reset();
        Check(!weather->IsRunning() && !weather->IsManual() && Near(weather->Elapsed(), 0) &&
                  Near(weather->RainIntensity(), 0) && weather->LastError().empty() &&
                  Near(emitter.spawnInterval, .02f) && !emitter.playing && !emitter.initialized &&
                  !emitter.pendingDestroy && Near(emitter.spawnAccumulator, 0),
              "Explicit weather reset retained an old rain envelope, timer or emitter runtime state");
        Check(weather->BeginManual(dusk.id) && weather->ApplyManual(.3f, .4f),
              "A reset weather component could not start a fresh manual transition");
        owner.RemoveScript<WeatherTransition>();
        Check(Near(emitter.spawnInterval, .02f) && !emitter.playing && !emitter.initialized,
              "Removing animated weather did not restore its authored rain settings");
        Check(definitions() == before && scene.GetInitialLightingScenario() == dusk.id &&
                  scene.GetCloudVolumeSettings() == authoredCloudVolume,
              "Runtime lighting transition changed serialized authoring definitions");
        std::filesystem::remove_all(directory);
    }
}

int main()
{
    SurfaceWetnessValidation();
    RainEnvelopeTiming();
    EmitterIntegration();
    RegistrationRoundTrip();
    LightingScenarioTransitions();
    std::cout << "Weather transition tests passed\n";
}
