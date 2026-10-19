#pragma once

#include <Canis/Entity.hpp>

#include <string>
#include <vector>

namespace Canis
{
    class App;

    // A restart captures Value(), so reversing or interrupting a transition
    // does not jump back to the previously authored endpoint.
    class WeatherRainEnvelope
    {
      public:
        void Begin(float from, float target, float delaySeconds, float durationSeconds);
        void Advance(float deltaTime, bool paused = false);
        float Value() const { return m_value; }
        float Elapsed() const { return m_elapsed; }
        bool Finished() const { return m_finished; }

      private:
        float m_from = 0.0f, m_target = 0.0f, m_value = 0.0f;
        float m_delay = 0.0f, m_duration = 0.0f, m_elapsed = 0.0f;
        bool m_finished = true;
        void Evaluate();
    };

    // Coordinates lighting scenarios (including sky and cloud coverage) and
    // continuous rain emitters without mutating shared assets.
    class WeatherTransition : public ScriptableEntity
    {
      public:
        static constexpr const char* ScriptName = "Canis::WeatherTransition";
        using ScriptableEntity::ScriptableEntity;

        bool playOnStart = false;
        // Stable scenario ID, selected by its display name in the Inspector.
        // Empty leaves lighting unchanged and only controls rain.
        std::string targetScenario;
        float lightingDuration = 30.0f;
        std::vector<Entity> rainEmitters;
        float rainDelay = 30.0f;
        float rainBuildDuration = 10.0f;
        // 1 uses each emitter's authored rate; 0 lets existing drops finish.
        float targetRainIntensity = 1.0f;

        void Ready() override;
        void Update(float deltaTime) override;
        void Destroy() override;
        ScriptUpdatePhase UpdatePhase() const override { return ScriptUpdatePhase::Presentation; }

        bool Start();
        bool IsRunning() const { return m_running; }
        float Elapsed() const { return m_elapsed; }
        float RainIntensity() const;
        const std::string& LastError() const { return m_lastError; }
        // No mutation. Useful for the Inspector and callers before starting.
        std::string Validate() const;

      private:
        struct RainState
        {
            Entity emitter;
            float authoredInterval = 0.03f;
            bool authoredPlaying = false, authoredInitialized = false;
            WeatherRainEnvelope envelope;
        };
        std::vector<RainState> m_rain;
        float m_elapsed = 0.0f;
        bool m_running = false, m_controlsLighting = false;
        std::string m_lastError;

        void ApplyRain();
    };

    void RegisterWeatherTransition(App& app);
    void UnRegisterWeatherTransition(App& app);
} // namespace Canis
