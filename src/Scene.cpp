#include <Canis/Time.hpp>
#include <Canis/AudioComponents.hpp>
#include <Canis/Audio.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Profiler.hpp>
#include <Canis/RenderMetrics.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/App.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Components.hpp>
#include <Canis/CombineModelLoad.hpp>
#include <Canis/GraphicsSettings.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/System.hpp>
#include <Canis/Time.hpp>
#include <Canis/Window.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/ECS/Systems/SpriteAnimationSystem.hpp>
#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>
#include <Canis/ECS/Systems/UIInteractionSystem.hpp>
#include <Canis/ECS/Systems/AnimatorSystem.hpp>
#include <Canis/ECS/Systems/AnimationPlayerSystem.hpp>
#include <Canis/ECS/Systems/ModelAnimation3DSystem.hpp>
#include <Canis/ECS/Systems/BoneAttachmentSystem.hpp>
#include <Canis/ECS/Systems/MeshRenderer3DSystem.hpp>
#include <Canis/ECS/Systems/JoltPhysics3DSystem.hpp>
#include <Canis/ECS/Systems/NavMeshSystem.hpp>
#include <Canis/ECS/Systems/CloudNavSystem.hpp>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <filesystem>
#include <cstdlib>
#include <unordered_set>
#include <SDL3/SDL_timer.h>

namespace Canis
{
    namespace
    {
        UUID ResolveEnvironmentAssetUUID(const YAML::Node &node)
        {
            if (!node)
                return UUID(0);

            if (node.IsMap())
            {
                if (YAML::Node uuidNode = node["uuid"])
                {
                    const UUID uuid = uuidNode.as<uint64_t>(0);
                    if ((uint64_t)uuid != 0)
                        return uuid;
                }

                if (YAML::Node pathNode = node["path"])
                {
                    const std::string path = pathNode.as<std::string>("");
                    if (!path.empty())
                    {
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                            return meta->uuid;
                    }
                }

                return UUID(0);
            }

            if (node.IsScalar())
            {
                const std::string rawValue = node.as<std::string>("");
                if (rawValue.empty())
                    return UUID(0);

                const bool isNumeric = std::all_of(rawValue.begin(), rawValue.end(), [](unsigned char c)
                    { return std::isdigit(c) != 0; });
                if (isNumeric)
                    return (UUID)std::stoull(rawValue);

                if (MetaFileAsset *meta = AssetManager::GetMetaFile(rawValue))
                    return meta->uuid;
            }

            return UUID(0);
        }

        bool HasHierarchyParent(Entity *_entity)
        {
            if (_entity == nullptr)
                return false;

            if (_entity->HasComponent<RectTransform>() && _entity->GetComponent<RectTransform>().parent != nullptr)
                return true;

            if (_entity->HasComponent<Transform>() && _entity->GetComponent<Transform>().parent != nullptr)
                return true;

            return false;
        }
    }

    void Scene::Init(App *_app, Window *_window, InputManager *_inputManger)
    {
        app = _app;
        m_window = _window;
        m_inputManager = _inputManger;
        m_paused = false;
        ClearLastRenderCamera();
        ClearEditorCameraOverrides();

        // TODO resizing breaks components
        // I think this is fixed but needs more testing
        m_entities.reserve(1000);
    }

    void Scene::SetEditorCamera3DOverride(const Matrix4 &_view, const Matrix4 &_projection)
    {
        m_editorCamera3DOverrideEnabled = true;
        m_editorCamera3DView = _view;
        m_editorCamera3DProjection = _projection;
    }

    void Scene::SetEditorCamera2DOverride(const Matrix4 &_cameraMatrix, const Vector2 &_position)
    {
        m_editorCamera2DOverrideEnabled = true;
        m_editorCamera2DMatrix = _cameraMatrix;
        m_editorCamera2DPosition = _position;
    }

    void Scene::ClearEditorCameraOverrides()
    {
        m_editorCamera3DOverrideEnabled = false;
        m_editorCamera2DOverrideEnabled = false;
    }

    void Scene::SetLastRenderCamera(const Matrix4 &_view, const Matrix4 &_projection, const Vector3 &_cameraPosition, float _nearClip, float _farClip)
    {
        m_lastRenderCameraValid = true;
        m_lastRenderView = _view;
        m_lastRenderProjection = _projection;
        m_lastRenderCameraPosition = _cameraPosition;
        m_lastRenderCameraNearClip = _nearClip;
        m_lastRenderCameraFarClip = _farClip;
    }

    void Scene::ClearLastRenderCamera()
    {
        m_lastRenderCameraValid = false;
        m_lastRenderView = Matrix4(1.0f);
        m_lastRenderProjection = Matrix4(1.0f);
        m_lastRenderCameraPosition = Vector3(0.0f);
        m_lastRenderCameraNearClip = 0.1f;
        m_lastRenderCameraFarClip = 100.0f;
    }

    bool Scene::Raycast(const Vector3 &_origin, const Vector3 &_direction, RaycastHit &_hit, float _maxDistance, u32 _mask)
    {
        if (JoltPhysics3DSystem *physicsSystem = GetSystem<JoltPhysics3DSystem>())
            return physicsSystem->Raycast(_origin, _direction, _hit, _maxDistance, _mask);

        _hit = RaycastHit{};
        return false;
    }

    bool Scene::Raycast(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance, u32 _mask)
    {
        RaycastHit hit = {};
        return Raycast(_origin, _direction, hit, _maxDistance, _mask);
    }

    std::vector<RaycastHit> Scene::RaycastAll(const Vector3 &_origin, const Vector3 &_direction, float _maxDistance, u32 _mask)
    {
        if (JoltPhysics3DSystem *physicsSystem = GetSystem<JoltPhysics3DSystem>())
            return physicsSystem->RaycastAll(_origin, _direction, _maxDistance, _mask);

        return {};
    }

    bool Scene::BuildNavMesh(Entity& _surfaceEntity)
    {
        if (JoltPhysics3DSystem* physicsSystem = GetSystem<JoltPhysics3DSystem>())
            physicsSystem->Update(m_registry, 0.0f);
        if (NavMeshSystem* navMeshSystem = GetSystem<NavMeshSystem>())
            return navMeshSystem->Build(m_registry, _surfaceEntity);
        return false;
    }

    void Scene::InvalidateNavMesh(Entity& _surfaceEntity)
    {
        if (NavMeshSystem* navMeshSystem = GetSystem<NavMeshSystem>())
            navMeshSystem->Invalidate(_surfaceEntity);
    }

    std::vector<Vector3> Scene::FindNavMeshPath(
        Entity& _surfaceEntity,
        const Vector3& _start,
        const Vector3& _destination)
    {
        if (NavMeshSystem* navMeshSystem = GetSystem<NavMeshSystem>())
            return navMeshSystem->FindPath(m_registry, _surfaceEntity, _start, _destination);
        return {};
    }

