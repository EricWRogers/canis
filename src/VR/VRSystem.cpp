#include <Canis/VR/VRSystem.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Window.hpp>
#include "OpenXRBackend.hpp"
#include "GLFoveation.hpp"
#include "GLComfortFade.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace Canis::VR
{
    struct System::Impl
    {
        Config config;
        GLFoveation foveation;
        GLComfortFade comfortFade;
        float fadeOpacity = 0;
        State state;
        Diagnostics diagnostics;
        Matrix4 origin{1.0f};
        std::unique_ptr<OpenXRBackend> backend;
        bool initialized = false;
        bool frameBegun = false;
        GLuint mirror = 0, color = 0, depth = 0;
        Pose simulatedHead{{0.0f, 1.65f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, true};
        std::array<Hand, 2> simulatedHands{};
        bool simulatedFocus = true;
        int width = 0, height = 0;
    };
    System::System() : m(std::make_unique<Impl>()) {}
    System::~System() { Shutdown(); }
    bool System::Initialize(Window& window, const Config& config, std::string& error)
    {
        Shutdown();
        m->config = config;
        m->state = {};
        m->diagnostics = {};
        m->origin = Matrix4(1.0f);
        try
        {
            if (!ValidatePlayerSettings(config.player,error)) throw std::runtime_error(error);
            if (!(config.nearClip > 0 && config.farClip > config.nearClip) ||
                !std::isfinite(config.player.localFloorHeight) || config.simulationEyeWidth < 16 || config.simulationEyeWidth > 4096 ||
                config.simulationEyeHeight < 16 || config.simulationEyeHeight > 4096)
                throw std::runtime_error("Invalid VR configuration");
            if (config.mode == Mode::OpenXR)
            {
                m->backend = CreateOpenXRBackend();
                m->backend->Initialize(window, config, m->diagnostics);
            }
            else
            {
                m->diagnostics.runtime = "Canis stereo simulation (not a headset runtime)";
                m->simulatedHead = {{0,config.player.localFloorHeight,0},{1,0,0,0},true};
                m->simulatedFocus = true;
                for (unsigned i = 0; i < 2; ++i)
                {
                    auto& hand = m->simulatedHands[i];
                    hand = {};
                    hand.active = true;
                    hand.grip.valid = hand.aim.valid = true;
                    hand.grip.position = Vector3(i == 0 ? -0.25f : 0.25f, 1.2f, -0.45f);
                    hand.aim = hand.grip;
                }
            }
            if (config.foveation != FoveationMode::Off)
            {
                if (config.mode == Mode::Simulated)
                    m->diagnostics.foveation = "off: simulation uses full-quality stereo";
                else if (!m->foveation.Supported())
                    m->diagnostics.foveation = "off: GL_NV_shading_rate_image is unavailable on this GPU";
                else if (config.foveation == FoveationMode::EyeTracked && !m->diagnostics.eyeGazeSupported)
                    m->diagnostics.foveation = "off: runtime has no supported eye-gaze input";
                else m->diagnostics.foveation = config.foveation == FoveationMode::Fixed ? "experimental fixed NVIDIA shading-rate image" : "experimental eye-tracked NVIDIA shading-rate image; invalid gaze uses full quality";
            }
            // Fixed-size spectator target, independent of runtime eye sizes.
            m->width = config.simulationEyeWidth * 2;
            m->height = config.simulationEyeHeight;
            GLint oldFramebuffer = 0, oldTexture = 0, oldRenderbuffer = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
            glGetIntegerv(GL_RENDERBUFFER_BINDING, &oldRenderbuffer);
            glGenFramebuffers(1, &m->mirror);
            glBindFramebuffer(GL_FRAMEBUFFER, m->mirror);
            glGenTextures(1, &m->color);
            glBindTexture(GL_TEXTURE_2D, m->color);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m->width, m->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m->color, 0);
            glGenRenderbuffers(1, &m->depth);
            glBindRenderbuffer(GL_RENDERBUFFER, m->depth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m->width, m->height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m->depth);
            bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
            // A headset may spend several frames idle before READY; never mirror
            // uninitialized texture contents while waiting for the first frame.
            if (complete)
            {
                GLfloat previousClear[4]; glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClear);
                const bool scissor = glIsEnabled(GL_SCISSOR_TEST);
                glDisable(GL_SCISSOR_TEST); glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
                glClearColor(previousClear[0],previousClear[1],previousClear[2],previousClear[3]);
                if (scissor) glEnable(GL_SCISSOR_TEST);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, oldFramebuffer);
            glBindTexture(GL_TEXTURE_2D, oldTexture);
            glBindRenderbuffer(GL_RENDERBUFFER, oldRenderbuffer);
            if (!complete) throw std::runtime_error("VR mirror framebuffer is incomplete");
            m->initialized = true;
            return true;
        }
        catch (const std::exception& e) { error = e.what(); Shutdown(); return false; }
    }
    void System::Shutdown()
    {
        m->comfortFade.Reset();
        m->fadeOpacity = 0;
        m->foveation.Reset();
        m->backend.reset();
        if (m->depth) glDeleteRenderbuffers(1, &m->depth);
        if (m->color) glDeleteTextures(1, &m->color);
        if (m->mirror) glDeleteFramebuffers(1, &m->mirror);
        m->depth = m->color = m->mirror = 0;
        m->initialized = m->frameBegun = false;
        m->state = {};
    }
    bool System::BeginFrame(std::string& error)
    {
        try
        {
            if (!m->initialized || m->frameBegun) throw std::runtime_error("VR BeginFrame lifecycle violation");
            if (m->backend) m->backend->Begin(m->state);
            else
            {
                m->state.running = true;
                m->state.focused = m->simulatedFocus;
                m->state.head = m->simulatedHead;
                m->state.shouldRender = m->state.head.valid;
                m->state.hands = m->simulatedFocus ? m->simulatedHands : std::array<Hand, 2>{};
                for (unsigned i = 0; i < 2; ++i)
                {
                    auto& eye = m->state.eyes[i];
                    eye.pose = m->simulatedHead;
                    eye.pose.position += eye.pose.orientation * Vector3(i == 0 ? -0.032f : 0.032f, 0, 0);
                    eye.projection = Projection(-PI / 4, PI / 4, -PI / 4, PI / 4, m->config.nearClip, m->config.farClip);
                    eye.width = m->config.simulationEyeWidth;
                    eye.height = m->config.simulationEyeHeight;
                }
            }
            m->frameBegun = m->state.running;
            return true;
        }
        catch (const std::exception& e) { error = e.what(); return false; }
    }
    bool System::RenderAndEndFrame(const RenderEye& render, std::string& error)
    {
        if (!m->frameBegun) return true;
        const auto start = std::chrono::steady_clock::now();
        GLint oldDraw = 0, oldRead = 0, viewport[4], scissor[4];
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &oldDraw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &oldRead);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissor);
        const bool scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
