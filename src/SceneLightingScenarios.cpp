#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <Canis/Scene.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
namespace Canis {
const LightingScenario *Scene::FindLightingScenario(const std::string &idOrName) const {
    if (idOrName.empty())
        return nullptr;
    for (const auto &s : m_lightingScenarios)
        if (s.id == idOrName)
            return &s;
    for (const auto &s : m_lightingScenarios)
        if (s.name == idOrName)
            return &s;
    return nullptr;
}
LightingScenario *Scene::FindLightingScenario(const std::string &idOrName) {
    return const_cast<LightingScenario *>(std::as_const(*this).FindLightingScenario(idOrName));
}
LightingScenario Scene::CaptureLightingScenario(const std::string &name) const {
    LightingScenario scenario;
    scenario.id = std::to_string(uint64_t(UUID()));
    scenario.name = name;
    scenario.cloudCoverage = m_cloudVolume.enabled ? .4f : 0.f;
    scenario.ambientLight = m_environmentAmbientLight;
    scenario.ambientIntensity = m_environmentAmbientLightIntensity;
    scenario.skyboxUUID = uint64_t(m_environmentSkyboxUUID);
    if (scenario.skyboxUUID) {
        scenario.skyboxPath = AssetManager::GetPath(m_environmentSkyboxUUID);
        if (scenario.skyboxPath == "Path was not found in AssetLibrary")
            scenario.skyboxPath.clear();
    }
    scenario.sunEnabled = false;
    for (auto handle : m_registry.view<DirectionalLight>()) {
        const auto &sun = m_registry.get<DirectionalLight>(handle);
        scenario.sunEnabled = sun.enabled;
        scenario.sunDirection = sun.direction;
        scenario.sunColor = sun.color;
        scenario.sunIntensity = sun.intensity;
        break;
    }
    return scenario;
}
void Scene::ResetLightingScenarioRuntime() {
    m_lightingScenarioFrom = m_lightingScenarioTo = m_initialLightingScenario;
    m_lightingScenarioQueued.clear();
    m_lightingScenarioBlend = m_lightingScenarioElapsed = 0;
    m_lightingScenarioTransitioning = false;
    m_lightingScenarioManual = false;
    ClearLightingScenarioPreview();
}
bool Scene::SetInitialLightingScenario(const std::string &idOrName) {
    auto *scenario = FindLightingScenario(idOrName);
    if (!scenario && !idOrName.empty())
        return false;
    m_initialLightingScenario = scenario ? scenario->id : "";
    ResetLightingScenarioRuntime();
    return true;
}
LightingScenarioBlend Scene::GetLightingScenarioBlend() const {
    const auto *from = FindLightingScenario(m_lightingScenarioPreview ? m_lightingScenarioPreviewFrom
                                                                      : m_lightingScenarioFrom);
    const auto *to =
        FindLightingScenario(m_lightingScenarioPreview ? m_lightingScenarioPreviewTo : m_lightingScenarioTo);
    if (!from)
        from = FindLightingScenario(m_initialLightingScenario);
    if (!from && !m_lightingScenarios.empty())
        from = &m_lightingScenarios.front();
    if (!to)
        to = from;
    return {from, to, m_lightingScenarioPreview ? m_lightingScenarioPreviewBlend : m_lightingScenarioBlend};
}
bool Scene::SetLightingScenarioPreview(const std::string &fromId, const std::string &toId, float blend) {
    auto *from = FindLightingScenario(fromId);
    auto *to = FindLightingScenario(toId);
    if (!from || !to || !std::isfinite(blend))
        return false;
    m_lightingScenarioPreview = true;
    m_lightingScenarioPreviewFrom = from->id;
    m_lightingScenarioPreviewTo = to->id;
    m_lightingScenarioPreviewBlend = std::clamp(blend, 0.f, 1.f);
    return true;
}
void Scene::ClearLightingScenarioPreview() {
    m_lightingScenarioPreview = false;
    m_lightingScenarioPreviewFrom.clear();
    m_lightingScenarioPreviewTo.clear();
    m_lightingScenarioPreviewBlend = 0;
}
bool Scene::BeginLightingScenarioBlend(const std::string &fromId, const std::string &toId) {
    auto *from = FindLightingScenario(fromId);
    auto *to = FindLightingScenario(toId);
    if (m_paused || !from || !to)
        return false;
    // Preparation happens at load, never at an animation keyframe. Renderless
    // tools/tests validate the files once here because they have no GPU cache.
    if (auto *renderer = GetSystem<MeshRenderer3DSystem>()) {
        if (!renderer->LightingScenarioProbesReady(from->id) ||
            !renderer->LightingScenarioProbesReady(to->id))
            return false;
    } else {
        std::string error;
        if (!LightingScenarioDataAvailable(*from, error) ||
            (from != to && !LightingScenarioDataAvailable(*to, error)))
            return false;
    }
    ClearLightingScenarioPreview();
    m_lightingScenarioFrom = from->id;
    m_lightingScenarioTo = to->id;
    m_lightingScenarioBlend = m_lightingScenarioElapsed = 0;
    m_lightingScenarioQueued.clear();
    m_lightingScenarioTransitioning = false;
    m_lightingScenarioManual = true;
    return true;
}
bool Scene::SetLightingScenarioBlend(float blend) {
    if (!m_lightingScenarioManual || m_paused || !std::isfinite(blend))
        return false;
    m_lightingScenarioBlend = std::clamp(blend, 0.f, 1.f);
    return true;
}
void Scene::EndLightingScenarioBlend() {
    m_lightingScenarioManual = false;
    // Interior values stay visible when an animation is stopped. Endpoints can
    // collapse to one input, which also permits subsequent timed transitions.
    if (m_lightingScenarioBlend <= 0)
        m_lightingScenarioTo = m_lightingScenarioFrom;
    else if (m_lightingScenarioBlend >= 1) {
        m_lightingScenarioFrom = m_lightingScenarioTo;
        m_lightingScenarioBlend = 0;
    }
}
bool Scene::TransitionLightingScenario(const std::string &idOrName, float seconds) {
    auto *target = FindLightingScenario(idOrName);
    if (!target || !std::isfinite(seconds) || seconds < 0)
        return false;
    // Renderer-backed scenes use preloaded readiness: a gameplay transition must
    // never scan meshes, hash sky images, or capture reflection probes.
    if (auto *renderer = GetSystem<MeshRenderer3DSystem>()) {
        if (!renderer->LightingScenarioProbesReady(target->id))
            return false;
    } else {
        std::string readinessError;
        if (!LightingScenarioDataAvailable(*target, readinessError))
            return false;
    }
    ClearLightingScenarioPreview();
    auto current = GetLightingScenarioBlend();
    if (!current.from)
        return false;
    // A timed request can take over an animator's interior weight without a
    // jump. A third target completes the existing pair before being queued.
    const bool activePair = m_lightingScenarioTransitioning ||
                            (current.from != current.to && current.blend > 0 && current.blend < 1);
    m_lightingScenarioManual = false;
    if (seconds && activePair && target->id != current.from->id && target->id != current.to->id) {
        // Two resident inputs per draw: finish the current blend before a third state.
        m_lightingScenarioQueued = target->id;
        m_lightingScenarioQueuedDuration = seconds;
        if (!m_lightingScenarioTransitioning) {
            m_lightingScenarioStart = m_lightingScenarioBlend;
            m_lightingScenarioEnd = 1;
            m_lightingScenarioElapsed = 0;
            m_lightingScenarioDuration = seconds;
            m_lightingScenarioTransitioning = true;
        }
        return true;
    }
    m_lightingScenarioQueued.clear();
    if (!seconds) {
        m_lightingScenarioFrom = m_lightingScenarioTo = target->id;
        m_lightingScenarioBlend = 0;
        m_lightingScenarioTransitioning = false;
        return true;
    }
    if (!activePair) {
        const auto *endpoint = current.blend >= 1 ? current.to : current.from;
        m_lightingScenarioFrom = endpoint->id;
        m_lightingScenarioTo = target->id;
        m_lightingScenarioBlend = 0;
        if (endpoint->id == target->id)
            return true;
        m_lightingScenarioEnd = 1;
    } else
        m_lightingScenarioEnd = target->id == current.from->id ? 0.f : 1.f;
    m_lightingScenarioStart = m_lightingScenarioBlend;
    m_lightingScenarioElapsed = 0;
    m_lightingScenarioDuration = seconds;
    m_lightingScenarioTransitioning = true;
    return true;
}
void Scene::UpdateLightingScenarioTransition(float dt) {
    if (m_paused || !m_lightingScenarioTransitioning || !std::isfinite(dt) || dt <= 0)
        return;
    m_lightingScenarioElapsed = std::min(m_lightingScenarioDuration, m_lightingScenarioElapsed + dt);
    float t = m_lightingScenarioElapsed / std::max(m_lightingScenarioDuration, .0001f);
    // Smooth endpoints without changing the linear-space irradiance blend itself.
    float eased = t * t * (3 - 2 * t);
    m_lightingScenarioBlend = glm::mix(m_lightingScenarioStart, m_lightingScenarioEnd, eased);
    if (t < 1)
        return;
    m_lightingScenarioFrom = m_lightingScenarioTo =
        m_lightingScenarioEnd >= 1 ? m_lightingScenarioTo : m_lightingScenarioFrom;
    m_lightingScenarioBlend = 0;
    m_lightingScenarioTransitioning = false;
    if (!m_lightingScenarioQueued.empty()) {
        auto queued = m_lightingScenarioQueued;
        auto duration = m_lightingScenarioQueuedDuration;
        m_lightingScenarioQueued.clear();
        TransitionLightingScenario(queued, duration);
    }
}
} // namespace Canis
