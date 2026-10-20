#include <Canis/WeatherTransition.hpp>

#include <Canis/App.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Scene.hpp>
#include <Canis/VFX/Particles.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace Canis
{
    namespace
    {
        float Seconds(float value)
        {
            return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
        }

        float Intensity(float value)
        {
            return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
        }

        ScriptConf weatherConf;
    }

    void WeatherRainEnvelope::Begin(float from, float target, float delaySeconds,
                                    float durationSeconds)
    {
        m_from = Intensity(from);
        m_target = Intensity(target);
        m_delay = Seconds(delaySeconds);
        m_duration = Seconds(durationSeconds);
        m_elapsed = 0.0f;
        m_finished = false;
        Evaluate();
    }

    void WeatherRainEnvelope::Advance(float deltaTime, bool paused)
    {
        if (paused || m_finished || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
            return;
        // Cap before addition, which also handles very large time steps safely.
        m_elapsed = float(std::min({double(std::numeric_limits<float>::max()),
                                   double(m_delay) + m_duration,
                                   double(m_elapsed) + double(deltaTime)}));
        Evaluate();
    }

    void WeatherRainEnvelope::Evaluate()
    {
        if (m_elapsed < m_delay)
        {
            m_value = m_from;
            return;
        }
        const float amount = m_duration > 0.0f
                                 ? std::clamp((m_elapsed - m_delay) / m_duration, 0.0f, 1.0f)
                                 : 1.0f;
        m_value = m_from + (m_target - m_from) * amount;
        m_finished = amount >= 1.0f;
    }

    std::string WeatherTransition::Validate() const
    {
        if (!targetScenario.empty())
        {
            const auto& scenarios = entity.scene.GetLightingScenarios();
            const auto found = std::find_if(scenarios.begin(), scenarios.end(), [&](const auto& scenario)
            { return scenario.id == targetScenario || scenario.name == targetScenario; });
            if (found == scenarios.end())
                return "Target lighting scenario is missing. Choose another scenario.";
        }
        if (!std::isfinite(lightingDuration) || lightingDuration < 0.0f ||
            !std::isfinite(rainDelay) || rainDelay < 0.0f ||
            !std::isfinite(rainBuildDuration) || rainBuildDuration < 0.0f)
            return "Transition durations must be finite and at least zero seconds.";
        if (!std::isfinite(targetRainIntensity) || targetRainIntensity < 0.0f ||
            targetRainIntensity > 1.0f)
            return "Rain intensity must be between zero and one.";
        std::unordered_set<uint64_t> seen;
        for (const Entity& target : rainEmitters)
        {
            if (!target || &target.scene != &entity.scene)
                return "A rain emitter reference is missing or belongs to another scene.";
            if (!seen.insert(uint64_t(target.GetUUID())).second)
                return "The same rain emitter is listed more than once.";
            if (!target.HasComponent<ParticleEmitter>())
                return "A rain target does not have a ParticleEmitter component.";
            const auto& emitter = target.GetComponent<ParticleEmitter>();
            if (emitter.burstMode || !emitter.looping || emitter.destroyEntityWhenDone)
                return "Rain emitters must be Continuous, Looping, and keep their entity when done.";
            if (!std::isfinite(emitter.spawnInterval) || emitter.spawnInterval <= 0.0f ||
                emitter.spawnCount <= 0)
                return "Rain emitter spawn count and interval must be positive.";
        }
        return {};
    }

    void WeatherTransition::Ready()
    {
        if (playOnStart)
            Start();
    }

    bool WeatherTransition::Start()
    {
        m_lastError = Validate();
        if (!m_lastError.empty())
        {
            Debug::Warning("Weather transition: %s", m_lastError.c_str());
            return false;
        }
        // Validate the lighting request before touching any emitter. Missing or
        // incompatible bake data must not start rain without the lighting fade.
        if (!targetScenario.empty() &&
            !entity.scene.TransitionLightingScenario(targetScenario, lightingDuration))
        {
            m_lastError = "Lighting transition could not start. Check the scenario bake status.";
            return false;
        }
        if (m_manual && targetScenario.empty() && m_controlsLighting)
            entity.scene.EndLightingScenarioBlend();
        CaptureRain(false);
        m_elapsed = 0.0f;
        m_controlsLighting = !targetScenario.empty();
        m_manual = false;
        m_running = true;
        ApplyRain();
        return true;
    }

    bool WeatherTransition::BeginManual(const std::string& fromScenario)
    {
        m_lastError = Validate();
        if (!m_lastError.empty())
            return false;
        if (!targetScenario.empty() &&
            !entity.scene.BeginLightingScenarioBlend(fromScenario, targetScenario))
        {
            m_lastError = "Lighting blend could not start. Check both scenario bake states.";
            return false;
        }
        if (m_manual && targetScenario.empty() && m_controlsLighting)
            entity.scene.EndLightingScenarioBlend();
        CaptureRain(true);
        m_elapsed = 0.0f;
        m_controlsLighting = !targetScenario.empty();
        m_manual = true;
        m_running = true;
        ApplyRain();
        return true;
    }

    bool WeatherTransition::ApplyManual(float lightingBlend, float rainIntensity)
    {
        // Check the entire frame before touching either channel. In particular,
        // invalid animation data must not leave lighting and rain out of sync.
        if (!m_manual || !m_running || entity.scene.IsPaused() ||
            !std::isfinite(lightingBlend) || !std::isfinite(rainIntensity))
            return false;
        if (m_controlsLighting && !entity.scene.SetLightingScenarioBlend(lightingBlend))
            return false;
        for (RainState& state : m_rain)
            state.envelope.Begin(rainIntensity, rainIntensity, 0, 0);
        ApplyRain();
        return true;
    }

    void WeatherTransition::EndManual()
    {
        if (!m_manual)
            return;
        if (m_controlsLighting)
            entity.scene.EndLightingScenarioBlend();
        m_manual = false;
        m_running = false;
    }

    void WeatherTransition::CaptureRain(bool manual)
    {
        std::vector<RainState> next;
        next.reserve(rainEmitters.size());
        for (Entity target : rainEmitters)
        {
            auto& emitter = target.GetComponent<ParticleEmitter>();
            auto previous = std::find_if(m_rain.begin(), m_rain.end(), [&](const RainState& state)
            { return state.emitter == target; });
            RainState state;
            float from = 0.0f;
            if (previous != m_rain.end())
            {
                state = *previous;
                from = state.envelope.Value();
            }
            else
            {
                state.emitter = target;
                state.authoredInterval = emitter.spawnInterval;
                state.authoredPlaying = emitter.playing;
                state.authoredInitialized = emitter.initialized;
                state.authoredPendingDestroy = emitter.pendingDestroy;
                state.authoredAccumulator = emitter.spawnAccumulator;
                from = (emitter.initialized ? emitter.playing : emitter.playOnStart) ? 1.0f : 0.0f;
            }
            if (manual)
                state.envelope.Begin(from, from, 0, 0);
            else
                state.envelope.Begin(from, targetRainIntensity, rainDelay, rainBuildDuration);
            next.push_back(state);
            if (!emitter.playing)
                emitter.spawnAccumulator = 0.0f;
            emitter.initialized = true;
            emitter.pendingDestroy = false;
        }
        // Removing a target while testing the component should release it back
        // to its original emission settings rather than leave it half faded.
        for (RainState& state : m_rain)
        {
            if (std::none_of(next.begin(), next.end(), [&](const RainState& kept)
                { return kept.emitter == state.emitter; }) && state.emitter &&
                state.emitter.HasComponent<ParticleEmitter>())
            {
                auto& emitter = state.emitter.GetComponent<ParticleEmitter>();
                emitter.spawnInterval = state.authoredInterval;
                emitter.playing = state.authoredPlaying;
                emitter.initialized = state.authoredInitialized;
                emitter.pendingDestroy = state.authoredPendingDestroy;
                emitter.spawnAccumulator = state.authoredAccumulator;
            }
        }
        m_rain = std::move(next);
    }

    void WeatherTransition::ApplyRain()
    {
        for (RainState& state : m_rain)
        {
            if (!state.emitter || !state.emitter.HasComponent<ParticleEmitter>())
                continue;
            auto& emitter = state.emitter.GetComponent<ParticleEmitter>();
            const float intensity = state.envelope.Value();
            emitter.playing = intensity > 0.0f;
            emitter.pendingDestroy = false;
            // Never call Stop(): rain tails stay alive until their own lifetime
            // ends. Preserve authored spawnCount and particle budget.
            emitter.spawnInterval = intensity > 0.0f
                                        ? float(std::min(double(std::numeric_limits<float>::max()),
                                                         double(std::max(state.authoredInterval, .001f)) / intensity))
                                        : state.authoredInterval;
        }
    }

    void WeatherTransition::Update(float deltaTime)
    {
        if (!m_running || entity.scene.IsPaused() || !std::isfinite(deltaTime) || deltaTime <= 0.0f)
            return;
        m_elapsed = float(std::min(double(std::numeric_limits<float>::max()),
                                   double(m_elapsed) + deltaTime));
        if (m_manual)
            return; // Animator/game script owns both curves; no second timer.
        bool rainFinished = true;
        for (RainState& state : m_rain)
        {
            state.envelope.Advance(deltaTime);
            rainFinished = rainFinished && state.envelope.Finished();
        }
        ApplyRain();
        m_running = !rainFinished || (m_controlsLighting &&
                    (entity.scene.IsLightingScenarioTransitioning() ||
                     entity.scene.HasQueuedLightingScenarioTransition()));
    }

    float WeatherTransition::RainIntensity() const
    {
        if (m_rain.empty())
            return 0.0f;
        float intensity = 0.0f;
        for (const RainState& state : m_rain)
            intensity += state.envelope.Value();
        return intensity / float(m_rain.size());
    }

    void WeatherTransition::Reset()
    {
        EndManual();
        for (RainState& state : m_rain)
            if (state.emitter && state.emitter.HasComponent<ParticleEmitter>())
            {
                auto& emitter = state.emitter.GetComponent<ParticleEmitter>();
                emitter.spawnInterval = state.authoredInterval;
                emitter.playing = state.authoredPlaying;
                emitter.initialized = state.authoredInitialized;
                emitter.pendingDestroy = state.authoredPendingDestroy;
                emitter.spawnAccumulator = state.authoredAccumulator;
            }
        m_rain.clear();
        m_elapsed = 0;
        m_running = m_manual = m_controlsLighting = false;
        m_lastError.clear();
    }

    void WeatherTransition::Destroy()
    {
        Reset();
    }

    void RegisterWeatherTransition(App& app)
    {
        weatherConf = {};
        REGISTER_PROPERTY(weatherConf, WeatherTransition, playOnStart);
        REGISTER_PROPERTY(weatherConf, WeatherTransition, targetScenario);
        REGISTER_PROPERTY(weatherConf, WeatherTransition, lightingDuration);
        REGISTER_PROPERTY(weatherConf, WeatherTransition, rainEmitters);
        REGISTER_PROPERTY(weatherConf, WeatherTransition, rainDelay);
        REGISTER_PROPERTY(weatherConf, WeatherTransition, rainBuildDuration);
        REGISTER_PROPERTY(weatherConf, WeatherTransition, targetRainIntensity);
        DEFAULT_CONFIG(weatherConf, WeatherTransition);
        weatherConf.DrawInspector = [](Editor& editor, Entity& owner, const ScriptConf& conf)
        {
#if CANIS_EDITOR
            auto* weather = owner.GetScript<WeatherTransition>();
            if (!weather)
                return;
            const char* id = conf.name.c_str();
            DrawInspectorField(editor, "Play on start", id, weather->playOnStart);
            ImGui::SeparatorText("Lighting");
            std::string preview = weather->targetScenario.empty() ? "Keep current lighting" : "Missing scenario";
            for (const auto& scenario : owner.scene.GetLightingScenarios())
                if (scenario.id == weather->targetScenario || scenario.name == weather->targetScenario)
                    preview = scenario.name;
            DrawInspectorFieldLabel("Target scenario");
            SetNextInspectorFieldItemWidth();
            if (ImGui::BeginCombo("##WeatherTargetScenario", preview.c_str()))
            {
                if (ImGui::Selectable("Keep current lighting", weather->targetScenario.empty()))
                {
                    weather->targetScenario.clear();
                    editor.NotifySceneChanged();
                }
                for (const auto& scenario : owner.scene.GetLightingScenarios())
                {
                    ImGui::PushID(scenario.id.c_str());
                    if (ImGui::Selectable(scenario.name.c_str(), scenario.id == weather->targetScenario))
                    {
                        weather->targetScenario = scenario.id;
                        editor.NotifySceneChanged();
                    }
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            DrawInspectorField(editor, "Fade (seconds)", id, weather->lightingDuration);
            ImGui::TextWrapped("Choose the previous scenario to reverse. Other targets wait for the active lighting blend to finish.");

            ImGui::SeparatorText("Rain");
            DrawInspectorField(editor, "Delay (seconds)", id, weather->rainDelay);
            DrawInspectorField(editor, "Build (seconds)", id, weather->rainBuildDuration);
            DrawInspectorField(editor, "Target intensity", id, weather->targetRainIntensity);
            ImGui::TextWrapped("Intensity 1 uses each emitter's authored rate. Intensity 0 lets existing drops finish.");
            if (weather->rainEmitters.empty())
                ImGui::TextWrapped("Add emitters to control rain. An empty list runs only the lighting transition.");
            for (size_t i = 0; i < weather->rainEmitters.size(); ++i)
            {
                ImGui::PushID(int(i));
                DrawInspectorField(editor, "Emitter", id, weather->rainEmitters[i]);
                if (ImGui::SmallButton("Remove emitter"))
                {
                    weather->rainEmitters.erase(weather->rainEmitters.begin() + i);
                    editor.NotifySceneChanged();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (ImGui::SmallButton("Add rain emitter"))
            {
                weather->rainEmitters.emplace_back();
                editor.NotifySceneChanged();
            }
            const std::string issue = weather->Validate();
            if (!issue.empty())
                ImGui::TextWrapped("%s", issue.c_str());
            const bool inPlay = editor.GetMode() != EDIT;
            ImGui::BeginDisabled(!inPlay || !issue.empty() || weather->IsManual());
            if (ImGui::Button(weather->IsRunning() ? "Restart from current values" : "Start transition"))
                weather->Start();
            ImGui::EndDisabled();
            if (!inPlay)
                ImGui::TextWrapped("Enter Play to test. Scene pause freezes the transition.");
            if (weather->IsRunning())
                ImGui::Text("Elapsed %.1f s | Rain %.0f%%", weather->Elapsed(), weather->RainIntensity() * 100.0f);
            if (weather->IsManual())
                ImGui::TextWrapped("Animation controls lighting and rain. The clip owns timing; the duration fields above apply to Start transition.");
            if (weather->IsRunning() && owner.scene.HasQueuedLightingScenarioTransition())
                ImGui::TextWrapped("Lighting target queued; rain timing runs from Start.");
            if (!weather->LastError().empty())
                ImGui::TextWrapped("%s", weather->LastError().c_str());
#endif
        };
        app.RegisterScript(weatherConf);
    }

    void UnRegisterWeatherTransition(App& app)
    {
        app.UnregisterScript(weatherConf);
    }
} // namespace Canis