    std::size_t Scene::GetNavMeshPointCount(const Entity& _surfaceEntity) const
    {
        for (System* system : m_systems)
        {
            if (const NavMeshSystem* navMeshSystem = dynamic_cast<const NavMeshSystem*>(system))
                return navMeshSystem->GetPointCount(_surfaceEntity);
        }
        return 0u;
    }

    bool Scene::BuildCloudNav(Entity& _surfaceEntity)
    {
        if (JoltPhysics3DSystem* physicsSystem = GetSystem<JoltPhysics3DSystem>())
            physicsSystem->Update(m_registry, 0.0f);
        if (CloudNavSystem* cloudNavSystem = GetSystem<CloudNavSystem>())
            return cloudNavSystem->Build(m_registry, _surfaceEntity);
        return false;
    }

    void Scene::InvalidateCloudNav(Entity& _surfaceEntity)
    {
        if (CloudNavSystem* cloudNavSystem = GetSystem<CloudNavSystem>())
            cloudNavSystem->Invalidate(_surfaceEntity);
    }

    std::vector<Vector3> Scene::FindCloudNavPath(
        Entity& _surfaceEntity,
        const Vector3& _start,
        const Vector3& _destination)
    {
        if (CloudNavSystem* cloudNavSystem = GetSystem<CloudNavSystem>())
            return cloudNavSystem->FindPath(m_registry, _surfaceEntity, _start, _destination);
        return {};
    }

    std::size_t Scene::GetCloudNavPointCount(const Entity& _surfaceEntity) const
    {
        for (System* system : m_systems)
        {
            if (const CloudNavSystem* cloudNavSystem = dynamic_cast<const CloudNavSystem*>(system))
                return cloudNavSystem->GetPointCount(_surfaceEntity);
        }
        return 0u;
    }

    bool Scene::TryGetRayFromCamera(const Entity &_cameraEntity, const Vector2 &_screenPosition, Ray &_ray) const
    {
        if (m_window == nullptr ||
            !_cameraEntity.HasComponent<Camera>() ||
            !_cameraEntity.HasComponent<Transform>())
        {
            _ray = Ray{};
            return false;
        }

        const float screenWidth = static_cast<float>(m_window->GetScreenWidth());
        const float screenHeight = static_cast<float>(m_window->GetScreenHeight());
        if (screenWidth <= 0.0f || screenHeight <= 0.0f)
        {
            _ray = Ray{};
            return false;
        }

        const Camera &camera = _cameraEntity.GetComponent<Camera>();
        const Transform &transform = _cameraEntity.GetComponent<Transform>();

        const Matrix4 projection = glm::perspective(
            DEG2RAD * camera.fovDegrees,
            screenWidth / screenHeight,
            camera.nearClip,
            camera.farClip);

        const Vector3 eye = transform.GetGlobalPosition();
        const Matrix4 view = glm::lookAt(eye, eye + transform.GetForward(), transform.GetUp());
        const Vector4 viewport = Vector4(0.0f, 0.0f, screenWidth, screenHeight);

        const Vector3 nearPoint = glm::unProject(
            Vector3(_screenPosition.x, _screenPosition.y, 0.0f),
            view,
            projection,
            viewport);

        const Vector3 farPoint = glm::unProject(
            Vector3(_screenPosition.x, _screenPosition.y, 1.0f),
            view,
            projection,
            viewport);

        const Vector3 direction = farPoint - nearPoint;
        if (glm::length(direction) <= 0.000001f)
        {
            _ray = Ray{};
            return false;
        }

        _ray.origin = eye;
        _ray.direction = glm::normalize(direction);
        return true;
    }

    bool Scene::TryGetMouseRayFromCamera(const Entity &_cameraEntity, Ray &_ray) const
    {
        if (m_inputManager == nullptr)
        {
            _ray = Ray{};
            return false;
        }

        return TryGetRayFromCamera(_cameraEntity, m_inputManager->mouse, _ray);
    }

    void Scene::DrawDebugGizmoLine(const Vector3 &_start, const Vector3 &_end, const Color &_color)
    {
        m_debugGizmoLines.push_back(DebugGizmoLine{
            .start = _start,
            .end = _end,
            .color = _color
        });
    }

    void Scene::ClearDebugGizmoLines()
    {
        m_debugGizmoLines.clear();
    }

    void Scene::Update(float _deltaTime)
    {
        Profiler::Scope frameScope("Scene.Update", Profiler::Category::Other);
        ClearDebugGizmoLines();
        if (!m_paused && std::isfinite(_deltaTime) && _deltaTime > 0)
            m_cloudSimulationTime += _deltaTime;
        UpdateLightingScenarioTransition(_deltaTime);
        m_isUpdating = true;

        const auto updateSystem = [&](System* _system)
        {
            Profiler::Scope scope(_system->GetName(), Profiler::SystemCategory(_system->GetName()));
            const Uint64 start = SDL_GetTicksNS();
            _system->Update(m_registry, _deltaTime);
            const float elapsedMs = static_cast<float>(SDL_GetTicksNS() - start) / 1000000.0f;
            if (SystemTiming* timing = GetSystemTiming(_system))
                timing->updateMs = elapsedMs;
        };

        for (System* system : m_updateSystems)
        {
            if (system->UpdateAfterScripts() ||
                (m_paused && !system->UpdateWhenPaused()))
                continue;

            updateSystem(system);
        }
        // Run Ready only on entities created since the last frame.
        for (size_t i = 0; i < m_entitiesToReady.size(); ++i)
        {
            Entity* e = m_entitiesToReady[i].TryGet();
            if (e == nullptr || !e->Active())
                continue;

            const auto scripts = e->GetScripts();
            for (size_t j = 0; j < scripts.size() && e->Active(); ++j)
            {
                ScriptableEntity* se = scripts[j].TryGet();
                if (!se || se->m_onReadyCalled)
                    continue;

                se->m_onReadyCalled = true;
                se->Ready();
            }
        }
        m_entitiesToReady.clear();

        // Snapshot once: scripts attached during Update start on the next frame.
        // Handles invalidate immediately on removal, including later phases.
        std::vector<ScriptHandle<ScriptableEntity>> frameScripts;
        for (Entity* e : m_entities)
            if (e && e->Active())
                for (auto handle : e->GetScripts()) frameScripts.push_back(handle);
        for (int phase = 0; phase < static_cast<int>(ScriptUpdatePhase::Count); ++phase)
        {
            if (phase == static_cast<int>(ScriptUpdatePhase::Gameplay) && !m_paused && managedUpdate) {
                Profiler::Scope scope("C# Scripts.Update", Profiler::Category::Scripts);
                managedUpdate(_deltaTime);
            }
            for (auto handle : frameScripts)
            {
                auto* se = handle.TryGet();
                if (!se || !se->entity.Active() || !se->m_onReadyCalled ||
                    static_cast<int>(se->UpdatePhase()) != phase ||
                    (m_paused && !se->UpdateWhenPaused())) continue;
                Profiler::Scope scope(se->entity.GetName(), Profiler::Category::Scripts);
                se->Update(_deltaTime);
            }

        }
        if (!m_paused) {
            Profiler::Scope scope("Tween.Update", Profiler::Category::Other);
            tweens.Update(_deltaTime, Time::UnscaledDeltaTime());
        }
        // Animation state and model assets are commonly selected by scripts.
        // Evaluate post-script systems before rendering so a newly selected
        // skinned model never reaches the renderer with an invalid/old pose.
        for (System* system : m_updateSystems)
        {
            if (!system->UpdateAfterScripts() ||
                (m_paused && !system->UpdateWhenPaused()))
                continue;

            updateSystem(system);
        }

        {
            Profiler::Scope scope("Audio.Update", Profiler::Category::Audio);
            UpdateSceneAudio(*this);
        }
        m_isUpdating = false;
        FlushRetiredScripts();
        auto pendingDestroy = std::move(m_entitiesToDestroy); m_entitiesToDestroy.clear();
        for (const auto& handle : pendingDestroy) DestroyNow(handle);
    }

