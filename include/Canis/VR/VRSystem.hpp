#pragma once
#include <Canis/VR/VRMath.hpp>
#include <Canis/VR/Foveation.hpp>
#include <Canis/VR/PlayerSettings.hpp>
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Canis { class Window; }
namespace Canis::VR
{
    enum class Mode { OpenXR, Simulated };
    struct Config
    {
        Mode mode = Mode::OpenXR;
        FoveationMode foveation = FoveationMode::Off;
        float nearClip = 0.05f;
        float farClip = 200.0f;
        PlayerSettings player;
        int simulationEyeWidth = 640;
        int simulationEyeHeight = 640;
    };
    struct Hand
    {
        Pose grip;
        Pose aim;
        float trigger = 0.0f;
        float squeeze = 0.0f;
        Vector2 stick{0.0f};
        bool select = false;
        bool active = false;
    };
    struct Eye
    {
        Pose pose; // tracking-space pose, with local-floor fallback applied
        Matrix4 projection{1.0f};
        int width = 0;
        int height = 0;
    };
    struct State
    {
        bool running = false;
        bool focused = false;
        bool shouldRender = false;
        bool exitRequested = false;
        bool stageSpace = false;
        Pose head;
        Pose gaze;
        std::array<Hand, 2> hands{};
        std::array<Eye, 2> eyes{};
    };
    struct Diagnostics
    {
        std::string runtime;
        std::vector<std::string> extensions;
        std::string foveation = "off";
        bool eyeGazeSupported = false;
        uint64_t frames = 0;
        uint64_t renderedEyes = 0;
        uint64_t skippedFrames = 0;
        double renderCpuMs = 0.0;
    };

    // Owns XR objects and GL eye/mirror targets. Must be shut down before Window.
    class System
    {
    public:
        System();
        ~System();
        System(const System&) = delete;
        System& operator=(const System&) = delete;
        bool Initialize(Window& window, const Config& config, std::string& error);
        void Shutdown();
        bool BeginFrame(std::string& error);
        using RenderEye = std::function<void(const Eye&, const Matrix4& worldView, unsigned int framebuffer)>;
        bool RenderAndEndFrame(const RenderEye& render, std::string& error);
        bool Haptic(unsigned hand, float amplitude, float seconds);
        const State& GetState() const;
        const Diagnostics& GetDiagnostics() const;
        const PlayerSettings& GetPlayerSettings() const;
        bool IsSimulated() const;
        void SetComfortFade(float opacity);
        // Rigid world transform only: scaling the player changes IPD/world scale.
        void SetOrigin(const Vector3& position, float yawRadians);
        const Matrix4& GetOrigin() const;
        Pose WorldPose(const Pose& pose) const;
        // Explicit simulation hook, ignored by actual OpenXR sessions.
        void SetSimulatedInput(const Pose& head, const std::array<Hand, 2>& hands, bool focused = true);
        unsigned int MirrorFramebuffer() const;
        unsigned int MirrorTexture() const;
        int MirrorWidth() const;
        int MirrorHeight() const;
    private:
        struct Impl;
        std::unique_ptr<Impl> m;
    };
}