#ifndef __EMSCRIPTEN__
        const bool srgbEnabled = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        glDisable(GL_FRAMEBUFFER_SRGB);
#endif
        bool ok = true;
        try
        {
            glDisable(GL_SCISSOR_TEST);
            if (m->backend)
            {
                auto foveatedRender = [&](const Eye& eye, const Matrix4& view, unsigned int framebuffer)
                {
                    Vector2 center(0.5f);
                    bool enabled = m->config.foveation == FoveationMode::Fixed;
                    if (m->config.foveation == FoveationMode::EyeTracked)
                        enabled = m->state.focused && GazeCenter(m->state.gaze, eye.pose, eye.projection, center);
                    if (enabled) m->foveation.Begin(eye.width, eye.height, center);
                    try { render(eye, view, framebuffer); }
                    catch (...) { m->foveation.End(); throw; }
                    m->foveation.End();
                    m->comfortFade.Draw(m->fadeOpacity);
                };
                m->backend->RenderEnd(m->state, foveatedRender, m->origin, m->mirror, m->width, m->height);
            }
            else if (m->state.shouldRender)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, m->mirror);
                glClearColor(0.05f, 0.06f, 0.08f, 1);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                for (unsigned i = 0; i < 2; ++i)
                {
                    const auto& eye = m->state.eyes[i];
                    glViewport(i * eye.width, 0, eye.width, eye.height);
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(i * eye.width, 0, eye.width, eye.height);
                    render(eye, glm::inverse(m->origin * PoseMatrix(eye.pose)), m->mirror);
                    m->comfortFade.Draw(m->fadeOpacity);
                }
            }
            ++m->diagnostics.frames;
            if (m->state.shouldRender) m->diagnostics.renderedEyes += 2;
            else ++m->diagnostics.skippedFrames;
        }
        catch (const std::exception& e) { error = e.what(); ok = false; }
        m->frameBegun = false;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, oldDraw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, oldRead);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
        if (scissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
#ifndef __EMSCRIPTEN__
        if (srgbEnabled) glEnable(GL_FRAMEBUFFER_SRGB);
#endif
        m->diagnostics.renderCpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        return ok;
    }
    bool System::Haptic(unsigned hand, float amplitude, float seconds)
    {
        if (hand >= 2 || !m->state.focused || !std::isfinite(amplitude) || !std::isfinite(seconds) || seconds <= 0) return false;
        return m->backend && m->backend->Haptic(hand, std::clamp(amplitude, 0.0f, 1.0f), std::min(seconds, 1.0f));
    }
    const State& System::GetState() const { return m->state; }
    const Diagnostics& System::GetDiagnostics() const { return m->diagnostics; }
    const PlayerSettings& System::GetPlayerSettings() const { return m->config.player; }
    bool System::IsSimulated() const { return m->config.mode == Mode::Simulated; }
    void System::SetComfortFade(float opacity) { m->fadeOpacity = std::isfinite(opacity) ? std::clamp(opacity,0.0f,1.0f) : 0.0f; }
    void System::SetOrigin(const Vector3& position, float yaw)
    {
        m->origin = glm::translate(Matrix4(1.0f), position) * glm::rotate(Matrix4(1.0f), yaw, Vector3(0, 1, 0));
    }
    const Matrix4& System::GetOrigin() const { return m->origin; }
    Pose System::WorldPose(const Pose& pose) const { return TransformPose(m->origin, pose); }
    void System::SetSimulatedInput(const Pose& head, const std::array<Hand, 2>& hands, bool focused)
    { m->simulatedHead = head; m->simulatedHands = hands; m->simulatedFocus = focused; }
    unsigned int System::MirrorFramebuffer() const { return m->mirror; }
    unsigned int System::MirrorTexture() const { return m->color; }
    int System::MirrorWidth() const { return m->width; }
    int System::MirrorHeight() const { return m->height; }
}