    void Scene::Render(float _deltaTime)
    {
        Render(_deltaTime, false);
    }

    void Scene::Render(float _deltaTime, bool _deferScreenSpaceUI)
    {
        if (!m_window) return;
        Profiler::Scope frameScope("Scene.Render", RenderMetrics::ProfileCategory());
        const int width = m_window->GetScreenWidth(), height = m_window->GetScreenHeight();
        const auto [renderWidth, renderHeight] = GraphicsRenderSize(width, height, GetGraphicsSettings());
        const bool scaled = !HasEditorCamera3DOverride() && !GetVRCamera().enabled &&
            (renderWidth != width || renderHeight != height);
        GLint draw = 0, read = 0, viewport[4] = {};
        if (scaled)
        {
            if (m_qualityRenderTarget.width != renderWidth || m_qualityRenderTarget.height != renderHeight)
                Debug::Log("GRAPHICS_RENDER_SIZE world=%dx%d ui=%dx%d", renderWidth, renderHeight, width, height);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
            glGetIntegerv(GL_VIEWPORT, viewport);
            EnsureRenderTarget(m_qualityRenderTarget, renderWidth, renderHeight);
            glBindFramebuffer(GL_FRAMEBUFFER, m_qualityRenderTarget.framebuffer);
            glViewport(0, 0, renderWidth, renderHeight);
            m_window->SetRenderSize(renderWidth, renderHeight);
            const auto clear = m_window->GetClearColor();
            glClearColor(clear.r, clear.g, clear.b, clear.a);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        //Canis::Debug::Log("Render Update %i", m_renderSystems.size());
        for (System* renderer : m_renderSystems)
        {
            Profiler::Scope scope(renderer->GetName(), RenderMetrics::ProfileCategory());
            const Uint64 start = SDL_GetTicksNS();
            if (auto* ui=dynamic_cast<SpriteRenderer2DSystem*>(renderer); ui && (_deferScreenSpaceUI || scaled))
            {
                // 2D scene coordinates retain their native logical layout even
                // when their world pass uses a smaller framebuffer viewport.
                if (scaled) m_window->SetRenderSize(width, height);
                ui->RenderWorldUI(m_registry, _deltaTime);
                if (scaled) m_window->SetRenderSize(renderWidth, renderHeight);
            }
            else
                renderer->Update(m_registry, _deltaTime);
            const float elapsedMs = static_cast<float>(SDL_GetTicksNS() - start) / 1000000.0f;
            if (SystemTiming* timing = GetSystemTiming(renderer))
                timing->renderMs = elapsedMs;
        }
        if (scaled)
        {
            m_window->SetRenderSize(width, height);
            BlitFramebuffer(m_qualityRenderTarget.framebuffer, renderWidth, renderHeight, draw, width, height);
            // Keep native-resolution depth consumers (post effects/world UI) aligned.
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_qualityRenderTarget.framebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
            glBlitFramebuffer(0, 0, renderWidth, renderHeight, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            if (!_deferScreenSpaceUI) CompositeScreenSpaceUI(draw, width, height);
        }
    }

    void Scene::CompositeScreenSpaceUI(unsigned int _framebuffer, int _width, int _height)
    {
        if (!m_window || _width <= 0 || _height <= 0) return;
        RenderMetrics::Scope timing("Screen-space UI composition");
        GLint draw=0,read=0,viewport[4];
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);
        glGetIntegerv(GL_VIEWPORT,viewport);
        const int width=m_window->GetScreenWidth(),height=m_window->GetScreenHeight();
        m_window->SetRenderSize(_width,_height);
        glBindFramebuffer(GL_FRAMEBUFFER,_framebuffer);glViewport(0,0,_width,_height);
        for (System* renderer : m_renderSystems)
        {
            if (auto* ui = dynamic_cast<SpriteRenderer2DSystem*>(renderer))
            {
                const Uint64 start = SDL_GetTicksNS();
                ui->RenderScreenSpaceUI(m_registry);
                if (SystemTiming* systemTiming = GetSystemTiming(renderer))
                    systemTiming->renderMs += static_cast<float>(SDL_GetTicksNS() - start) / 1000000.0f;
            }
        }
        m_window->SetRenderSize(width,height);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,draw);glBindFramebuffer(GL_READ_FRAMEBUFFER,read);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
    }

    void Scene::UpdateEditor()
    {
        Profiler::Scope frameScope("Scene.UpdateEditor", Profiler::Category::Editor);
        // Edit mode intentionally does not run game scripts. A small subset
        // of systems still needs to evaluate derived authoring data before
        // rendering and gizmo interaction. A zero delta initializes the
        // selected animation pose without advancing playback.
        for (System* system : m_updateSystems)
        {
            if (system != nullptr && system->UpdateInEditor())
            {
                Profiler::Scope scope(system->GetName(), Profiler::Category::Editor);
                system->Update(m_registry, 0.0f);
            }
        }
        if (auto* physics = GetSystem<JoltPhysics3DSystem>())
        {
            Profiler::Scope scope("Combined collider editor preview", Profiler::Category::Editor);
            physics->UpdateCombinedCollidersInEditor(m_registry);
        }
    }

    void Scene::InvokeScriptAction(ScriptableEntity& script, const std::function<void()>& action)
    {
        ScriptHandle<ScriptableEntity> alive(&script);
        if (!alive) return;
        InvokeComponentAction(action);
    }

