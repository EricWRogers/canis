#include <Canis/AssetManager.hpp>
#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Scene.hpp>
#include <SDL3/SDL.h>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <imgui.h>
#include <map>
#include <set>
namespace Canis {
namespace {
void DisabledWrapped(const char *text) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}
bool ScenarioCombo(const char *label, Scene &scene, std::string &id) {
    auto *selected = scene.FindLightingScenario(id);
    bool changed = false;
    if (ImGui::BeginCombo(label, selected ? selected->name.c_str() : "Choose scenario")) {
        for (const auto &scenario : scene.GetLightingScenarios()) {
            ImGui::PushID(scenario.id.c_str());
            if (ImGui::Selectable(scenario.name.c_str(), scenario.id == id)) {
                id = scenario.id;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}
std::string UniqueScenarioName(const Scene &scene, const std::string &base) {
    std::string name = base;
    int suffix = 2;
    while (scene.FindLightingScenario(name))
        name = base + " " + std::to_string(suffix++);
    return name;
}
} // namespace
void Editor::DrawCloudVolume() {
    ImGui::PushFont(nullptr, 18.f);
    bool open = ImGui::CollapsingHeader("3D clouds");
    ImGui::PopFont();
    if (!open)
        return;
    auto &clouds = m_scene->GetCloudVolumeSettings();
    ImGui::BeginDisabled(m_mode != EditorMode::EDIT || m_lightingScenarioBakeRunning);
    ImGui::PushItemWidth(std::max(100.f, ImGui::GetContentRegionAvail().x - 160.f));
    bool changed = ImGui::Checkbox("Enable 3D clouds", &clouds.enabled);
    ImGui::BeginDisabled(!clouds.enabled);
    changed |= ImGui::DragFloat("Base height (m)", &clouds.baseHeight, 1.f, -1000, 10000);
    changed |= ImGui::DragFloat("Thickness (m)", &clouds.thickness, 1.f, 1, 1000);
    changed |= ImGui::DragFloat2("Center X/Z", &clouds.center.x, 1.f);
    changed |= ImGui::DragFloat2("Half size X/Z", &clouds.halfExtents.x, 5.f, 1, 20000);
    changed |= ImGui::DragFloat("Noise scale", &clouds.noiseScale, .0001f, .0001f, .1f, "%.4f");
    changed |= ImGui::DragFloat2("Wind X/Z (m/s)", &clouds.windVelocity.x, .1f, -100, 100);
    changed |= ImGui::DragFloat("Extinction", &clouds.extinction, .001f, .001f, 1, "%.3f");
    changed |= ImGui::SliderInt("Ray steps", &clouds.steps, 8, 64);
    ImGui::EndDisabled();
    DisabledWrapped("Cloud coverage and density are stored with each lighting scenario below. Wind pauses "
                    "with the scene.");
    DisabledWrapped("More ray steps give smoother cloud edges and cost more GPU time. Rebake scenarios after "
                    "changing the cloud layer so reflections match.");
    if (changed) {
        clouds.Sanitize();
        if (auto *renderer = m_scene->GetSystem<MeshRenderer3DSystem>())
            renderer->InvalidateLightingProbes();
        m_lightingScenarioNextCheck = 0;
        NotifySceneChanged();
    }
    ImGui::PopItemWidth();
    ImGui::EndDisabled();
    ImGui::Separator();
}
void Editor::StartLightingScenarioBake(bool all) {
    if (m_lightingScenarioBakeRunning || m_environmentBakeRunning || m_mode != EditorMode::EDIT || !m_scene)
        return;
    auto definitions = m_scene->GetLightingScenarios();
    if (definitions.empty())
        return;
    auto snapshot = CaptureLightingBakeScene(*m_scene);
    snapshot["BakeSourceScenePath"] = m_scene->GetPath();
    LightingBakeOptions options;
    options.vertexSpacing = m_lightingScenarioSpacing;
    options.indirectSamples = m_lightingScenarioSamples;
    options.cancel = &m_lightingScenarioBakeCancel;
    if (!all)
        options.selectedScenarioIds = {m_selectedLightingScenario};
    auto directory = std::filesystem::path("assets/Baked") /
                     std::filesystem::path(m_scene->GetPath()).stem().string() / "scenarios";
    m_lightingScenarioBakeCancel = false;
    m_lightingScenarioBakeRunning = true;
    m_lightingScenarioStatus.clear();
    m_scene->ClearLightingScenarioPreview();
    m_lightingScenarioPreviewEnabled = false;
    {
        std::lock_guard lock(m_lightingScenarioBakeMutex);
        m_lightingScenarioBakeProgress = "Preparing shared lighting samples...";
    }
    m_lightingScenarioBakeJob =
        std::async(std::launch::async, [this, snapshot, directory, definitions, options]() mutable {
            return BakeLightingScenarios(snapshot, directory, definitions, options,
                                         [this](const std::string &message) {
                                             std::lock_guard lock(m_lightingScenarioBakeMutex);
                                             m_lightingScenarioBakeProgress = message;
                                         });
        });
}
void Editor::UpdateLightingScenarioBake() {
    if (!m_lightingScenarioBakeRunning || !m_lightingScenarioBakeJob.valid())
        return;
    if (m_lightingScenarioBakeJob.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;
    if (m_mode != EditorMode::EDIT) {
        m_lightingScenarioStatus = "Bake finished. Return to Edit mode to apply.";
        return;
    }
    m_lightingScenarioBakeRunning = false;
    try {
        auto result = m_lightingScenarioBakeJob.get();
        if (result.scene["BakeSourceScenePath"].as<std::string>("") != m_scene->GetPath()) {
            m_lightingScenarioStatus = "Bake finished for another scene; current scene was not changed.";
            return;
        }
        const auto currentHash = LightingScenarioSourceFingerprint(CaptureLightingBakeScene(*m_scene));
        bool matching =
            std::all_of(result.scenarios.begin(), result.scenarios.end(), [&](const LightingScenario &s) {
                auto *current = m_scene->FindLightingScenario(s.id);
                return current && LightingScenarioSettingsFingerprint(*current) ==
                                      LightingScenarioSettingsFingerprint(s);
            });
        bool matchingGeometry =
            std::any_of(result.scenarios.begin(), result.scenarios.end(),
                        [&](const LightingScenario &s) { return s.sourceFingerprint == currentHash; });
        if (!matching || !matchingGeometry ||
            result.scenarios.size() != m_scene->GetLightingScenarios().size()) {
            m_lightingScenarioStatus =
                "Scene changed while baking. Result was saved but not applied; bake the current scene again.";
            return;
        }
        ApplyLightingBake(*m_scene, result);
        m_scene->SetLightingScenarioSources(result.scene["Environment"]["LightingScenarioSources"]);
        m_scene->GetLightingScenarios() = result.scenarios;
        m_scene->ResetLightingScenarioRuntime();
        std::string error;
        if (auto *renderer = m_scene->GetSystem<MeshRenderer3DSystem>()) {
            renderer->InvalidateStaticShadowCache();
            renderer->InvalidateLightingProbes();
            if (!renderer->PrepareLightingScenarios(true, error))
                m_lightingScenarioStatus = "Surface lighting baked. Probe preparation: " + error;
        }
        if (error.empty())
            m_lightingScenarioStatus =
                "Bake complete. Surface lighting and probe captures are ready. Save the scene.";
        m_lightingScenarioNextCheck = 0;
        NotifySceneChanged();
    } catch (const std::exception &error) {
        m_lightingScenarioStatus = m_lightingScenarioBakeCancel
                                       ? "Bake canceled. Previous baked data retained."
                                       : "Bake failed: " + std::string(error.what());
    }
}
void Editor::DrawLightingScenarios() {
    ImGui::PushFont(nullptr, 18.f);
    bool open = ImGui::CollapsingHeader("Lighting scenarios", ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopFont();
    if (!open)
        return;
    auto &definitions = m_scene->GetLightingScenarios();
    const auto path = m_scene->GetPath();
    if (m_lightingScenarioCheckedPath != path) {
        m_lightingScenarioCheckedPath = path;
        m_lightingScenarioSourceHash.clear();
        m_lightingScenarioAuthoringHashes.clear();
        m_lightingScenarioSettingsHashes.clear();
        m_lightingScenarioValidationErrors.clear();
        m_lightingScenarioQualities.clear();
        m_lightingScenarioNextCheck = 0;
        m_selectedLightingScenario.clear();
        m_lightingScenarioPreviewEnabled = false;
    }
    if (!definitions.empty() && !m_scene->FindLightingScenario(m_selectedLightingScenario))
        m_selectedLightingScenario = definitions.front().id;
    if (m_lightingScenarioCheckJob.valid() &&
        m_lightingScenarioCheckJob.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto checked = m_lightingScenarioCheckJob.get();
            if (checked.scenePath == path) {
                m_lightingScenarioSourceHash = checked.sourceHash;
                m_lightingScenarioValidationErrors = checked.errors;
                m_lightingScenarioAuthoringHashes = checked.authoringHashes;
                m_lightingScenarioSettingsHashes = checked.settingsHashes;
                m_lightingScenarioQualities = checked.qualities;
            }
        } catch (...) {
            m_lightingScenarioSourceHash.clear();
        }
    }
    const double now = SDL_GetTicks() / 1000.0;
    // Hash source assets off-thread; never scan large files once per ImGui frame.
    if (!definitions.empty() && m_mode == EditorMode::EDIT && !m_lightingScenarioBakeRunning &&
        !m_lightingScenarioCheckJob.valid() && now >= m_lightingScenarioNextCheck &&
        !ImGui::IsAnyItemActive()) {
        auto snapshot = CaptureLightingBakeScene(*m_scene);
        m_lightingScenarioNextCheck = now + 2;
        m_lightingScenarioCheckJob = std::async(std::launch::async, [snapshot, path, definitions] {
            LightingScenarioValidation result;
            result.scenePath = path;
            result.sourceHash = LightingScenarioSourceFingerprint(snapshot);
            for (const auto &scenario : definitions) {
                result.authoringHashes[scenario.id] = LightingScenarioAuthoringFingerprint(scenario);
                result.settingsHashes[scenario.id] = LightingScenarioSettingsFingerprint(scenario);
                std::string error;
                if (!LightingScenarioDataAvailable(scenario, error))
                    result.errors[scenario.id] = error;
                if (!scenario.manifestPath.empty()) {
                    try {
                        auto q = YAML::LoadFile(scenario.manifestPath)["Quality"];
                        result.qualities[scenario.id] = {q["VertexSpacing"].as<float>(1.8f),
                                                         q["IndirectSamples"].as<int>(4)};
                    } catch (...) {
                    }
                }
            }
            return result;
        });
    }
    ImGui::TextWrapped("Store different lighting for the same scene. Preview blends without changing "
                       "authored lights or geometry.");
    const bool editing = m_mode == EditorMode::EDIT && !m_lightingScenarioBakeRunning;
    ImGui::BeginDisabled(!editing);
    if (ImGui::Button("Add from current lighting")) {
        auto scenario = m_scene->CaptureLightingScenario(
            UniqueScenarioName(*m_scene, definitions.empty() ? "Dusk" : "Scenario"));
        m_selectedLightingScenario = scenario.id;
        definitions.push_back(scenario);
        if (m_scene->GetInitialLightingScenario().empty())
            m_scene->SetInitialLightingScenario(scenario.id);
        NotifySceneChanged();
    }
    auto *selected = m_scene->FindLightingScenario(m_selectedLightingScenario);
    if (selected) {
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) {
            auto copy = *selected;
            copy.id = std::to_string(uint64_t(UUID()));
            copy.name = UniqueScenarioName(*m_scene, selected->name + " copy");
            copy.manifestPath.clear();
            copy.sourceFingerprint.clear();
            copy.settingsFingerprint.clear();
            copy.probeCachePaths.clear();
            m_selectedLightingScenario = copy.id;
            definitions.push_back(copy);
            NotifySceneChanged();
        }
    }
    ImGui::EndDisabled();
    if (definitions.empty()) {
        DisabledWrapped("Add a scenario, duplicate it, then edit and bake both.");
        ImGui::Separator();
        return;
    }
    ImGui::PushItemWidth(std::max(100.f, ImGui::GetContentRegionAvail().x - 160.f));
    ScenarioCombo("Edit scenario", *m_scene, m_selectedLightingScenario);
    selected = m_scene->FindLightingScenario(m_selectedLightingScenario);
    ImGui::BeginDisabled(!editing);
    if (selected) {
        char name[128];
        std::snprintf(name, sizeof(name), "%s", selected->name.c_str());
        if (ImGui::InputText("Name", name, sizeof(name)) && name[0]) {
            auto *same = m_scene->FindLightingScenario(name);
            if (!same || same == selected) {
                selected->name = name;
                NotifySceneChanged();
            } else
                DisabledWrapped("That name is already used. Choose a unique scenario name.");
        }
        bool changed = false;
        changed |= ImGui::ColorEdit3("Ambient color", &selected->ambientLight.x);
        changed |= ImGui::DragFloat("Ambient intensity", &selected->ambientIntensity, .01f, 0, 8);
        changed |= ImGui::Checkbox("Sun enabled", &selected->sunEnabled);
        changed |= ImGui::ColorEdit3("Sun color", &selected->sunColor.x);
        changed |= ImGui::DragFloat("Sun intensity", &selected->sunIntensity, .01f, 0, 16);
        Vector3 direction = selected->sunDirection;
        if (ImGui::DragFloat3("Sun direction", &direction.x, .01f, -1, 1) && glm::length(direction) > .001f) {
            selected->sunDirection = glm::normalize(direction);
            changed = true;
        }
        changed |= ImGui::DragFloat("Sky intensity", &selected->skyboxIntensity, .01f, 0, 8);
        changed |= ImGui::SliderFloat("Cloud coverage", &selected->cloudCoverage, 0, 1, "%.2f");
        changed |= ImGui::DragFloat("Cloud density", &selected->cloudDensity, .01f, 0, 4, "%.2f");
        if (!m_scene->GetCloudVolumeSettings().enabled)
            DisabledWrapped("Enable the 3D clouds layer above to show this scenario's clouds.");
        std::string skyLabel = selected->skyboxPath.empty()
                                   ? "Drop skybox asset"
                                   : std::filesystem::path(selected->skyboxPath).stem().string();
        ImGui::Button((skyLabel + "##ScenarioSky").c_str(), ImVec2(180, 0));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG")) {
                auto dropped = *static_cast<const AssetDragData *>(payload->Data);
                auto droppedPath = AssetManager::GetPath(dropped.uuid);
                if (auto *meta = AssetManager::GetMetaFile(droppedPath);
                    meta && meta->type == MetaFileAsset::FileType::SKYBOX) {
                    selected->skyboxUUID = uint64_t(dropped.uuid);
                    selected->skyboxPath = droppedPath;
                    changed = true;
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear sky")) {
            selected->skyboxUUID = 0;
            selected->skyboxPath.clear();
            changed = true;
        }
        if (changed)
            NotifySceneChanged();
        DisabledWrapped("Baked local lights and probe positions are shared by all scenarios.");
        const bool missing = selected->manifestPath.empty();
        const bool checked = m_lightingScenarioAuthoringHashes.contains(selected->id);
        const bool authoringChanged = checked && m_lightingScenarioAuthoringHashes[selected->id] !=
                                                     LightingScenarioAuthoringFingerprint(*selected);
        const bool settingsChanged = !missing && checked &&
                                     (authoringChanged || selected->settingsFingerprint !=
                                                              m_lightingScenarioSettingsHashes[selected->id]);
        const bool geometryChanged = !missing && !m_lightingScenarioSourceHash.empty() &&
                                     selected->sourceFingerprint != m_lightingScenarioSourceHash;
        bool qualityChanged =
            !missing && m_lightingScenarioQualities.contains(selected->id) &&
            (std::abs(m_lightingScenarioQualities[selected->id].first - m_lightingScenarioSpacing) > .0001f ||
             m_lightingScenarioQualities[selected->id].second != m_lightingScenarioSamples);
        ImGui::Text("Bake status: %s",
                    missing                                            ? "Not baked"
                    : settingsChanged                                  ? "Needs rebake (lighting changed)"
                    : geometryChanged                                  ? "Needs rebake (scene changed)"
                    : qualityChanged                                   ? "Needs rebake (quality changed)"
                    : !checked || m_lightingScenarioSourceHash.empty() ? "Checking scene..."
                    : m_lightingScenarioValidationErrors.contains(selected->id) ? "Data unavailable"
                                                                                : "Ready");
        if (!missing && m_lightingScenarioValidationErrors.contains(selected->id))
            ImGui::TextWrapped("Data status: %s", m_lightingScenarioValidationErrors[selected->id].c_str());
        ImGui::DragFloat("Sample spacing (m)", &m_lightingScenarioSpacing, .1f, .1f, 4, "%.1f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Smaller spacing adds lighting detail, vertices, bake time and memory.");
        ImGui::SliderInt("Indirect samples", &m_lightingScenarioSamples, 0, 32);
        ImGui::BeginDisabled(m_environmentBakeRunning);
        if (ImGui::Button("Bake selected"))
            StartLightingScenarioBake(false);
        ImGui::SameLine();
        if (ImGui::Button("Bake all scenarios"))
            StartLightingScenarioBake(true);
        ImGui::EndDisabled();
        if (ImGui::Button("Remove scenario"))
            ImGui::OpenPopup("Remove lighting scenario?");
        if (ImGui::BeginPopupModal("Remove lighting scenario?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Remove this scenario definition? Weather components referencing it will need "
                               "another target.");
            if (ImGui::Button("Remove")) {
                auto id = selected->id;
                definitions.erase(std::remove_if(definitions.begin(), definitions.end(),
                                                 [&](const LightingScenario &s) { return s.id == id; }),
                                  definitions.end());
                m_scene->ClearLightingScenarioPreview();
                m_lightingScenarioPreviewEnabled = false;
                if (m_scene->GetInitialLightingScenario() == id)
                    m_scene->SetInitialLightingScenario(definitions.empty() ? "" : definitions.front().id);
                m_selectedLightingScenario = definitions.empty() ? "" : definitions.front().id;
                NotifySceneChanged();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    auto startup = m_scene->GetInitialLightingScenario();
    if (ScenarioCombo("Startup scenario", *m_scene, startup)) {
        m_scene->SetInitialLightingScenario(startup);
        NotifySceneChanged();
    }
    ImGui::EndDisabled();
    if (m_lightingScenarioBakeRunning) {
        std::string progress;
        {
            std::lock_guard lock(m_lightingScenarioBakeMutex);
            progress = m_lightingScenarioBakeProgress;
        }
        ImGui::TextWrapped("%s", progress.c_str());
        if (ImGui::Button("Cancel scenario bake"))
            m_lightingScenarioBakeCancel = true;
    }
    if (!m_lightingScenarioStatus.empty())
        ImGui::TextWrapped("%s", m_lightingScenarioStatus.c_str());
    if (definitions.size() >= 2) {
        if (!m_scene->FindLightingScenario(m_lightingScenarioPreviewFrom))
            m_lightingScenarioPreviewFrom = definitions[0].id;
        if (!m_scene->FindLightingScenario(m_lightingScenarioPreviewTo))
            m_lightingScenarioPreviewTo = definitions[1].id;
        ImGui::SeparatorText("Temporary preview");
        const bool animationOwnsPreview = m_animationLightingPreview.captured;
        ImGui::BeginDisabled(!editing || animationOwnsPreview);
        bool previewChanged = ScenarioCombo("From", *m_scene, m_lightingScenarioPreviewFrom);
        previewChanged |= ScenarioCombo("To", *m_scene, m_lightingScenarioPreviewTo);
        ImGui::EndDisabled();
        auto *previewA = m_scene->FindLightingScenario(m_lightingScenarioPreviewFrom);
        auto *previewB = m_scene->FindLightingScenario(m_lightingScenarioPreviewTo);
        auto *previewRenderer = m_scene->GetSystem<MeshRenderer3DSystem>();
        auto readyForPreview = [&](const LightingScenario *s) {
            return s && !s->manifestPath.empty() && m_lightingScenarioAuthoringHashes.contains(s->id) &&
                   m_lightingScenarioAuthoringHashes[s->id] == LightingScenarioAuthoringFingerprint(*s) &&
                   s->settingsFingerprint == m_lightingScenarioSettingsHashes[s->id] &&
                   !m_lightingScenarioSourceHash.empty() &&
                   s->sourceFingerprint == m_lightingScenarioSourceHash &&
                   !m_lightingScenarioValidationErrors.contains(s->id);
        };
        bool previewReady = readyForPreview(previewA) && readyForPreview(previewB) && previewRenderer &&
                            previewRenderer->LightingScenarioProbesReady(previewA->id) &&
                            previewRenderer->LightingScenarioProbesReady(previewB->id);
        if (!animationOwnsPreview && !previewReady && m_lightingScenarioPreviewEnabled) {
            m_lightingScenarioPreviewEnabled = false;
            m_scene->ClearLightingScenarioPreview();
        }
        if (!animationOwnsPreview && m_mode != EditorMode::EDIT) {
            m_lightingScenarioPreviewEnabled = false;
            m_scene->ClearLightingScenarioPreview();
        }
        ImGui::BeginDisabled(!editing || !previewReady || animationOwnsPreview);
        previewChanged |= ImGui::SliderFloat("Blend", &m_lightingScenarioPreviewAmount, 0, 1, "%.2f");
        if (ImGui::Checkbox("Preview in viewport", &m_lightingScenarioPreviewEnabled))
            previewChanged = true;
        if (!animationOwnsPreview && m_lightingScenarioPreviewEnabled &&
            (previewChanged || !m_scene->HasLightingScenarioPreview()))
            m_scene->SetLightingScenarioPreview(m_lightingScenarioPreviewFrom, m_lightingScenarioPreviewTo,
                                                m_lightingScenarioPreviewAmount);
        if (!animationOwnsPreview && previewChanged && !m_lightingScenarioPreviewEnabled)
            m_scene->ClearLightingScenarioPreview();
        ImGui::EndDisabled();
        if (animationOwnsPreview)
            DisabledWrapped("The Animation window controls the temporary lighting preview. Disable its Preview or close the window to restore these controls.");
        if (!previewReady)
            DisabledWrapped("Bake both scenarios to enable the viewport blend preview.");
        DisabledWrapped("Preview is temporary; Startup scenario controls saved gameplay lighting.");
    }
    if (auto *renderer = m_scene->GetSystem<MeshRenderer3DSystem>();
        renderer && !renderer->LightingScenariosReady()) {
        ImGui::TextWrapped("Probe captures: %s", renderer->LightingScenariosError().c_str());
        bool surfacesReady =
            std::all_of(definitions.begin(), definitions.end(), [&](const LightingScenario &s) {
                return !s.manifestPath.empty() && m_lightingScenarioAuthoringHashes.contains(s.id) &&
                       m_lightingScenarioAuthoringHashes[s.id] == LightingScenarioAuthoringFingerprint(s) &&
                       s.settingsFingerprint == m_lightingScenarioSettingsHashes[s.id] &&
                       !m_lightingScenarioSourceHash.empty() &&
                       s.sourceFingerprint == m_lightingScenarioSourceHash;
            });
        ImGui::BeginDisabled(!editing || m_lightingScenarioBakeRunning || !surfacesReady);
        if (ImGui::Button("Prepare scenario probe captures")) {
            std::string error;
            m_lightingScenarioStatus = renderer->PrepareLightingScenarios(true, error)
                                           ? "Scenario probe captures are ready."
                                           : error;
        }
        ImGui::EndDisabled();
    }
    ImGui::PopItemWidth();
    ImGui::Separator();
}
} // namespace Canis