    void Scene::InvokeComponentAction(const std::function<void()>& action)
    {
        ++m_scriptCallbackDepth;
        try { action(); }
        catch (...)
        {
            --m_scriptCallbackDepth;
            if (!m_isUpdating && !m_isLoadingEntityNodes && m_scriptCallbackDepth == 0) FlushRetiredScripts();
            throw;
        }
        --m_scriptCallbackDepth;
        if (!m_isUpdating && !m_isLoadingEntityNodes && m_scriptCallbackDepth == 0) FlushRetiredScripts();
    }

    ScriptableEntity* Scene::InvokeScriptCallback(ScriptableEntity* script, bool ready)
    {
        ScriptHandle<ScriptableEntity> handle(script);
        ++m_scriptCallbackDepth;
        try {
            if (ready) script->Ready(); else script->Create();
        } catch (...) {
            --m_scriptCallbackDepth;
            if (!m_isUpdating && !m_isLoadingEntityNodes && m_scriptCallbackDepth == 0) FlushRetiredScripts();
            throw;
        }
        --m_scriptCallbackDepth;
        if (!m_isUpdating && !m_isLoadingEntityNodes && m_scriptCallbackDepth == 0) FlushRetiredScripts();
        return handle.TryGet();
    }

    void Scene::RetireScript(ScriptableEntity* script)
    {
        m_retiredScripts.push_back(script);
        if (!m_isUpdating && !m_isLoadingEntityNodes && m_scriptCallbackDepth == 0) FlushRetiredScripts();
    }
    void Scene::FlushRetiredScripts()
    {
        while (!m_retiredScripts.empty()) {
            auto scripts = std::move(m_retiredScripts); m_retiredScripts.clear();
            for (auto* script : scripts) { script->Destroy(); delete script; }
        }
    }
    void Scene::SetPaused(bool paused)
    {
        m_paused=paused;
        Audio::SetScenePaused(paused);
    }
    void Scene::Unload()
    {
        DestroyRenderTarget(m_qualityRenderTarget);
        Audio::SetScenePaused(false);
        Audio::SetListener(Vector3(0),Quaternion(1,0,0,0),1,false);
        tweens.Clear();
        if (managedStop) managedStop();
        tweens.Clear();
        ++m_scriptingEpoch;
        m_isUpdating = m_isLoadingEntityNodes = false;
        FlushRetiredScripts();
        for (auto* entity : m_entities) if (entity) m_entityStates.at(entity->GetHandle()).pendingDestroy = true;
        for (Entity*& e : m_entities)
        {
            Entity* entity = e;
            e = nullptr;

            if (entity == nullptr)
                continue;
            entity->RemoveAllScripts();
        }

        m_entityStates.clear();
        m_referenceUUIDs.clear();
        m_missingReferences.clear();
        m_entities.clear();
        m_entityIndices.clear();
        m_registry.clear();
        // Fully release EnTT storage pools now, while any game-code component
        // types are still loaded. Otherwise pool destruction can be deferred
        // until Scene itself dies after the game shared library unloads.
        entt::registry emptyRegistry = {};
        // Retain EnTT generations across reloads so old two-field wrappers stay invalid.
        emptyRegistry.storage<entt::entity>().swap(m_registry.storage<entt::entity>());
        m_registry.swap(emptyRegistry);
        m_paused = false;
        m_entitiesToReady.clear();
        m_entitiesToDestroy.clear();
        m_loadingEntities.clear();
        m_isUpdating = false;
        m_isLoadingEntityNodes = false;
        m_environmentSkyboxUUID = UUID(0);
        m_environmentPostProcessUUID = UUID(0);
        m_showColliders = false;
        m_baking = {};
        m_lightingProbes.clear();
        m_surfaceWetness = {};
        ClearDebugGizmoLines();
        ClearLastRenderCamera();
        ClearEditorCameraOverrides();

        for (System* system : m_systems)
        {
            system->OnDestroy();
        }

        for (System* system : m_systems)
        {
            delete system;
        }

        m_systems.clear();
        m_updateSystems.clear();
        m_renderSystems.clear();
        m_systemTimings.clear();
    }

    void Scene::QuitGame()
    {
        SetPaused(false);
        Time::SetTimeScale(1.0f);

#if CANIS_EDITOR
        if (app != nullptr)
        {
            Editor& editor = app->GetEditor();
            const EditorMode mode = editor.GetMode();
            if (mode == EditorMode::PLAY || mode == EditorMode::PAUSE)
            {
                editor.RequestStopPlayMode();
                return;
            }
        }
#endif

        GetWindow().RequestClose();
    }

    void Scene::Load(std::string _path)
    {
        YAML::Node root;
        try
        {
            m_path = _path;
            root = YAML::LoadFile(m_path);
        }
        catch (const YAML::BadFile&)
        {
            Debug::FatalError("Scene not found: %s", m_path.c_str());
            return;
        }
        catch (const YAML::Exception &_exception)
        {
            Debug::FatalError("Failed to load scene '%s': %s", m_path.c_str(), _exception.what());
            return;
        }

        if (!root)
        {
            Debug::FatalError("Scene not found: %s", m_path.c_str());
        }
        
        AssetManager::PreloadReferencedTextures(root);
        LoadSceneNode(root);
    }

    void Scene::LoadSceneNode(YAML::Node &_root)
    {
        CreateRenderSystem<Canis::MeshRenderer3DSystem>();
        CreateRenderSystem<Canis::SpriteRenderer2DSystem>();
        CreateSystem<Canis::UIInteractionSystem>();
        CreateSystem<Canis::AnimatorSystem>();
        CreateSystem<Canis::AnimationPlayerSystem>();
        CreateSystem<Canis::ModelAnimation3DSystem>();
        CreateSystem<Canis::BoneAttachmentSystem>();
        CreateSystem<Canis::SpriteAnimationSystem>();
        CreateSystem<Canis::JoltPhysics3DSystem>();
        CreateSystem<Canis::NavMeshSystem>();
        CreateSystem<Canis::CloudNavSystem>();

        if (app != nullptr)
        {
            for (const SystemConf& conf : app->GetSystemRegistry())
            {
                if (!conf.autoCreate || conf.Construct == nullptr)
                    continue;

                if (conf.pipeline == SystemPipeline::Render)
                    CreateRenderSystem(conf.Construct());
                else
                    CreateSystem(conf.Construct());
            }
        }

        m_environmentAmbientLight = Color(0.24f, 0.26f, 0.32f, 1.0f);
        m_environmentAmbientLightIntensity = 1.0f;
        m_environmentReflectionsEnabled = true;
        m_environmentSkyboxUUID = UUID(0);
        m_environmentPostProcessUUID = UUID(0);
        m_showColliders = false;
        m_baking = {};
        m_lightingProbes.clear();
        m_lightingScenarios.clear();
        m_cloudVolume = {};
        m_surfaceWetness = {};
        m_cloudSimulationTime = 0;
        m_lightingScenarioSources = YAML::Node(YAML::NodeType::Sequence);
        m_initialLightingScenario.clear();
        ResetLightingScenarioRuntime();
        m_paused = false;
        
        for (System* system : m_systems)
        {
            system->Create();
        }

        for (System* system : m_systems)
        {
            system->Ready();
        }

        auto environment = _root["Environment"]; 

        if (environment)
        {
            m_window->SetClearColor(
                environment["ClearColor"].as<Vector4>(Vector4(0.05f, 0.05f, 0.05f, 1.0f))
            );
            m_environmentAmbientLight = environment["AmbientLight"].as<Vector4>(Vector4(0.24f, 0.26f, 0.32f, 1.0f));
            m_environmentAmbientLightIntensity = std::max(environment["AmbientLightIntensity"].as<float>(1.0f), 0.0f);
            m_environmentReflectionsEnabled = environment["Reflections"].as<bool>(true);
            m_cloudVolume = DeserializeCloudVolumeSettings(environment["CloudVolume"]);
            m_surfaceWetness = DeserializeSurfaceWetnessSettings(environment["SurfaceWetness"]);
            if(auto probes=environment["LightingProbes"]; probes && probes.IsSequence()) {
                for(const auto& node:probes) {
                    if(m_lightingProbes.size()>=MaxLightingProbes)break;
                    LightingProbe probe;
                    probe.enabled=node["Enabled"].as<bool>(true);
                    probe.cachePath=node["CachePath"].as<std::string>("");
                    probe.position=node["Position"].as<Vector3>(Vector3(0));
                    probe.halfExtents=node["HalfExtents"].as<Vector3>(Vector3(5));
                    probe.blendDistance=node["BlendDistance"].as<float>(1);
                    probe.diffuseIntensity=node["DiffuseIntensity"].as<float>(1);
                    probe.specularIntensity=node["SpecularIntensity"].as<float>(1);
                    probe.nearClip=node["NearClip"].as<float>(.1f);
                    probe.farClip=node["FarClip"].as<float>(100);
                    probe.Sanitize();m_lightingProbes.push_back(probe);
                }
            }
            m_lightingScenarioSources = YAML::Clone(environment["LightingScenarioSources"]);
            if (auto scenarios = environment["LightingScenarios"]; scenarios && scenarios.IsSequence())
                for (const auto& node : scenarios) {
                    auto scenario = DeserializeLightingScenario(node);
                    if (!scenario.id.empty() && !FindLightingScenario(scenario.id))
                        m_lightingScenarios.push_back(std::move(scenario));
                }
            m_initialLightingScenario = environment["InitialLightingScenario"].as<std::string>("");
            if (!FindLightingScenario(m_initialLightingScenario) && !m_lightingScenarios.empty())
                m_initialLightingScenario = m_lightingScenarios.front().id;
            ResetLightingScenarioRuntime();
            m_showColliders = environment["ShowColliders"].as<bool>(false);
            if (auto baking = environment["Baking"]) {
                m_baking.lightmaps = baking["Lightmaps"].as<bool>(true);
                m_baking.staticShadows = baking["StaticShadows"].as<bool>(true);
                m_baking.occlusion = baking["Occlusion"].as<bool>(true);
            }


            if (YAML::Node skyboxNode = environment["SkyboxAsset"])
            {
                m_environmentSkyboxUUID = ResolveEnvironmentAssetUUID(skyboxNode);
            }

            if (YAML::Node postProcessNode = environment["PostProcessAsset"])
            {
                m_environmentPostProcessUUID = ResolveEnvironmentAssetUUID(postProcessNode);
            }
        }

        auto runtimeRoot = RestoreLightingScenarioSources(_root, true);
        auto entities = runtimeRoot["Entities"];

        LoadEntityNodes(entities);
        if (m_window != nullptr && !m_lightingScenarios.empty()) {
            // Ready callbacks run on the next update. Preload scenario data now,
            // including scene switches and editor restores. Missing/stale probe
            // caches are repaired here, after validating the existing surface
            // bake, so reaching a weather trigger never performs captures.
            std::string error;
            if (auto* renderer = GetSystem<MeshRenderer3DSystem>())
                if (!renderer->LoadLightingScenarios(error))
                    Debug::Warning("Lighting scenarios: %s", error.c_str());
        }
    }

    std::vector<Entity*> Scene::LoadEntityNodes(YAML::Node &_entities, bool _copyUUID)
    {
        const auto loadStart = SDL_GetTicksNS();
        const size_t authoredCount = _entities ? _entities.size() : 0;
        std::vector<CombinedModelLoad> combinedGroups;
        YAML::Node entities;
        if (ShouldLoadCombinedModels(*this))
        {
            const char* build = std::getenv("CANIS_BUILD_COMBINE_MODELS");
            entities.reset(PrepareCombinedModelNodes(_entities, build && std::string(build) == "1", combinedGroups));
        }
        else entities.reset(_entities);
        std::unordered_set<UUID> sourceIDs;
        if (entities) for (auto node : entities) {
            if (!sourceIDs.insert(node["Entity"].as<UUID>(0)).second)
                throw std::runtime_error("Duplicate entity UUID in scene/prefab");
        }
        m_loadingEntities.clear();
        m_isLoadingEntityNodes = true;
        std::vector<Canis::Entity*> newEntitys = {};

        if (entities)
        {
            // Allocate all targets before decoding any reference fields.
            for (auto node : entities) {
                const UUID sourceUUID = node["Entity"].as<UUID>(0);
                auto created = CreateEntity();
                if (_copyUUID) created.SetUUID(sourceUUID);
                m_loadingEntities.emplace(sourceUUID, created);
                newEntitys.push_back(created.TryGet());
            }
            for (auto node : entities) DecodeEntity(node, _copyUUID);
            // Keep bidirectional hierarchy links in sync after pointer remapping.
            for (Canis::Entity* entity : newEntitys)
            {
                if (entity == nullptr)
                    continue;

                if (entity->HasComponent<RectTransform>())
                {
                    RectTransform& transform = entity->GetComponent<RectTransform>();
                    if (transform.parent != nullptr)
                    {
                        if (transform.parent->HasComponent<RectTransform>())
                        {
                            RectTransform& parentTransform = transform.parent->GetComponent<RectTransform>();
                            auto& siblings = parentTransform.children;
                            if (std::find(siblings.begin(), siblings.end(), entity) == siblings.end())
                                siblings.push_back(entity);
                        }
                        else
                        {
                            transform.parent = nullptr;
                        }
                    }

                    auto& rectChildren = transform.children;
                    std::unordered_set<Canis::Entity*> seenRectChildren = {};
                    rectChildren.erase(std::remove_if(rectChildren.begin(), rectChildren.end(),
                        [entity, &seenRectChildren](Canis::Entity* child) -> bool
                        {
                            if (child == nullptr || !child->HasComponent<RectTransform>())
                                return true;

                            RectTransform& childTransform = child->GetComponent<RectTransform>();
                            // The child's encoded parent is authoritative. Only
                            // infer it from a children list for legacy data that
                            // did not encode a parent.
                            if (childTransform.parent == nullptr)
                                childTransform.parent = entity;
                            return childTransform.parent != entity ||
                                !seenRectChildren.insert(child).second;
                        }), rectChildren.end());
                }

                if (entity->HasComponent<Transform>())
                {
                    Transform& transform = entity->GetComponent<Transform>();
                    if (transform.parent != nullptr)
                    {
                        if (transform.parent->HasComponent<Transform>())
                        {
                            Transform& parentTransform = transform.parent->GetComponent<Transform>();
                            auto& siblings = parentTransform.children;
                            if (std::find(siblings.begin(), siblings.end(), entity) == siblings.end())
                                siblings.push_back(entity);
                        }
                        else
                        {
                            transform.parent = nullptr;
                        }
                    }

                    auto& transformChildren = transform.children;
                    std::unordered_set<Canis::Entity*> seenTransformChildren = {};
                    transformChildren.erase(std::remove_if(transformChildren.begin(), transformChildren.end(),
                        [entity, &seenTransformChildren](Canis::Entity* child) -> bool
                        {
                            if (child == nullptr || !child->HasComponent<Transform>())
                                return true;

                            Transform& childTransform = child->GetComponent<Transform>();
                            if (childTransform.parent == nullptr)
                                childTransform.parent = entity;
                            return childTransform.parent != entity ||
                                !seenTransformChildren.insert(child).second;
                        }), transformChildren.end());
                }
            }

            for (auto* e : newEntitys) {
                const auto scripts = e->GetScripts();
                for (const auto& handle : scripts) if (auto* script = handle.TryGet()) script->Create();
            }
        }

        FinishCombinedModelLoad(*this, combinedGroups, authoredCount, newEntitys.size(),
            double(SDL_GetTicksNS() - loadStart) / 1e6);
        m_loadingEntities.clear();
        m_isLoadingEntityNodes = false;
        FlushRetiredScripts();

        if (!m_entitiesToDestroy.empty())
        {
            auto pendingDestroyIds = std::move(m_entitiesToDestroy);
            m_entitiesToDestroy.clear();

            for (const auto& handle : pendingDestroyIds) DestroyNow(handle);
        }

        return newEntitys;
    }

    Canis::Entity& Scene::DecodeEntity(YAML::Node _node, bool _copyUUID)
    {
        const UUID sourceUUID = _node["Entity"].as<UUID>(0);
        const auto prepared = m_loadingEntities.find(sourceUUID);
        Entity& entity = m_isLoadingEntityNodes && prepared != m_loadingEntities.end()
            ? *prepared->second : *CreateEntity();
        if (_copyUUID) entity.SetUUID(sourceUUID);
        entity.GetName() = _node["Name"].as<std::string>("");
        entity.SetTag(_node["Tag"].as<std::string>(""));
        entity.Active() = _node["Active"].as<bool>(true);
        entity.EditorLocked() = _node["EditorLocked"].as<bool>(false);

        if (app != nullptr)
        {
            std::vector<ScriptConf>& scriptRegistry = app->GetScriptRegistry();

            for (int i = 0; i < scriptRegistry.size(); i++)
            {
                if (scriptRegistry[i].Decode)
                {
                    scriptRegistry[i].Decode(_node, entity, false);
                }
            }
        }

        Scripting::DecodeManagedComponents(_node, entity);
        return entity;
    }

    Scene::~Scene() { Unload(); }

    UUID Scene::GetReferenceUUID(entt::entity handle) const
    {
        if (m_registry.valid(handle)) {
            if (const auto* metadata = m_registry.try_get<EntityMetadata>(handle)) return metadata->uuid;
        }
        const auto found = m_referenceUUIDs.find(handle);
        return found == m_referenceUUIDs.end() ? UUID(0) : found->second;
    }
    Entity Scene::ResolveReference(UUID uuid)
    {
        if (uuid == UUID(0)) return Entity(*this, entt::null);
        if (m_isLoadingEntityNodes) {
            const auto prepared = m_loadingEntities.find(uuid);
            if (prepared != m_loadingEntities.end()) return prepared->second;
        }
        if (auto* target = GetEntityWithUUID(uuid)) return *target;
        auto found = m_missingReferences.find(uuid);
        if (found != m_missingReferences.end()) return Entity(*this, found->second);
        // Reserve an invalid versioned ID, with its authored UUID stored only in Scene.
        const auto missing = m_registry.create();
        m_registry.destroy(missing);
        m_referenceUUIDs.emplace(missing, uuid);
        m_missingReferences.emplace(uuid, missing);
        return Entity(*this, missing);
    }
    void Scene::GetEntityAfterLoad(UUID uuid, Entity& variable) { variable = ResolveReference(uuid); }

    std::vector<Entity*> Scene::Instantiate(const SceneAssetHandle &_sceneAssetHandle)
    {
        std::vector<Entity*> rootEntities = {};
        const std::string scenePath = AssetManager::ResolvePath(_sceneAssetHandle);

        if (scenePath.empty())
        {
            Debug::Warning(
                "Scene::Instantiate could not resolve scene handle (uuid=%llu, path='%s').",
                static_cast<unsigned long long>(_sceneAssetHandle.uuid),
                _sceneAssetHandle.path.c_str());
            return rootEntities;
        }

        if (!std::filesystem::exists(scenePath))
        {
            Debug::Warning("Scene::Instantiate could not find scene '%s'.", scenePath.c_str());
            return rootEntities;
        }

        try
        {
            YAML::Node root = YAML::LoadFile(scenePath);
            YAML::Node entities = root["Entities"];
            std::vector<Entity*> newEntities = LoadEntityNodes(entities, false);

            for (Entity *entity : newEntities)
            {
                if (entity != nullptr && !HasHierarchyParent(entity))
                    rootEntities.push_back(entity);
            }
        }
        catch (const YAML::Exception &_exception)
        {
            Debug::Warning(
                "Scene::Instantiate failed to load '%s': %s",
                scenePath.c_str(),
                _exception.what());
        }

        return rootEntities;
    }

    void Scene::ForceReady(Entity& _entity)
    {
        const Entity owner(&_entity);
        const auto scripts = _entity.GetScripts();
        for (const auto& handle : scripts)
        {
            if (!owner) break;
            auto* script = handle.TryGet();
            if (script == nullptr || script->m_onReadyCalled)
                continue;

            script->m_onReadyCalled = true;
            InvokeScriptCallback(script, true);
        }
    }

    bool Scene::Save()
    {
        Debug::Log("Save Scene");
        
        YAML::Emitter out;

        out << EncodeScene();

        const std::string path = m_path.empty() ? m_name : m_path;
        std::ofstream fout(path);
        fout << out.c_str();
        fout.flush();
        if (!out.good() || !fout.good())
        {
            Debug::Warning("Failed to save scene '%s'.", path.c_str());
            return false;
        }
        return true;
    }

    YAML::Node Scene::EncodeScene()
    {
        YAML::Node node;

        YAML::Node environment;
        environment["ClearColor"] = m_window->GetClearColor();
        environment["AmbientLight"] = m_environmentAmbientLight;
        environment["AmbientLightIntensity"] = m_environmentAmbientLightIntensity;
        environment["Reflections"] = m_environmentReflectionsEnabled;
        environment["ShowColliders"] = m_showColliders;
        auto baking = environment["Baking"];
        baking["Lightmaps"] = m_baking.lightmaps;
        baking["StaticShadows"] = m_baking.staticShadows;
        baking["Occlusion"] = m_baking.occlusion;

        if ((uint64_t)m_environmentSkyboxUUID != 0)
        {
            YAML::Node skyboxAsset(YAML::NodeType::Map);
            skyboxAsset["uuid"] = (uint64_t)m_environmentSkyboxUUID;
            environment["SkyboxAsset"] = skyboxAsset;
        }
        if ((uint64_t)m_environmentPostProcessUUID != 0)
        {
            YAML::Node postProcessAsset(YAML::NodeType::Map);
            postProcessAsset["uuid"] = (uint64_t)m_environmentPostProcessUUID;
            environment["PostProcessAsset"] = postProcessAsset;
        }
        YAML::Node probes(YAML::NodeType::Sequence);
        for(auto probe:m_lightingProbes) {
            if(probes.size()>=MaxLightingProbes)break;
            probe.Sanitize();YAML::Node p;
            p["Enabled"]=probe.enabled;p["CachePath"]=probe.cachePath;p["Position"]=probe.position;p["HalfExtents"]=probe.halfExtents;
            p["BlendDistance"]=probe.blendDistance;p["DiffuseIntensity"]=probe.diffuseIntensity;
            p["SpecularIntensity"]=probe.specularIntensity;p["NearClip"]=probe.nearClip;p["FarClip"]=probe.farClip;
            probes.push_back(p);
        }
        environment["LightingProbes"]=probes;
        environment["CloudVolume"] = SerializeCloudVolumeSettings(m_cloudVolume);
        environment["SurfaceWetness"] = SerializeSurfaceWetnessSettings(m_surfaceWetness);
        if (!m_lightingScenarios.empty()) {
            YAML::Node scenarios(YAML::NodeType::Sequence);
            for (const auto& scenario : m_lightingScenarios) scenarios.push_back(SerializeLightingScenario(scenario));
            environment["LightingScenarios"] = scenarios;
            environment["InitialLightingScenario"] = m_initialLightingScenario;
        }
        if (m_lightingScenarioSources && m_lightingScenarioSources.IsSequence())
        {
            auto sources = YAML::Clone(m_lightingScenarioSources);
            // An empty sequence has no block representation. Set its style
            // explicitly so load/re-encode does not change editor history text.
            if (sources.size() == 0)
                sources.SetStyle(YAML::EmitterStyle::Flow);
            environment["LightingScenarioSources"] = sources;
        }
        node["Environment"] = environment;

        YAML::Node entities = YAML::Node(YAML::NodeType::Sequence);

        for(Entity* entity : m_entities)
        {
            if (!entity)
                continue;

            entities.push_back(EncodeEntity(*entity));
        }

        node["Entities"] = entities;

        return node;
    }

    YAML::Node Scene::EncodeEntity(Entity &_entity)
    {
        YAML::Node node;
        Scripting::EncodeManagedComponents(node, _entity);
        node["Entity"] = _entity.GetUUID();
        node["Name"] = _entity.GetName();
        node["Tag"] = _entity.GetTagName();
        node["Active"] = _entity.Active();
        node["EditorLocked"] = _entity.EditorLocked();

        std::vector<ScriptConf>& scriptRegistry = app->GetScriptRegistry();

        for (int i = 0; i < scriptRegistry.size(); i++)
            if (scriptRegistry[i].Encode)
                scriptRegistry[i].Encode(node, _entity);
        
        return node;
    }

    Entity Scene::CreateEntity(std::string _name, std::string _tag)
    {
        const auto handle = m_registry.create();
        m_registry.emplace<EntityMetadata>(handle);
        Entity* entity = &m_registry.emplace<Entity>(handle, *this, handle);
        m_entityStates.emplace(handle, EntityState{});
        entity->GetName() = _name;
        entity->SetTag(_tag);

        // TODO : handle better
        for (int i = 0; i < m_entities.size(); i++)
        {
            if (m_entities[i] == nullptr)
            {
                if (i == 0)
                    Debug::Log("The Camera Just Died");
                m_entities[i] = entity;
                m_entityIndices.emplace(handle, i);
                QueueEntityForReady(*entity);
                return entity;
            }
        }
        
        m_entityIndices.emplace(handle, static_cast<int>(m_entities.size()));
        m_entities.push_back(entity);
        QueueEntityForReady(*entity);
        return entity;
    }

    Scene::EntityState* Scene::GetEntityState(entt::entity handle)
    {
        const auto entry = m_entityStates.find(handle);
        return entry == m_entityStates.end() ? nullptr : &entry->second;
    }

    int Scene::GetEntityIndex(const Entity& entity) const
    {
        if (&entity.scene != this) return -1;
        const auto entry = m_entityIndices.find(entity.GetHandle());
        if (entry == m_entityIndices.end()) return -1;
        const int index = entry->second;
        return index >= 0 && index < static_cast<int>(m_entities.size()) && m_entities[index] && m_entities[index]->GetHandle() == entity.GetHandle()
            ? index : -1;
    }

    Entity* Scene::GetEntity(int _id)
    {
        if (_id > -1 && _id < m_entities.size())
            return m_entities[_id];
        
        // TODO : Handle Error
        return nullptr; 
    }

    Entity* Scene::GetEntityWithUUID(Canis::UUID _uuid)
    {
        for (int i = 0; i < m_entities.size(); i++)
            if (m_entities[i] != nullptr)
                if (m_entities[i]->GetUUID() == _uuid)
                    return m_entities[i];

        
        // TODO : Handle Error
        return nullptr; 
    }

    UUID Scene::GetLiveEntityUUID(const Entity* _entity) const
    {
        if (_entity == nullptr)
            return UUID(0);

        for (const Entity* liveEntity : m_entities)
        {
            if (liveEntity == _entity)
                return liveEntity->GetUUID();
        }

        return UUID(0);
    }

    Entity* Scene::FindEntityWithName(std::string _name)
    {
        for (Entity* entity : m_entities)
        {
            if (entity == nullptr)
                continue;
            
            if (entity->GetName() == _name)
                return entity;
        }

        return nullptr;
    }

    Entity* Scene::GetEntityWithTag(TagId _tag)
    {
        for (Entity* entity : m_entities)
        {
            if (entity == nullptr)
                continue;
            
            if (entity->GetTag() == _tag)
                return entity;
        }

        return nullptr;
    }

    std::vector<Entity*> Scene::GetEntitiesWithTag(TagId _tag)
    {
        std::vector<Entity*> entities = {};

        for (Entity* entity : m_entities)
        {
            if (entity == nullptr)
                continue;
            
            if (entity->GetTag() == _tag)
                entities.push_back(entity);
        }

        return entities;
    }

    void Scene::Destroy(int _id)
    {
        if (_id < 0 || m_entities.size() <= _id)
        {
            Debug::Log("size %d, id %d", m_entities.size(), _id);
            return;
        }

        if (m_entities[_id] == nullptr)
        {
            Debug::Log("Why are you NULL");
            return;
        }

        if (m_isUpdating || m_isLoadingEntityNodes || m_scriptCallbackDepth != 0)
        {
            if (!m_entityStates.at(m_entities[_id]->GetHandle()).pendingDestroy) {
                MarkPendingDestroy(m_entities[_id]);
                m_entitiesToDestroy.emplace_back(m_entities[_id]);
            }
            return;
        }

        DestroyNow(_id);
    }

    void Scene::MarkPendingDestroy(Entity* entity)
    {
        if (!entity || m_entityStates.at(entity->GetHandle()).pendingDestroy) return;
        m_entityStates.at(entity->GetHandle()).pendingDestroy = true;
        entity->Active() = false;
        if (auto* transform = m_registry.try_get<Transform>(entity->GetHandle()))
            for (const auto& child : transform->children) MarkPendingDestroy(child.ResolveIncludingPending());
        if (auto* transform = m_registry.try_get<RectTransform>(entity->GetHandle()))
            for (const auto& child : transform->children) MarkPendingDestroy(child.ResolveIncludingPending());
    }

    void Scene::DestroyNow(const Entity& handle)
    {
        auto* entity = handle.ResolveIncludingPending();
        if (entity) DestroyNow(GetEntityIndex(*entity));
    }

    void Scene::DestroyNow(int _id)
    {
        if (_id < 0 || m_entities.size() <= _id)
            return;

        Entity* entity = m_entities[_id];
        if (entity == nullptr)
            return;

        if (m_entityStates.at(entity->GetHandle()).destroying) return;
        m_entityStates.at(entity->GetHandle()).destroying = true;
        MarkPendingDestroy(entity);
        // Capture child ids before component teardown mutates hierarchy links.
        std::vector<Entity> childIdsToDestroy = {};
        auto queueChildForDestroy = [&](Entity* _child)
        {
            if (_child == nullptr || _child == entity)
                return;

            const int childId = GetEntityIndex(*_child);
            if (childId < 0 || childId >= static_cast<int>(m_entities.size()))
                return;

            if (m_entities[childId] != _child)
                return;

            if (std::find(childIdsToDestroy.begin(), childIdsToDestroy.end(), _child) == childIdsToDestroy.end())
                childIdsToDestroy.emplace_back(_child);
        };

        if (entity->HasComponent<RectTransform>())
        {
            RectTransform& rectTransform = entity->GetComponent<RectTransform>();
            if (rectTransform.parent != nullptr)
            {
                if (rectTransform.parent->HasComponent<RectTransform>())
                {
                    RectTransform& parentTransform = rectTransform.parent->GetComponent<RectTransform>();
                    auto& siblings = parentTransform.children;
                    siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
                }
                rectTransform.parent = nullptr;
            }

            for (const auto& child : rectTransform.children)
                queueChildForDestroy(child.ResolveIncludingPending());
        }

        if (entity->HasComponent<Transform>())
        {
            Transform& transform3D = entity->GetComponent<Transform>();
            if (transform3D.parent != nullptr)
            {
                if (transform3D.parent->HasComponent<Transform>())
                {
                    Transform& parentTransform = transform3D.parent->GetComponent<Transform>();
                    auto& siblings = parentTransform.children;
                    siblings.erase(std::remove(siblings.begin(), siblings.end(), entity), siblings.end());
                }
                transform3D.parent = nullptr;
            }

            for (const auto& child : transform3D.children)
                queueChildForDestroy(child.ResolveIncludingPending());
        }

        for (const auto& child : childIdsToDestroy) DestroyNow(child);

        // Clear slot first to prevent recursive self-destroy during callbacks.
        m_entities[_id] = nullptr;
        m_entityIndices.erase(entity->GetHandle());

        m_entityStates.at(entity->GetHandle()).pendingDestroy = true;
        entity->RemoveAllScripts();
        const auto handle = entity->GetHandle();
        m_referenceUUIDs[handle] = entity->GetUUID();
        m_entityStates.erase(handle);
        if (handle != entt::null && m_registry.valid(handle)) m_registry.destroy(handle);
    }

    void Scene::Destroy(Entity& _entity)
    {
        const int index = GetEntityIndex(_entity);
        if (index >= 0) Destroy(index);
    }

    void Scene::QueueEntityForReady(Entity& entity)
    {
        if (GetEntityIndex(entity) >= 0)
            m_entitiesToReady.emplace_back(&entity);
    }

    System* Scene::CreateSystem(System* _system)
    {
        if (_system == nullptr)
            return nullptr;

        m_updateSystems.push_back(_system);
        ReadySystem(_system);
        return _system;
    }

    System* Scene::CreateRenderSystem(System* _system)
    {
        if (_system == nullptr)
            return nullptr;

        m_renderSystems.push_back(_system);
        ReadySystem(_system);
        return _system;
    }

    void Scene::ReadySystem(System *_system)
    {
        _system->scene = this;
        _system->window = m_window;
        _system->inputManager = m_inputManager;
        //_system->time = m_time;
        //_system->camera = camera;
        m_systems.push_back(_system);
        m_systemTimings.push_back(SystemTiming{
            .name = _system->GetName(),
            .updateMs = 0.0f,
            .renderMs = 0.0f,
            .system = _system
        });
    }

    Scene::SystemTiming* Scene::GetSystemTiming(System* _system)
    {
        for (SystemTiming& timing : m_systemTimings)
        {
            if (timing.system == _system)
                return &timing;
        }

        return nullptr;
    }
}
