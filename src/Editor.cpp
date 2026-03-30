#include <Canis/Editor.hpp>

#include <Canis/Canis.hpp>
#include <Canis/Debug.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Window.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>
#include <Canis/App.hpp>
#include <Canis/Time.hpp>
#include <Canis/Shader.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/GameCodeObject.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Yaml.hpp>

#include <SDL3/SDL.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>

#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cctype>

namespace Canis
{
    static const char *GetInspectorFieldID(const char *_label, const char *_idSuffix)
    {
        if (_idSuffix != nullptr && _idSuffix[0] != '\0')
            return _idSuffix;

        return _label;
    }

    namespace
    {
        YAML::Node g_lastPlaySceneNode;

        struct RectTransformRenderBounds
        {
            Vector2 min = Vector2(0.0f);
            Vector2 size = Vector2(0.0f);
            Vector2 rotationPivot = Vector2(0.0f);
        };

        RectTransformRenderBounds GetRenderBounds(const Entity& _entity, const RectTransform& _transform)
        {
            RectTransformRenderBounds bounds = {};
            bounds.min = _transform.GetRectMin() + _transform.originOffset;
            bounds.size = _transform.GetResolvedSize();
            bounds.rotationPivot = _transform.GetPosition();

            if (_entity.HasComponent<Text>())
                bounds.rotationPivot += _transform.rotationOriginOffset;

            return bounds;
        }
    }

    static std::string ResolveAssetRefPath(const YAML::Node &_node)
    {
        if (!_node)
            return "";

        if (_node.IsMap())
        {
            if (YAML::Node uuidNode = _node["uuid"])
            {
                UUID uuid = uuidNode.as<uint64_t>(0);
                if ((uint64_t)uuid != 0)
                {
                    std::string path = AssetManager::GetPath(uuid);
                    if (path != "Path was not found in AssetLibrary")
                        return path;
                }
            }

            if (YAML::Node pathNode = _node["path"])
                return pathNode.as<std::string>("");

            return "";
        }

        if (_node.IsScalar())
        {
            std::string raw = _node.as<std::string>("");
            if (raw.empty())
                return "";

            bool isNumeric = std::all_of(raw.begin(), raw.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
            if (isNumeric)
            {
                UUID uuid = (UUID)std::stoull(raw);
                std::string path = AssetManager::GetPath(uuid);
                if (path != "Path was not found in AssetLibrary")
                    return path;
            }

            return raw;
        }

        return "";
    }

    static void SetAssetRefUUID(YAML::Node &_root, const std::string &_key, const std::string &_path)
    {
        YAML::Node node(YAML::NodeType::Map);
        if (MetaFileAsset *meta = AssetManager::GetMetaFile(_path))
            node["uuid"] = (uint64_t)meta->uuid;
        _root[_key] = node;
    }

    static void ApplyMaterialNodeToAsset(const YAML::Node &_root, MaterialAsset *_material)
    {
        if (_material == nullptr)
            return;

        _material->info = 0u;
        _material->shaderId = -1;
        _material->albedoId = -1;
        _material->specularId = -1;
        _material->roughnessId = -1;
        _material->metallicId = -1;
        _material->emissionId = -1;
        _material->color = Color(1.0f);
        _material->specularValue = 0.5f;
        _material->roughnessValue = 0.5f;
        _material->metallicValue = 0.0f;
        _material->materialFields = MaterialFields();

        if (YAML::Node shaderNode = _root["shader"])
        {
            std::string shaderPath = ResolveAssetRefPath(shaderNode);
            if (!shaderPath.empty())
            {
                if (shaderPath.size() > 3 && shaderPath.ends_with(".vs"))
                    shaderPath = shaderPath.substr(0, shaderPath.size() - 3);
                else if (shaderPath.size() > 3 && shaderPath.ends_with(".fs"))
                    shaderPath = shaderPath.substr(0, shaderPath.size() - 3);

                _material->shaderId = AssetManager::LoadShader(shaderPath);
                if (_material->shaderId >= 0)
                    _material->info |= MATERIAL_HAS_SHADER;
            }
        }

        if (YAML::Node albedoNode = _root["albedo"])
        {
            std::string path = ResolveAssetRefPath(albedoNode);
            if (!path.empty())
            {
                _material->albedoId = AssetManager::LoadTexture(path);
                if (_material->albedoId >= 0)
                    _material->info |= MATERIAL_HAS_ALBEDO;
            }
        }

        if (YAML::Node specularNode = _root["specular"])
        {
            std::string path = ResolveAssetRefPath(specularNode);
            if (!path.empty())
            {
                _material->specularId = AssetManager::LoadTexture(path);
                if (_material->specularId >= 0)
                    _material->info |= MATERIAL_HAS_SPECULAR;
            }
        }

        if (YAML::Node roughnessNode = _root["roughness"])
        {
            std::string path = ResolveAssetRefPath(roughnessNode);
            if (!path.empty())
            {
                _material->roughnessId = AssetManager::LoadTexture(path);
                if (_material->roughnessId >= 0)
                    _material->info |= MATERIAL_HAS_ROUGHNESS;
            }
        }

        if (YAML::Node metallicNode = _root["metallic"])
        {
            std::string path = ResolveAssetRefPath(metallicNode);
            if (!path.empty())
            {
                _material->metallicId = AssetManager::LoadTexture(path);
                if (_material->metallicId >= 0)
                    _material->info |= MATERIAL_HAS_METALLIC;
            }
        }

        if (YAML::Node emissionNode = _root["emission"])
        {
            std::string path = ResolveAssetRefPath(emissionNode);
            if (!path.empty())
            {
                _material->emissionId = AssetManager::LoadTexture(path);
                if (_material->emissionId >= 0)
                    _material->info |= MATERIAL_HAS_EMISSION;
            }
        }

        if (YAML::Node colorNode = _root["color"])
        {
            _material->color = colorNode.as<Color>(Color(1.0f));
            _material->info |= MATERIAL_HAS_COLOR;
        }

        _material->specularValue = _root["specularValue"].as<float>(0.5f);
        _material->roughnessValue = _root["roughnessValue"].as<float>(0.5f);
        _material->metallicValue = _root["metallicValue"].as<float>(0.0f);

        if (YAML::Node cullNode = _root["backFaceCulling"]; cullNode.as<bool>(false))
            _material->info |= MATERIAL_BACK_FACE_CULLING;

        if (YAML::Node cullNode = _root["frontFaceCulling"]; cullNode.as<bool>(false))
            _material->info |= MATERIAL_FRONT_FACE_CULLING;

        for (const auto &entry : _root)
        {
            const std::string key = entry.first.as<std::string>("");
            if (key == "shader" || key == "albedo" || key == "specular" || key == "roughness" || key == "metallic" ||
                key == "emission" || key == "color" || key == "specularValue" || key == "roughnessValue" || key == "metallicValue" ||
                key == "backFaceCulling" || key == "frontFaceCulling")
            {
                continue;
            }

            if (!entry.second.IsScalar())
                continue;

            try
            {
                _material->materialFields.SetFloat(key, entry.second.as<float>());
            }
            catch (const YAML::Exception &)
            {
            }
        }
    }

    std::vector<const char *> ConvertComponentToCStringVector(App &_app, Entity &_entity)
    {
        std::vector<const char *> cStringVector;
        for (ScriptConf &conf : _app.GetScriptRegistry())
        {
            if (conf.Has(_entity))
                continue;

            cStringVector.push_back(conf.name.c_str());
        }
        return cStringVector;
    }

    void Editor::Init(Window *_window)
    {
#if CANIS_EDITOR
        // if (GetProjectConfig().editor == false)
        //     return;
        //{

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        //ImGui::LoadIniSettingsFromMemory("");
        static std::string imguiIniPath = std::string(SDL_GetBasePath()) + "project_settings/imgui.ini";
        io.IniFilename = imguiIniPath.c_str();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
        io.ConfigWindowsMoveFromTitleBarOnly = true;

#ifdef __EMSCRIPTEN__

#else
        // Keep editor windows docked inside the main SDL window.
        io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        io.ConfigViewportsNoAutoMerge = false;
        io.ConfigViewportsNoTaskBarIcon = true;
#endif


        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // ImGui::StyleColorsLight();

        // Setup scaling
        ImGuiStyle &style = ImGui::GetStyle();
        float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
        style.ScaleAllSizes(main_scale);   // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
        style.FontScaleDpi = main_scale;   // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)
        io.ConfigDpiScaleFonts = true;     // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
        io.ConfigDpiScaleViewports = true; // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        // Setup Platform/Renderer backends
        ImGui_ImplSDL3_InitForOpenGL((SDL_Window *)_window->GetSDLWindow(), (SDL_GLContext)_window->GetGLContext());
        ImGui_ImplOpenGL3_Init(OPENGLVERSION);

        m_assetPaths = FindFilesInFolder("assets", "");

        m_gameViewportWidth = _window->GetWindowWidth();
        m_gameViewportHeight = _window->GetWindowHeight();
        EnsureGameRenderTarget(m_gameViewportWidth, m_gameViewportHeight);
        m_playViewportWidth = _window->GetWindowWidth();
        m_playViewportHeight = _window->GetWindowHeight();
        EnsurePlayRenderTarget(m_playViewportWidth, m_playViewportHeight);
#endif
    }

    Editor::~Editor()
    {
        DestroyGameRenderTarget();
        DestroyPlayRenderTarget();
    }

    void Editor::BeginGameRender(Window* _window)
    {
#if CANIS_EDITOR
        int targetWidth = (m_gameViewportWidth > 0) ? m_gameViewportWidth : _window->GetWindowWidth();
        int targetHeight = (m_gameViewportHeight > 0) ? m_gameViewportHeight : _window->GetWindowHeight();

        EnsureGameRenderTarget(targetWidth, targetHeight);
        if (m_gameFramebuffer == 0)
            return;

        _window->SetRenderSize(targetWidth, targetHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, m_gameFramebuffer);
        glViewport(0, 0, targetWidth, targetHeight);

        Color clear = _window->GetClearColor();
        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
    }

    void Editor::BeginPlayRender(Window* _window)
    {
#if CANIS_EDITOR
        int targetWidth = (m_playViewportWidth > 0) ? m_playViewportWidth : _window->GetWindowWidth();
        int targetHeight = (m_playViewportHeight > 0) ? m_playViewportHeight : _window->GetWindowHeight();

        EnsurePlayRenderTarget(targetWidth, targetHeight);
        if (m_playFramebuffer == 0)
            return;

        _window->SetRenderSize(targetWidth, targetHeight);

        glBindFramebuffer(GL_FRAMEBUFFER, m_playFramebuffer);
        glViewport(0, 0, targetWidth, targetHeight);

        Color clear = _window->GetClearColor();
        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
    }

    void Editor::EndGameRender(Window* _window)
    {
#if CANIS_EDITOR
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, _window->GetWindowWidth(), _window->GetWindowHeight());
#endif
    }

    void Editor::Draw(Scene *_scene, Window *_window, App *_app, GameCodeObject *_gameSharedLib, float _deltaTime)
    {
#if CANIS_EDITOR
        // if (GetProjectConfig().editor)
        //{
        if (m_scene != _scene)
        {
            Debug::Log("new scene");
            m_hierarchyRootOrder.clear();
        }
        m_app = _app;
        m_scene = _scene;
        m_window = _window;
        m_gameSharedLib = _gameSharedLib;
        m_gameInputWindowID = SDL_GetWindowID((SDL_Window *)m_window->GetSDLWindow());

        // Pass 1: runtime/game camera (used by Game panel).
        m_scene->ClearEditorCameraOverrides();
        BeginPlayRender(m_window);
        m_scene->Render(_deltaTime);
        EndGameRender(m_window);

        // Pass 2: editor scene camera (used by Scene panel + gizmos).
        ApplyInternalSceneCamera(_deltaTime);
        BeginGameRender(m_window);
        m_scene->Render(_deltaTime);
        RenderGameDebug();
        EndGameRender(m_window);
        m_scene->ClearEditorCameraOverrides();

        // Keep logical gameplay size set to Game panel size for scripts/input math between frames.
        const int gameplayWidth = (m_playViewportWidth > 0) ? m_playViewportWidth : m_window->GetWindowWidth();
        const int gameplayHeight = (m_playViewportHeight > 0) ? m_playViewportHeight : m_window->GetWindowHeight();
        m_window->SetRenderSize(gameplayWidth, gameplayHeight);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        DrawMainDockspace();

        bool refresh = DrawHierarchyPanel();
        DrawInspectorPanel(refresh);
        DrawEnvironment();
        DrawSystemPanel();
        DrawAssetsPanel();
        DrawProjectSettings();
        DrawSceneView();
        DrawGameView();
        DrawEditorPanel(); // draw last

        SelectSprite2D();

        // find camera and verfy target entity
        m_debugDraw = DebugDraw::NONE;
        Camera2D *camera2D = nullptr;

        if (m_index > -1 && m_index < m_scene->GetEntities().size() && m_scene->GetEntities()[m_index] != nullptr)
        {
            Entity &entity = *m_scene->GetEntities()[m_index];

            std::vector<Entity *> &entities = m_scene->GetEntities();

            for (Entity *entity : entities)
            {
                if (entity == nullptr)
                    continue;

                Camera2D *camera = (entity != nullptr && entity->HasComponent<Camera2D>() ? &entity->GetComponent<Camera2D>() : nullptr);

                if (camera == nullptr)
                    continue;

                camera2D = camera;
            }

            if (entity.HasComponent<RectTransform>() && camera2D)
            {
                m_debugDraw = DebugDraw::RECT;
            }
        }

        // rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO &io = ImGui::GetIO();
        (void)io;

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

#endif
    }

    void Editor::DrawMainDockspace()
    {
        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;

        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        windowFlags |= ImGuiWindowFlags_NoTitleBar;
        windowFlags |= ImGuiWindowFlags_NoCollapse;
        windowFlags |= ImGuiWindowFlags_NoResize;
        windowFlags |= ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        windowFlags |= ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("MainDockspace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        const ImGuiID dockspaceID = ImGui::GetID("MainDockspaceID");
        ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), dockspaceFlags);
        ImGui::End();
    }

    void Editor::ApplyInternalSceneCamera(float _deltaTime)
    {
        if (m_scene == nullptr || m_window == nullptr)
            return;

        m_scene->ClearEditorCameraOverrides();

        // Scene view should always use the editor camera.
        // Game panel already renders with runtime/game cameras in pass 1.
        if (m_mode == EditorMode::HIDDEN)
            return;

        InputManager& input = m_scene->GetInputManager();
        const bool rightClickNavigation = m_gameViewHovered && input.GetRightClick();

        const int renderWidth = std::max(1, (m_gameViewportWidth > 0) ? m_gameViewportWidth : m_window->GetWindowWidth());
        const int renderHeight = std::max(1, (m_gameViewportHeight > 0) ? m_gameViewportHeight : m_window->GetWindowHeight());

        if (m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_3D)
        {
            if (rightClickNavigation)
            {
                m_editorCamera3DYaw += input.mouseRel.x * m_editorCamera3DLookSensitivity;
                m_editorCamera3DPitch -= input.mouseRel.y * m_editorCamera3DLookSensitivity;
                m_editorCamera3DPitch = std::clamp(m_editorCamera3DPitch, -89.0f, 89.0f);
            }

            const float yaw = DEG2RAD * m_editorCamera3DYaw;
            const float pitch = DEG2RAD * m_editorCamera3DPitch;

            Vector3 forward = Vector3(
                std::cos(pitch) * std::cos(yaw),
                std::sin(pitch),
                std::cos(pitch) * std::sin(yaw));
            forward = glm::normalize(forward);

            const Vector3 worldUp = Vector3(0.0f, 1.0f, 0.0f);
            Vector3 right = glm::normalize(glm::cross(forward, worldUp));
            Vector3 up = glm::normalize(glm::cross(right, forward));

            if (rightClickNavigation)
            {
                float moveSpeed = m_editorCamera3DMoveSpeed * _deltaTime;
                if (input.GetKey(Canis::Key::LSHIFT) || input.GetKey(Canis::Key::RSHIFT))
                    moveSpeed *= 3.0f;

                if (input.GetKey(Canis::Key::W))
                    m_editorCamera3DPosition += forward * moveSpeed;
                if (input.GetKey(Canis::Key::S))
                    m_editorCamera3DPosition -= forward * moveSpeed;
                if (input.GetKey(Canis::Key::A))
                    m_editorCamera3DPosition -= right * moveSpeed;
                if (input.GetKey(Canis::Key::D))
                    m_editorCamera3DPosition += right * moveSpeed;
                if (input.GetKey(Canis::Key::Q))
                    m_editorCamera3DPosition -= worldUp * moveSpeed;
                if (input.GetKey(Canis::Key::E))
                    m_editorCamera3DPosition += worldUp * moveSpeed;
            }

            const Matrix4 view = glm::lookAt(m_editorCamera3DPosition, m_editorCamera3DPosition + forward, up);
            const float aspect = static_cast<float>(renderWidth) / static_cast<float>(renderHeight);
            const Matrix4 projection = glm::perspective(DEG2RAD * m_editorCamera3DFovDegrees, aspect, 0.05f, 2000.0f);

            m_scene->SetEditorCamera3DOverride(view, projection);
        }
        else
        {
            if (rightClickNavigation)
            {
                m_editorCamera2DPosition.x -= input.mouseRel.x;
                m_editorCamera2DPosition.y += input.mouseRel.y;
            }

            Matrix4 projection = glm::ortho(0.0f, static_cast<float>(renderWidth), 0.0f,
                                            static_cast<float>(renderHeight), 0.0f, 100.0f);
            Matrix4 view = Matrix4(1.0f);
            view = glm::translate(view, Vector3(-m_editorCamera2DPosition.x + renderWidth * 0.5f,
                                                -m_editorCamera2DPosition.y + renderHeight * 0.5f, 0.0f));
            view = glm::scale(view, Vector3(m_editorCamera2DScale, m_editorCamera2DScale, 0.0f));
            m_scene->SetEditorCamera2DOverride(projection * view, m_editorCamera2DPosition);
        }
    }

    void Editor::RenderGameDebug()
    {
#if CANIS_EDITOR
        if (!m_scene)
            return;

        Camera2D *camera2D = nullptr;
        std::vector<Entity *> &entities = m_scene->GetEntities();

        for (Entity *entity : entities)
        {
            if (entity == nullptr)
                continue;

            Camera2D *camera = (entity != nullptr && entity->HasComponent<Camera2D>() ? &entity->GetComponent<Camera2D>() : nullptr);
            if (camera)
            {
                camera2D = camera;
                break;
            }
        }

        if (!camera2D)
            return;

        DrawSelectionMouseDebug(camera2D);

        if (m_index >= 0 && m_index < m_scene->GetEntities().size() && m_scene->GetEntities()[m_index] != nullptr)
        {
            Entity &selected = *m_scene->GetEntities()[m_index];
            if (selected.HasComponent<RectTransform>())
                DrawBoundingBox(camera2D);
        }
        
        //ImGui::Render();
        //ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
    }

    void Editor::StopPlayMode()
    {
        if (m_mode != EditorMode::PLAY && m_mode != EditorMode::PAUSE)
            return;

        if (!g_lastPlaySceneNode || m_scene == nullptr || m_app == nullptr)
            return;

        Time::SetTargetFPS(Canis::GetProjectConfig().frameLimitEditor + 0.0f);
        m_mode = EditorMode::EDIT;
        m_scene->Unload();
        m_scene->LoadSceneNode(m_app->GetScriptRegistry(), g_lastPlaySceneNode);
    }

    void Editor::DrawSceneView()
    {
        ImGui::Begin("Scene");

        ImVec2 avail = ImGui::GetContentRegionAvail();
        int nextWidth = static_cast<int>(avail.x);
        int nextHeight = static_cast<int>(avail.y);
        bool hovered = false;

        if (nextWidth > 0 && nextHeight > 0)
        {
            ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
            ImGuiViewport *viewport = ImGui::GetWindowViewport();

            if (viewport)
                m_gameViewportId = viewport->ID;

            if (nextWidth != m_gameViewportWidth || nextHeight != m_gameViewportHeight)
            {
                m_gameViewportWidth = nextWidth;
                m_gameViewportHeight = nextHeight;
            }

            if (m_gameColorTexture != 0)
            {
                float targetW = static_cast<float>((m_gameTextureWidth > 0) ? m_gameTextureWidth : 1);
                float targetH = static_cast<float>((m_gameTextureHeight > 0) ? m_gameTextureHeight : 1);
                float targetAspect = targetW / targetH;
                float availAspect = (avail.y > 0.0f) ? (avail.x / avail.y) : targetAspect;

                ImVec2 drawSize = avail;
                if (availAspect > targetAspect) {
                    drawSize.x = avail.y * targetAspect; // letterbox left/right
                } else {
                    drawSize.y = avail.x / targetAspect; // letterbox top/bottom
                }

                ImVec2 cursor = ImGui::GetCursorPos();
                ImVec2 offset((avail.x - drawSize.x) * 0.5f, (avail.y - drawSize.y) * 0.5f);
                m_gameViewportPosX = cursorScreen.x + offset.x;
                m_gameViewportPosY = cursorScreen.y + offset.y;
                m_gameViewportDrawWidth = drawSize.x;
                m_gameViewportDrawHeight = drawSize.y;
                ImGui::SetCursorPos(ImVec2(cursor.x + offset.x, cursor.y + offset.y));

                ImGui::Image(
                    (ImTextureID)(intptr_t)m_gameColorTexture,
                    drawSize,
                    ImVec2(0.0f, 1.0f),
                    ImVec2(1.0f, 0.0f));
                hovered = ImGui::IsItemHovered();
                DrawSceneViewGizmo();
            }
            else
            {
                m_gameViewportPosX = 0.0f;
                m_gameViewportPosY = 0.0f;
                m_gameViewportDrawWidth = 0.0f;
                m_gameViewportDrawHeight = 0.0f;
                ImGui::Text("Game view unavailable.");
            }
        }
        else
        {
            m_gameViewportPosX = 0.0f;
            m_gameViewportPosY = 0.0f;
            m_gameViewportDrawWidth = 0.0f;
            m_gameViewportDrawHeight = 0.0f;
            m_gameViewportWidth = 0;
            m_gameViewportHeight = 0;
        }

        m_gameViewHovered = hovered;
        ImGui::End();
    }

    void Editor::DrawGameView()
    {
        ImGui::Begin("Game");

        m_playViewportPosX = 0.0f;
        m_playViewportPosY = 0.0f;
        m_playViewportDrawWidth = 0.0f;
        m_playViewportDrawHeight = 0.0f;

        if (ImGuiViewport *viewport = ImGui::GetWindowViewport())
        {
            if (viewport->PlatformHandle != nullptr)
            {
                SDL_Window *viewportWindow = static_cast<SDL_Window *>(viewport->PlatformHandle);
                if (viewportWindow != nullptr)
                    m_gameInputWindowID = SDL_GetWindowID(viewportWindow);
            }
        }

        ImVec2 avail = ImGui::GetContentRegionAvail();
        int nextWidth = static_cast<int>(avail.x);
        int nextHeight = static_cast<int>(avail.y);

        if (nextWidth != m_playViewportWidth || nextHeight != m_playViewportHeight)
        {
            m_playViewportWidth = std::max(0, nextWidth);
            m_playViewportHeight = std::max(0, nextHeight);
        }

        if (nextWidth > 0 && nextHeight > 0)
        {
            if (m_playColorTexture != 0)
            {
                ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
                float targetW = static_cast<float>((m_playTextureWidth > 0) ? m_playTextureWidth : 1);
                float targetH = static_cast<float>((m_playTextureHeight > 0) ? m_playTextureHeight : 1);
                float targetAspect = targetW / targetH;
                float availAspect = (avail.y > 0.0f) ? (avail.x / avail.y) : targetAspect;

                ImVec2 drawSize = avail;
                if (availAspect > targetAspect) {
                    drawSize.x = avail.y * targetAspect;
                } else {
                    drawSize.y = avail.x / targetAspect;
                }

                ImVec2 cursor = ImGui::GetCursorPos();
                ImVec2 offset((avail.x - drawSize.x) * 0.5f, (avail.y - drawSize.y) * 0.5f);
                m_playViewportPosX = cursorScreen.x + offset.x;
                m_playViewportPosY = cursorScreen.y + offset.y;
                m_playViewportDrawWidth = drawSize.x;
                m_playViewportDrawHeight = drawSize.y;
                ImGui::SetCursorPos(ImVec2(cursor.x + offset.x, cursor.y + offset.y));

                ImGui::Image(
                    (ImTextureID)(intptr_t)m_playColorTexture,
                    drawSize,
                    ImVec2(0.0f, 1.0f),
                    ImVec2(1.0f, 0.0f));

                if (m_scene != nullptr)
                {
                    float logicalWidth = static_cast<float>((m_playTextureWidth > 0) ? m_playTextureWidth : m_playViewportWidth);
                    float logicalHeight = static_cast<float>((m_playTextureHeight > 0) ? m_playTextureHeight : m_playViewportHeight);
                    logicalWidth = std::max(1.0f, logicalWidth);
                    logicalHeight = std::max(1.0f, logicalHeight);

                    float localViewportPosX = m_playViewportPosX;
                    float localViewportPosY = m_playViewportPosY;
                    if (ImGuiViewport *view = ImGui::GetWindowViewport())
                    {
                        localViewportPosX -= view->Pos.x;
                        localViewportPosY -= view->Pos.y;
                    }

                    m_scene->GetInputManager().SetGameMouseViewport(
                        localViewportPosX,
                        localViewportPosY,
                        m_playViewportDrawWidth,
                        m_playViewportDrawHeight,
                        logicalWidth,
                        logicalHeight);
                }
            }
            else
            {
                if (m_scene != nullptr)
                    m_scene->GetInputManager().ClearGameMouseViewport();
                ImGui::Text("Game view unavailable.");
            }
        }
        else if (m_scene != nullptr)
        {
            m_scene->GetInputManager().ClearGameMouseViewport();
        }

        ImGui::End();
    }

    void Editor::DrawSceneViewGizmo()
    {
        if (m_gameViewportWidth <= 0 || m_gameViewportHeight <= 0)
            return;

        if (m_index < 0 || m_index >= m_scene->GetEntities().size())
            return;

        Entity *selected = m_scene->GetEntities()[m_index];
        if (!selected)
            return;

        float rectW = (m_gameViewportDrawWidth > 0.0f) ? m_gameViewportDrawWidth : static_cast<float>(m_gameViewportWidth);
        float rectH = (m_gameViewportDrawHeight > 0.0f) ? m_gameViewportDrawHeight : static_cast<float>(m_gameViewportHeight);

        // Keep ImGuizmo's internal helper window attached to the Scene viewport.
        // This preserves gizmo interaction while preventing it from showing up as its own platform window.
        ImGuiWindow* sceneWindow = ImGui::GetCurrentWindow();
        if (sceneWindow != nullptr && sceneWindow->Viewport != nullptr)
            ImGui::SetNextWindowViewport(sceneWindow->Viewport->ID);

        ImGuiWindowClass gizmoWindowClass = {};
        gizmoWindowClass.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoTaskBarIcon;
        gizmoWindowClass.ViewportFlagsOverrideClear = ImGuiViewportFlags_NoAutoMerge;
        ImGui::SetNextWindowClass(&gizmoWindowClass);
        ImGuizmo::BeginFrame();

        ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
        ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());
        ImGuizmo::SetRect(m_gameViewportPosX, m_gameViewportPosY, rectW, rectH);
        ImGuizmo::Enable(true);

        if (Transform *transform3D = (selected != nullptr && selected->HasComponent<Transform>() ? &selected->GetComponent<Transform>() : nullptr))
        {
            const bool useEditorSceneCamera =
                m_mode != EditorMode::HIDDEN &&
                m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_3D;

            if (!useEditorSceneCamera)
                return;

            // Overrides are cleared before UI draw; keep using the last scene-camera matrices.
            Matrix4 projection = m_scene->GetEditorCamera3DProjection();
            Matrix4 view = m_scene->GetEditorCamera3DView();

            Matrix4 model = transform3D->GetModelMatrix();

            ImGuizmo::SetOrthographic(false);
            static ImGuizmo::OPERATION operation3D = ImGuizmo::TRANSLATE;
            if (!ImGui::GetIO().WantTextInput)
            {
                if (ImGui::IsKeyPressed(ImGuiKey_W))
                    operation3D = ImGuizmo::TRANSLATE;
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                    operation3D = ImGuizmo::ROTATE;
                if (ImGui::IsKeyPressed(ImGuiKey_R))
                    operation3D = ImGuizmo::SCALE;
            }

            ImGuizmo::Manipulate(
                glm::value_ptr(view),
                glm::value_ptr(projection),
                operation3D,
                (ImGuizmo::MODE)m_guizmoMode,
                glm::value_ptr(model));

            if (ImGuizmo::IsUsing())
            {
                float t[3], r[3], s[3];
                ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), t, r, s);

                const Vector3 worldPosition(t[0], t[1], t[2]);
                const Vector3 worldRotation(DEG2RAD * r[0], DEG2RAD * r[1], DEG2RAD * r[2]);
                const Vector3 worldScale(s[0], s[1], s[2]);

                if (transform3D->parent != nullptr)
                {
                    if (transform3D->parent->HasComponent<Transform>())
                    {
                        Transform& parentTransform = transform3D->parent->GetComponent<Transform>();
                        const Vector3 parentWorldPosition = parentTransform.GetGlobalPosition();
                        const Vector3 parentWorldRotation = parentTransform.GetGlobalRotation();
                        const Vector3 parentWorldScale = parentTransform.GetGlobalScale();

                        const Vector3 parentSpacePosition = worldPosition - parentWorldPosition;
                        Matrix4 inverseParentRotation = Matrix4(1.0f);
                        inverseParentRotation = glm::rotate(inverseParentRotation, -parentWorldRotation.x, Vector3(1.0f, 0.0f, 0.0f));
                        inverseParentRotation = glm::rotate(inverseParentRotation, -parentWorldRotation.y, Vector3(0.0f, 1.0f, 0.0f));
                        inverseParentRotation = glm::rotate(inverseParentRotation, -parentWorldRotation.z, Vector3(0.0f, 0.0f, 1.0f));
                        const Vector4 localPosition4 = inverseParentRotation * Vector4(
                            parentSpacePosition.x,
                            parentSpacePosition.y,
                            parentSpacePosition.z,
                            0.0f);

                        transform3D->position.x = (parentWorldScale.x != 0.0f) ? (localPosition4.x / parentWorldScale.x) : localPosition4.x;
                        transform3D->position.y = (parentWorldScale.y != 0.0f) ? (localPosition4.y / parentWorldScale.y) : localPosition4.y;
                        transform3D->position.z = (parentWorldScale.z != 0.0f) ? (localPosition4.z / parentWorldScale.z) : localPosition4.z;

                        transform3D->rotation = worldRotation - parentWorldRotation;
                        transform3D->scale.x = (parentWorldScale.x != 0.0f) ? (worldScale.x / parentWorldScale.x) : worldScale.x;
                        transform3D->scale.y = (parentWorldScale.y != 0.0f) ? (worldScale.y / parentWorldScale.y) : worldScale.y;
                        transform3D->scale.z = (parentWorldScale.z != 0.0f) ? (worldScale.z / parentWorldScale.z) : worldScale.z;
                    }
                    else
                    {
                        transform3D->position = worldPosition;
                        transform3D->rotation = worldRotation;
                        transform3D->scale = worldScale;
                    }
                }
                else
                {
                    transform3D->position = worldPosition;
                    transform3D->rotation = worldRotation;
                    transform3D->scale = worldScale;
                }
            }

            return;
        }

        RectTransform *rtc = (selected != nullptr && selected->HasComponent<RectTransform>() ? &selected->GetComponent<RectTransform>() : nullptr);
        if (!rtc)
            return;

        const bool useEditorSceneCamera2D =
            m_mode != EditorMode::HIDDEN &&
            m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_2D;

        if (!useEditorSceneCamera2D)
            return;

        const float renderWidth = std::max(1.0f, (m_gameTextureWidth > 0)
            ? static_cast<float>(m_gameTextureWidth)
            : static_cast<float>((m_gameViewportWidth > 0) ? m_gameViewportWidth : m_window->GetWindowWidth()));
        const float renderHeight = std::max(1.0f, (m_gameTextureHeight > 0)
            ? static_cast<float>(m_gameTextureHeight)
            : static_cast<float>((m_gameViewportHeight > 0) ? m_gameViewportHeight : m_window->GetWindowHeight()));

        Matrix4 projection = glm::ortho(0.0f, renderWidth, 0.0f, renderHeight, 0.0f, 100.0f);

        Vector2 pos = rtc->GetPosition();
        Vector2 globalScale = rtc->GetScale();

        Matrix4 model = Matrix4(1.0f);
        model = glm::translate(model, Vector3(pos.x, pos.y, 0.0f));
        model = glm::rotate(model, rtc->rotation, Vector3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, Vector3(rtc->size.x * globalScale.x, rtc->size.y * globalScale.y, 1.0f));

        Matrix4 view = Matrix4(1.0f);
        view = glm::translate(view, Vector3(-m_editorCamera2DPosition.x + renderWidth * 0.5f,
                                            -m_editorCamera2DPosition.y + renderHeight * 0.5f, 0.0f));
        // Keep camera's 2D behavior, but avoid zero Z scale for gizmo rendering.
        view = glm::scale(view, Vector3(m_editorCamera2DScale, m_editorCamera2DScale, 1.0f));

        ImGuizmo::SetOrthographic(true);

        static ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_W))
                operation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E))
                operation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R))
                operation = ImGuizmo::SCALE;
        }

        ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(projection),
            operation,
            (ImGuizmo::MODE)m_guizmoMode,
            glm::value_ptr(model));

        if (ImGuizmo::IsUsing())
        {
            float t[3], r[3], s[3];
            ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), t, r, s);

            Vector2 newPos(t[0], t[1]);
            rtc->SetPosition(newPos);

            rtc->rotation = DEG2RAD * r[2];

            rtc->SetScale(Vector2(s[0] / rtc->size.x, s[1] / rtc->size.y));
        }
    }

    void Editor::EnsureGameRenderTarget(int _width, int _height)
    {
        if (_width <= 0 || _height <= 0)
            return;

        if (m_gameFramebuffer != 0 &&
            _width == m_gameTextureWidth &&
            _height == m_gameTextureHeight)
        {
            return;
        }

        DestroyGameRenderTarget();

        glGenFramebuffers(1, &m_gameFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_gameFramebuffer);

        glGenTextures(1, &m_gameColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_gameColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_gameColorTexture, 0);

        glGenRenderbuffers(1, &m_gameDepthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_gameDepthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, _width, _height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_gameDepthRbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            Debug::Log("Game framebuffer incomplete.");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_gameTextureWidth = _width;
        m_gameTextureHeight = _height;
    }

    void Editor::EnsurePlayRenderTarget(int _width, int _height)
    {
        if (_width <= 0 || _height <= 0)
            return;

        if (m_playFramebuffer != 0 &&
            _width == m_playTextureWidth &&
            _height == m_playTextureHeight)
        {
            return;
        }

        DestroyPlayRenderTarget();

        glGenFramebuffers(1, &m_playFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, m_playFramebuffer);

        glGenTextures(1, &m_playColorTexture);
        glBindTexture(GL_TEXTURE_2D, m_playColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_playColorTexture, 0);

        glGenRenderbuffers(1, &m_playDepthRbo);
        glBindRenderbuffer(GL_RENDERBUFFER, m_playDepthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, _width, _height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_playDepthRbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            Debug::Log("Play framebuffer incomplete.");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        m_playTextureWidth = _width;
        m_playTextureHeight = _height;
    }

    void Editor::DestroyGameRenderTarget()
    {
        if (m_gameDepthRbo != 0)
        {
            glDeleteRenderbuffers(1, &m_gameDepthRbo);
            m_gameDepthRbo = 0;
        }

        if (m_gameColorTexture != 0)
        {
            glDeleteTextures(1, &m_gameColorTexture);
            m_gameColorTexture = 0;
        }

        if (m_gameFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_gameFramebuffer);
            m_gameFramebuffer = 0;
        }

        m_gameTextureWidth = 0;
        m_gameTextureHeight = 0;
    }

    void Editor::DestroyPlayRenderTarget()
    {
        if (m_playDepthRbo != 0)
        {
            glDeleteRenderbuffers(1, &m_playDepthRbo);
            m_playDepthRbo = 0;
        }

        if (m_playColorTexture != 0)
        {
            glDeleteTextures(1, &m_playColorTexture);
            m_playColorTexture = 0;
        }

        if (m_playFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &m_playFramebuffer);
            m_playFramebuffer = 0;
        }

        m_playTextureWidth = 0;
        m_playTextureHeight = 0;
    }

    void Editor::FocusEntity(Canis::Entity *_entity)
    {
        for (int i = 0; i < m_scene->GetEntities().size(); i++)
        {
            if (m_scene->GetEntities()[i] == _entity)
            {
                m_index = i;
                m_selectedAssetPath.clear();
                return;
            }
        }
    }

    void Editor::InputEntity(const std::string &_name, Canis::Entity *&_variable)
    {
        InputEntity(_name, nullptr, _variable);
    }

    void Editor::InputEntity(const std::string &_name, const char *_idSuffix, Canis::Entity *&_variable)
    {
        ImGui::PushID(GetInspectorFieldID(_name.c_str(), _idSuffix));
        ImGui::Text("%s", _name.c_str());
        
        ImGui::SameLine();

        std::string label;
        Canis::Entity *entity = *&_variable;
        if (entity)
            label = "[entity] " + entity->name;
        else
            label = "[ missing entity ]";

        ImGui::Button(label.c_str(), ImVec2(150, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
            {
                const Canis::UUID dropped = *static_cast<const Canis::UUID *>(payload->Data);
                Canis::Entity *e = m_scene->GetEntityWithUUID(dropped);

                if (e)
                    *&_variable = e;
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (entity)
                FocusEntity(entity);
        }

        if (entity && ImGui::BeginPopupContextItem("ctx"))
        {
            if (ImGui::MenuItem("Clear"))
                *&_variable = nullptr;

            if (ImGui::MenuItem("Select in Hierarchy"))
                FocusEntity(entity);

            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    void Editor::InputAnimationClip(const std::string& _name, Canis::AnimationClip2DID &_variable)
    {
        InputAnimationClip(_name, nullptr, _variable);
    }

    void Editor::InputAnimationClip(const std::string& _name, const char *_idSuffix, Canis::AnimationClip2DID &_variable)
    {
        ImGui::PushID(GetInspectorFieldID(_name.c_str(), _idSuffix));
        ImGui::Text("%s", _name.c_str());

        ImGui::SameLine();

        const char* empty = "[ empty ]";

        if (auto *meta = AssetManager::GetMetaFile(AssetManager::GetPath(_variable)))
            ImGui::Button(meta->name.c_str(), ImVec2(150, 0));
        else
            ImGui::Button(empty, ImVec2(150, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                std::string path = AssetManager::GetPath(dropped.uuid);
                SpriteAnimationAsset* asset = AssetManager::GetSpriteAnimation(path);

                if (asset)
                {
                    _variable = AssetManager::GetID(path);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    }

    void Editor::InputSceneAsset(const std::string &_name, Canis::SceneAssetHandle &_variable)
    {
        InputSceneAsset(_name, nullptr, _variable);
    }

    void Editor::InputSceneAsset(const std::string &_name, const char *_idSuffix, Canis::SceneAssetHandle &_variable)
    {
        ImGui::PushID(GetInspectorFieldID(_name.c_str(), _idSuffix));
        ImGui::Text("%s", _name.c_str());
        ImGui::SameLine();

        std::string label = "[ none ]";
        if (!_variable.path.empty())
        {
            if (MetaFileAsset *meta = AssetManager::GetMetaFile(_variable.path))
                label = meta->name;
            else
                label = _variable.path;
        }

        ImGui::Button(label.c_str(), ImVec2(170, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                const std::string path = AssetManager::GetPath(dropped.uuid);

                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::SCENE)
                        _variable.path = path;
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("scene_asset_ctx"))
        {
            if (ImGui::MenuItem("Clear"))
                _variable.path.clear();

            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X##clear_scene_asset"))
            _variable.path.clear();

        ImGui::PopID();
    }

    bool Editor::IsDescendantOf(Canis::Entity *_parent, Canis::Entity *_potentialChild)
    {
        if (!_parent || !_potentialChild)
            return false;

        std::vector<Canis::Entity*>* children = nullptr;
        if (auto *parentRT = (_parent != nullptr && _parent->HasComponent<RectTransform>() ? &_parent->GetComponent<RectTransform>() : nullptr))
            children = &parentRT->children;
        else if (auto *parentTransform = (_parent != nullptr && _parent->HasComponent<Transform>() ? &_parent->GetComponent<Transform>() : nullptr))
            children = &parentTransform->children;

        if (children == nullptr)
            return false;

        for (auto *child : *children)
        {
            if (!child)
                continue;
            if (child == _potentialChild)
                return true;
            if (IsDescendantOf(child, _potentialChild))
                return true;
        }

        return false;
    }

    static std::vector<Canis::Entity*>* GetHierarchyChildren(Canis::Entity *_entity)
    {
        if (_entity == nullptr)
            return nullptr;

        if (Canis::RectTransform* transform = (_entity != nullptr && _entity->HasComponent<RectTransform>() ? &_entity->GetComponent<RectTransform>() : nullptr))
            return &transform->children;

        if (Canis::Transform* transform = (_entity != nullptr && _entity->HasComponent<Transform>() ? &_entity->GetComponent<Transform>() : nullptr))
            return &transform->children;

        return nullptr;
    }

    static Canis::Entity* GetHierarchyParent(Canis::Entity *_entity)
    {
        if (_entity == nullptr)
            return nullptr;

        if (Canis::RectTransform* transform = (_entity != nullptr && _entity->HasComponent<RectTransform>() ? &_entity->GetComponent<RectTransform>() : nullptr))
            return transform->parent;

        if (Canis::Transform* transform = (_entity != nullptr && _entity->HasComponent<Transform>() ? &_entity->GetComponent<Transform>() : nullptr))
            return transform->parent;

        return nullptr;
    }

    static bool SetHierarchyParent(Canis::Entity *_child, Canis::Entity *_parent)
    {
        if (_child == nullptr)
            return false;

        if (Canis::RectTransform* childTransform = (_child != nullptr && _child->HasComponent<RectTransform>() ? &_child->GetComponent<RectTransform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<RectTransform>())
                return false;

            childTransform->SetParent(_parent);
            return true;
        }

        if (Canis::Transform* childTransform = (_child != nullptr && _child->HasComponent<Transform>() ? &_child->GetComponent<Transform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<Transform>())
                return false;

            childTransform->SetParent(_parent);
            return true;
        }

        return false;
    }

    static bool SetHierarchyParentAtIndex(Canis::Entity *_child, Canis::Entity *_parent, std::size_t _index)
    {
        if (_child == nullptr)
            return false;

        if (Canis::RectTransform* childTransform = (_child != nullptr && _child->HasComponent<RectTransform>() ? &_child->GetComponent<RectTransform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<RectTransform>())
                return false;

            childTransform->SetParentAtIndex(_parent, _index);
            return true;
        }

        if (Canis::Transform* childTransform = (_child != nullptr && _child->HasComponent<Transform>() ? &_child->GetComponent<Transform>() : nullptr))
        {
            if (_parent != nullptr && !_parent->HasComponent<Transform>())
                return false;

            childTransform->SetParentAtIndex(_parent, _index);
            return true;
        }

        return false;
    }

    static bool UnparentHierarchyEntity(Canis::Entity *_entity)
    {
        if (_entity == nullptr)
            return false;

        if (Canis::RectTransform* transform = (_entity != nullptr && _entity->HasComponent<RectTransform>() ? &_entity->GetComponent<RectTransform>() : nullptr))
        {
            if (transform->parent == nullptr)
                return false;

            transform->SetParent(nullptr);
            return true;
        }

        if (Canis::Transform* transform = (_entity != nullptr && _entity->HasComponent<Transform>() ? &_entity->GetComponent<Transform>() : nullptr))
        {
            if (transform->parent == nullptr)
                return false;

            transform->SetParent(nullptr);
            return true;
        }

        return false;
    }

    static void GetHierarchyChildrenRecursive(Canis::Entity* _entity, std::vector<Canis::Entity*> &_entities)
    {
        std::vector<Canis::Entity*>* children = GetHierarchyChildren(_entity);
        if (children == nullptr)
            return;

        for (std::size_t i = 0; i < children->size(); ++i)
        {
            Canis::Entity* e = (*children)[i];
            if (!e)
                continue;

            _entities.push_back(e);

            GetHierarchyChildrenRecursive(e, _entities);
        }
    }

    void Editor::DrawHierarchyNode(Canis::Entity *_entity, std::vector<Canis::Entity *> &_entities, bool &_refresh)
    {
        if (!_entity)
            return;

        std::vector<Canis::Entity*> *children = GetHierarchyChildren(_entity);

        bool isSelected = (m_index >= 0 && m_index < (int)_entities.size() &&
                           _entities[m_index] == _entity);

        bool hasChildren = (children != nullptr && !children->empty());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (isSelected)
            flags |= ImGuiTreeNodeFlags_Selected;

        std::string label = _entity->name + "##" + std::to_string(_entity->uuid);
        bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);

        // select on click
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            for (int i = 0; i < (int)_entities.size(); ++i)
            {
                if (_entities[i] == _entity)
                {
                    m_index = i;
                    m_selectedAssetPath.clear();
                    _refresh = true;
                    break;
                }
            }
        }

        // drag source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            Canis::UUID uuid = _entity->uuid;
            ImGui::SetDragDropPayload("ENTITY_DRAG", &uuid, sizeof(Canis::UUID));
            ImGui::Text("Entity: %s", _entity->name.c_str());
            ImGui::EndDragDropSource();
        }

        // drop ON node = parent and append at end
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
            {
                Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);

                Canis::Entity *droppedEntity = nullptr;
                for (auto *e : _entities)
                {
                    if (e && e->uuid == droppedUUID)
                    {
                        droppedEntity = e;
                        break;
                    }
                }

                if (droppedEntity && droppedEntity != _entity)
                {
                    if (!IsDescendantOf(droppedEntity, _entity) &&
                        SetHierarchyParent(droppedEntity, _entity))
                    {
                        _refresh = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // context menu
        bool removeRequested = false;
        if (ImGui::BeginPopupContextItem())
        {
            int idx = -1;
            for (int i = 0; i < (int)_entities.size(); ++i)
            {
                if (_entities[i] == _entity)
                {
                    idx = i;
                    break;
                }
            }

            if (ImGui::MenuItem("Create"))
                m_scene->CreateEntity();

            if (idx >= 0 && ImGui::MenuItem("Duplicate"))
            {
                Debug::Log("Duplicate");
                Canis::Entity *selected = _entities[idx];

                std::vector<Canis::Entity*> entities;
                entities.push_back(selected);

                // get all entities to duplicate
                GetHierarchyChildrenRecursive(selected, entities);

                // encode entities into sequence of nodes
                YAML::Node nodes;
                for (Canis::Entity* e : entities)
                {
                    nodes.push_back(m_scene->EncodeEntity(m_app->GetScriptRegistry(), *e));
                }

                // option to tell it to generate new UUIDS
                m_scene->LoadEntityNodes(m_app->GetScriptRegistry(), nodes, false);
            }

            if (idx >= 0 && ImGui::MenuItem("Remove"))
            {
                m_scene->Destroy(idx);
                if (m_index == idx)
                    m_index = -1;
                _refresh = true;
                removeRequested = true;
            }

            for (auto &item : m_app->GetInspectorItemRegistry())
            {
                if (ImGui::MenuItem((item.name + "##").c_str()))
                    item.Func(*m_app, *this, *_entity, m_app->GetScriptRegistry());
            }

            ImGui::EndPopup();
        }

        if (removeRequested)
        {
            if (nodeOpen)
                ImGui::TreePop();
            return;
        }

        // children + single per-gap drop slots
        if (nodeOpen)
        {
            if (children != nullptr)
            {
                for (std::size_t ci = 0; ci < children->size(); ++ci)
                {
                    Canis::Entity *child = (*children)[ci];
                    if (!child)
                        continue;

                    // drop BEFORE this child -> specific index
                    ImGui::PushID((void *)((uintptr_t)child ^ 0xBEEF));
                    {
                        ImVec2 slotSize(std::max(ImGui::GetContentRegionAvail().x, 1.0f), 1.0f);
                        ImGui::InvisibleButton("##drop_before", slotSize);

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
                            {
                                Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);

                                Canis::Entity *droppedEntity = nullptr;
                                for (auto *e2 : _entities)
                                {
                                    if (e2 && e2->uuid == droppedUUID)
                                    {
                                        droppedEntity = e2;
                                        break;
                                    }
                                }

                                if (droppedEntity && droppedEntity != _entity)
                                {
                                    if (!IsDescendantOf(droppedEntity, _entity) &&
                                        SetHierarchyParentAtIndex(droppedEntity, _entity, ci))
                                    {
                                        _refresh = true;
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                    ImGui::PopID();

                    // actual child row
                    DrawHierarchyNode(child, _entities, _refresh);
                    if (_refresh)
                        break;
                }
            }

            ImGui::TreePop();
        }
    }

    bool Editor::DrawHierarchyPanel()
    {
        ImGui::Begin("Hierarchy");
        bool refresh = false;

        std::vector<Canis::Entity *> &entities = m_scene->GetEntities();

        auto findEntityByUUID = [&](Canis::UUID _uuid) -> Canis::Entity*
        {
            for (Canis::Entity* entity : entities)
            {
                if (entity != nullptr && entity->uuid == _uuid)
                    return entity;
            }

            return nullptr;
        };

        // Build root list in stable editor-controlled order.
        std::vector<Canis::Entity*> rootsBySceneOrder = {};
        rootsBySceneOrder.reserve(entities.size());

        for (int i = 0; i < (int)entities.size(); ++i)
        {
            Canis::Entity *entity = entities[i];
            if (!entity)
                continue;

            if (GetHierarchyParent(entity) != nullptr)
                continue;

            rootsBySceneOrder.push_back(entity);
        }

        std::vector<Canis::UUID> rootOrderThisFrame = {};
        rootOrderThisFrame.reserve(rootsBySceneOrder.size());

        // Keep roots from previously saved order if they still exist and are roots.
        for (Canis::UUID orderedUUID : m_hierarchyRootOrder)
        {
            Canis::Entity* entity = findEntityByUUID(orderedUUID);
            if (entity == nullptr || GetHierarchyParent(entity) != nullptr)
                continue;

            if (std::find(rootOrderThisFrame.begin(), rootOrderThisFrame.end(), entity->uuid) == rootOrderThisFrame.end())
                rootOrderThisFrame.push_back(entity->uuid);
        }

        // Append new roots not yet tracked.
        for (Canis::Entity* entity : rootsBySceneOrder)
        {
            if (std::find(rootOrderThisFrame.begin(), rootOrderThisFrame.end(), entity->uuid) == rootOrderThisFrame.end())
                rootOrderThisFrame.push_back(entity->uuid);
        }

        m_hierarchyRootOrder = rootOrderThisFrame;

        std::vector<Canis::Entity *> rootEntities = {};
        rootEntities.reserve(rootOrderThisFrame.size());
        for (Canis::UUID rootUUID : rootOrderThisFrame)
        {
            if (Canis::Entity* entity = findEntityByUUID(rootUUID))
            {
                if (GetHierarchyParent(entity) == nullptr)
                    rootEntities.push_back(entity);
            }
        }

        auto moveRootToPos = [&](Canis::Entity *droppedEntity, int targetRootPos)
        {
            if (!droppedEntity)
                return;

            bool changed = false;

            if (GetHierarchyParent(droppedEntity) != nullptr)
            {
                if (UnparentHierarchyEntity(droppedEntity))
                    changed = true;
            }

            std::vector<Canis::UUID> updatedOrder = rootOrderThisFrame;

            if (auto it = std::find(updatedOrder.begin(), updatedOrder.end(), droppedEntity->uuid); it != updatedOrder.end())
                updatedOrder.erase(it);

            targetRootPos = std::clamp(targetRootPos, 0, static_cast<int>(updatedOrder.size()));
            updatedOrder.insert(updatedOrder.begin() + targetRootPos, droppedEntity->uuid);

            if (updatedOrder != rootOrderThisFrame)
            {
                rootOrderThisFrame = updatedOrder;
                m_hierarchyRootOrder = rootOrderThisFrame;
                refresh = true;
                changed = true;
            }

            if (changed)
                refresh = true;
        };

        // ---------- TOP ROOT DROP SLOT (move to front) ----------
        {
            ImVec2 slotSize(std::max(ImGui::GetContentRegionAvail().x, 1.0f), 1.0f);
            ImGui::InvisibleButton("##root_drop_before_first", slotSize);

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
                {
                    Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);
                    Canis::Entity *droppedEntity = nullptr;
                    for (auto *e : entities)
                    {
                        if (e && e->uuid == droppedUUID)
                        {
                            droppedEntity = e;
                            break;
                        }
                    }
                    moveRootToPos(droppedEntity, 0); // move to first root
                }
                ImGui::EndDragDropTarget();
            }
        }

        // ---------- ROOT ENTITIES + BETWEEN-SLOTS ----------
        for (int ri = 0; ri < (int)rootEntities.size(); ++ri)
        {
            Canis::Entity *entity = rootEntities[ri];
            if (!entity)
                continue;

            DrawHierarchyNode(entity, entities, refresh);
            if (refresh)
                break;

            // drop slot AFTER this root -> position ri+1
            ImGui::PushID((void *)((uintptr_t)entity ^ 0xABCDEF));
            ImVec2 slotSize(std::max(ImGui::GetContentRegionAvail().x, 1.0f), 1.0f);
            ImGui::InvisibleButton("##root_drop_after", slotSize);

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
                {
                    Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);
                    Canis::Entity *droppedEntity = nullptr;
                    for (auto *e : entities)
                    {
                        if (e && e->uuid == droppedUUID)
                        {
                            droppedEntity = e;
                            break;
                        }
                    }
                    moveRootToPos(droppedEntity, ri + 1);
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();
        }

        // --------- ROOT DROP ZONE (unparent children) ----------
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.y < 24.0f)
            avail.y = 24.0f;

        avail.x = std::max(avail.x, 1.0f);
        avail.y = std::max(avail.y, 1.0f);
        ImGui::InvisibleButton("##hierarchy_root_drop_zone", avail);

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_DRAG"))
            {
                Canis::UUID droppedUUID = *static_cast<const Canis::UUID *>(payload->Data);

                Canis::Entity *droppedEntity = nullptr;
                for (auto *e : entities)
                {
                    if (e && e->uuid == droppedUUID)
                    {
                        droppedEntity = e;
                        break;
                    }
                }

                if (droppedEntity)
                {
                    moveRootToPos(droppedEntity, static_cast<int>(rootOrderThisFrame.size()));
                }
            }
            ImGui::EndDragDropTarget();
        }
        // -----------------------------------------------------

        ImGui::End();
        return refresh;
    }

    void Editor::DrawInspectorPanel(bool _refresh)
    {
        ImGui::Begin("Inspector");

        if (!m_selectedAssetPath.empty())
        {
            if (DrawMaterialAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }

            if (DrawSkyboxAssetInspector(m_selectedAssetPath))
            {
                ImGui::End();
                return;
            }
        }

        std::vector<Entity *> &entities = m_scene->GetEntities();

        if (entities.empty())
        {
            m_index = -1;
            ImGui::End();
            return;
        }

        if (m_index < 0 || m_index >= (int)entities.size())
            m_index = 0;

        if (entities[m_index] == nullptr)
        {
            m_index = -1;
            for (int i = 0; i < (int)entities.size(); ++i)
            {
                if (entities[i] != nullptr)
                {
                    m_index = i;
                    break;
                }
            }
        }

        if (m_index >= 0 && entities[m_index] != nullptr)
        {
            Entity &entity = *entities[m_index];

            ImGui::Text("active:");
            ImGui::SameLine();
            ImGui::Checkbox("##entity_active", &entity.active);
            ImGui::Text("name: ");
            ImGui::SameLine();
            ImGui::InputText("##name", &entity.name);
            ImGui::Text("tag:  ");
            ImGui::SameLine();
            ImGui::InputText("##tag", &entity.tag);

            for (ScriptConf &conf : m_app->GetScriptRegistry())
            {
                if (conf.Has(entity))
                {
                    bool open = ImGui::CollapsingHeader(conf.name.c_str());

                    if (ImGui::BeginPopupContextItem(std::string("Menu##" + conf.name).c_str()))
                    {
                        if (ImGui::MenuItem(std::string("Remove##" + conf.name).c_str()))
                        {
                            conf.Remove(entity);
                            open = false;
                        }

                        ImGui::EndPopup();
                    }

                    if (open)
                    {
                        conf.DrawInspector(*this, entity, conf);
                    }
                }
            }

            DrawAddComponentDropDown(_refresh);
        }

        ImGui::End();
    }

    bool Editor::DrawMaterialAssetInspector(const std::string &_materialPath)
    {
        MetaFileAsset *meta = AssetManager::GetMetaFile(_materialPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::MATERIAL)
            return false;

        ImGui::Text("Asset: %s", meta->name.c_str());
        ImGui::Text("Path: %s", meta->path.c_str());
        ImGui::Separator();

        YAML::Node root = YAML::LoadFile(_materialPath);
        bool dirty = false;

        auto drawAssetField = [&](const char *_label, const char *_key, bool _shaderField) -> void
        {
            std::string refPath = ResolveAssetRefPath(root[_key]);
            std::string display = "None";
            if (!refPath.empty())
            {
                if (MetaFileAsset *assetMeta = AssetManager::GetMetaFile(refPath))
                    display = assetMeta->name;
                else
                    display = refPath;
            }

            ImGui::PushID(_key);

            ImGui::Text("%s", _label);
            ImGui::SameLine();
            const std::string buttonLabel = display + "##asset_ref";
            ImGui::Button(buttonLabel.c_str(), ImVec2(220, 0));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                    std::string path = AssetManager::GetPath(dropped.uuid);

                    bool valid = false;
                    if (_shaderField)
                    {
                        if (MetaFileAsset *droppedMeta = AssetManager::GetMetaFile(path))
                        {
                            valid = droppedMeta->type == MetaFileAsset::FileType::VERTEX ||
                                    droppedMeta->type == MetaFileAsset::FileType::FRAGMENT;
                        }
                    }
                    else
                    {
                        TextureAsset *texture = AssetManager::GetTexture(path);
                        valid = texture != nullptr;
                    }

                    if (valid)
                    {
                        SetAssetRefUUID(root, _key, path);
                        dirty = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Right-click on field to clear assigned asset reference.
            if (ImGui::BeginPopupContextItem("asset_ref_ctx"))
            {
                if (ImGui::MenuItem("Clear"))
                {
                    root.remove(_key);
                    dirty = true;
                }
                ImGui::EndPopup();
            }

            // Explicit clear button.
            ImGui::SameLine();
            if (ImGui::SmallButton("X##clear_asset_ref"))
            {
                root.remove(_key);
                dirty = true;
            }

            ImGui::PopID();
        };

        drawAssetField("shader", "shader", true);
        drawAssetField("albedo", "albedo", false);
        drawAssetField("specular", "specular", false);
        drawAssetField("roughness", "roughness", false);
        drawAssetField("metallic", "metallic", false);
        drawAssetField("emission", "emission", false);

        Color color = root["color"].as<Color>(Color(1.0f));
        if (ImGui::ColorEdit4("color", &color.r))
        {
            root["color"] = color;
            dirty = true;
        }

        float specularValue = root["specularValue"].as<float>(0.5f);
        if (ImGui::DragFloat("specularValue", &specularValue, 0.01f, 0.0f, 1.0f))
        {
            root["specularValue"] = specularValue;
            dirty = true;
        }

        float roughnessValue = root["roughnessValue"].as<float>(0.5f);
        if (ImGui::DragFloat("roughnessValue", &roughnessValue, 0.01f, 0.0f, 1.0f))
        {
            root["roughnessValue"] = roughnessValue;
            dirty = true;
        }

        float metallicValue = root["metallicValue"].as<float>(0.0f);
        if (ImGui::DragFloat("metallicValue", &metallicValue, 0.01f, 0.0f, 1.0f))
        {
            root["metallicValue"] = metallicValue;
            dirty = true;
        }

        bool backFaceCulling = root["backFaceCulling"].as<bool>(false);
        if (ImGui::Checkbox("backFaceCulling", &backFaceCulling))
        {
            root["backFaceCulling"] = backFaceCulling;
            dirty = true;
        }

        bool frontFaceCulling = root["frontFaceCulling"].as<bool>(false);
        if (ImGui::Checkbox("frontFaceCulling", &frontFaceCulling))
        {
            root["frontFaceCulling"] = frontFaceCulling;
            dirty = true;
        }

        for (const auto &entry : root)
        {
            std::string key = entry.first.as<std::string>("");
            if (key == "shader" || key == "albedo" || key == "specular" || key == "roughness" || key == "metallic" ||
                key == "emission" || key == "color" || key == "specularValue" || key == "roughnessValue" || key == "metallicValue" ||
                key == "backFaceCulling" || key == "frontFaceCulling")
            {
                continue;
            }

            if (!entry.second.IsScalar())
                continue;

            try
            {
                float value = entry.second.as<float>();
                if (ImGui::DragFloat(key.c_str(), &value, 0.01f))
                {
                    root[key] = value;
                    dirty = true;
                }
            }
            catch (const YAML::Exception &)
            {
            }
        }

        if (dirty)
        {
            std::ofstream fout(_materialPath);
            fout << root;
            fout.close();

            int id = AssetManager::GetID(_materialPath);
            if (id >= 0)
                ApplyMaterialNodeToAsset(root, AssetManager::GetMaterial(id));
        }

        return true;
    }

    bool Editor::DrawSkyboxAssetInspector(const std::string &_skyboxPath)
    {
        MetaFileAsset *meta = AssetManager::GetMetaFile(_skyboxPath);
        if (meta == nullptr || meta->type != MetaFileAsset::FileType::SKYBOX)
            return false;

        ImGui::Text("Asset: %s", meta->name.c_str());

        YAML::Node root;
        if (FileExists(_skyboxPath.c_str()))
            root = YAML::LoadFile(_skyboxPath);
        if (!root || !root.IsMap())
            root = YAML::Node(YAML::NodeType::Map);

        bool changed = false;

        auto drawFaceAssetField = [&](const char *_label, const char *_key) -> void
        {
            std::string refPath = ResolveAssetRefPath(root[_key]);
            std::string buttonLabel = "[ none ]";
            if (!refPath.empty())
            {
                if (MetaFileAsset *assetMeta = AssetManager::GetMetaFile(refPath))
                    buttonLabel = assetMeta->name;
                else
                    buttonLabel = refPath;
            }

            ImGui::Text("%s", _label);
            ImGui::SameLine();

            ImGui::PushID(_key);
            ImGui::Button(buttonLabel.c_str(), ImVec2(170, 0));

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                {
                    const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                    std::string path = AssetManager::GetPath(dropped.uuid);
                    if (MetaFileAsset *droppedMeta = AssetManager::GetMetaFile(path))
                    {
                        if (droppedMeta->type == MetaFileAsset::FileType::TEXTURE)
                        {
                            SetAssetRefUUID(root, _key, path);
                            changed = true;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::BeginPopupContextItem("skybox_asset_ref_ctx"))
            {
                if (ImGui::MenuItem("Clear"))
                {
                    root.remove(_key);
                    changed = true;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        };

        drawFaceAssetField("right (+X)", "right");
        drawFaceAssetField("left (-X)", "left");
        drawFaceAssetField("top (+Y)", "top");
        drawFaceAssetField("bottom (-Y)", "bottom");
        drawFaceAssetField("front (+Z)", "front");
        drawFaceAssetField("back (-Z)", "back");

        if (changed)
        {
            std::ofstream fout(_skyboxPath);
            fout << root;
            fout.close();

            AssetManager::Free<SkyboxAsset>(_skyboxPath);
            AssetManager::LoadSkybox(_skyboxPath);
        }

        return true;
    }

    void Editor::DrawAddComponentDropDown(bool _refresh)
    {
        if (m_index < 0 || m_index >= (int)m_scene->GetEntities().size())
            return;

        Entity *selectedEntity = m_scene->GetEntities()[m_index];
        if (selectedEntity == nullptr)
            return;

        Entity &entity = *selectedEntity;

        if (_refresh)
        {
            m_addComponentSelection = 0;
            m_addComponentSearch.clear();
            m_focusAddComponentSearch = false;
        }

        std::vector<const char *> cStringItems = ConvertComponentToCStringVector(*m_app, entity);
        const bool hasAvailableComponents = !cStringItems.empty();

        if (!hasAvailableComponents)
        {
            ImGui::BeginDisabled();
            ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f));
            ImGui::EndDisabled();
            return;
        }

        if (ImGui::Button("Add Component", ImVec2(-1.0f, 0.0f)))
        {
            m_addComponentSelection = 0;
            m_addComponentSearch.clear();
            m_focusAddComponentSearch = true;
            ImGui::OpenPopup("Add Component");
        }

        ImGuiViewport *viewport = ImGui::GetMainViewport();
        if (viewport != nullptr)
        {
            const ImVec2 popupCenter = ImVec2(
                viewport->Pos.x + (viewport->Size.x * 0.5f),
                viewport->Pos.y + (viewport->Size.y * 0.5f));
            ImGui::SetNextWindowPos(popupCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        }
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);

        if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            auto addSelectedComponent = [&](const std::vector<int> &_filteredIndices) -> bool
            {
                if (_filteredIndices.empty())
                    return false;

                const std::string componentName = cStringItems[_filteredIndices[m_addComponentSelection]];
                for (ScriptConf &conf : m_app->GetScriptRegistry())
                {
                    if (conf.name == componentName)
                    {
                        conf.Add(entity);
                        ImGui::CloseCurrentPopup();
                        m_addComponentSelection = 0;
                        m_addComponentSearch.clear();
                        return true;
                    }
                }

                return false;
            };

            auto cancelAddComponent = [&]() -> void
            {
                m_addComponentSelection = 0;
                m_addComponentSearch.clear();
                ImGui::CloseCurrentPopup();
            };

            auto matchesSearch = [&](const char *_value) -> bool
            {
                if (m_addComponentSearch.empty())
                    return true;

                std::string value = _value;
                std::string search = m_addComponentSearch;

                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                std::transform(search.begin(), search.end(), search.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return value.find(search) != std::string::npos;
            };

            if (m_focusAddComponentSearch)
            {
                ImGui::SetKeyboardFocusHere();
                m_focusAddComponentSearch = false;
            }

            ImGui::InputTextWithHint("##AddComponentSearch", "Search components...", &m_addComponentSearch);
            ImGui::Separator();

            std::vector<int> filteredIndices = {};
            filteredIndices.reserve(cStringItems.size());
            for (int i = 0; i < static_cast<int>(cStringItems.size()); ++i)
            {
                if (matchesSearch(cStringItems[i]))
                    filteredIndices.push_back(i);
            }

            if (filteredIndices.empty())
            {
                m_addComponentSelection = 0;
                ImGui::TextDisabled("No components match the current search.");
            }
            else
            {
                m_addComponentSelection = std::clamp(m_addComponentSelection, 0, static_cast<int>(filteredIndices.size()) - 1);

                ImGui::BeginChild("##AddComponentList", ImVec2(420.0f, 260.0f), true);
                for (int filteredIndex = 0; filteredIndex < static_cast<int>(filteredIndices.size()); ++filteredIndex)
                {
                    const int componentIndex = filteredIndices[filteredIndex];
                    const bool selected = (filteredIndex == m_addComponentSelection);

                    if (ImGui::Selectable(cStringItems[componentIndex], selected))
                        m_addComponentSelection = filteredIndex;

                    if (selected)
                        ImGui::SetItemDefaultFocus();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        addSelectedComponent(filteredIndices);
                }
                ImGui::EndChild();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
            {
                cancelAddComponent();
            }
            else if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
            {
                addSelectedComponent(filteredIndices);
            }

            const bool canAddComponent = !filteredIndices.empty();
            if (!canAddComponent)
                ImGui::BeginDisabled();

            if (ImGui::Button("Add", ImVec2(120.0f, 0.0f)))
                addSelectedComponent(filteredIndices);

            if (!canAddComponent)
                ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                cancelAddComponent();

            ImGui::EndPopup();
        }
    }

    void Editor::DrawEnvironment()
    {
        ImGui::Begin("Environment");
        Color background = m_window->GetClearColor();
        ImGui::ColorEdit4("Background##", &background.r);

        if (background != m_window->GetClearColor())
            m_window->SetClearColor(background);

        ImGui::Text("Skybox");
        ImGui::SameLine();

        UUID skyboxUUID = m_scene->GetEnvironmentSkyboxUUID();
        std::string skyboxLabel = "[ none ]";
        if ((uint64_t)skyboxUUID != 0)
        {
            const std::string skyboxPath = AssetManager::GetPath(skyboxUUID);
            if (skyboxPath != "Path was not found in AssetLibrary")
            {
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(skyboxPath))
                    skyboxLabel = meta->name;
                else
                    skyboxLabel = skyboxPath;
            }
        }

        ImGui::Button(skyboxLabel.c_str(), ImVec2(180, 0));
        if (ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            m_scene->SetEnvironmentSkyboxUUID(UUID(0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                std::string path = AssetManager::GetPath(dropped.uuid);
                if (MetaFileAsset *meta = AssetManager::GetMetaFile(path))
                {
                    if (meta->type == MetaFileAsset::FileType::SKYBOX)
                        m_scene->SetEnvironmentSkyboxUUID(meta->uuid);
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem("skybox_env_ctx"))
        {
            if (ImGui::MenuItem("Clear"))
                m_scene->SetEnvironmentSkyboxUUID(UUID(0));
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    void Editor::CommitAssetRename()
    {
        using namespace std;
        namespace fs = std::filesystem;

        if (!m_isRenamingAsset || m_renamingPath.empty())
            return;

        string newName = m_renameBuffer;

        // nothing entered, cancel
        if (newName.empty())
        {
            m_isRenamingAsset = false;
            return;
        }

        fs::path oldPath = m_renamingPath;
        fs::path newPath = oldPath;
        newPath.replace_filename(newName);

        // handle no change
        if (newPath == oldPath)
        {
            m_isRenamingAsset = false;
            return;
        }

        AssetManager::MoveAsset(oldPath.string(), newPath.string());

        m_isRenamingAsset = false;
        m_renamingPath.clear();
    }

    void Editor::DrawDirectoryRecursive(const std::string &_dirPath)
    {
        namespace fs = std::filesystem;
        fs::path path = _dirPath;

        for (const auto &entry : fs::directory_iterator(path))
        {
            const std::string name = entry.path().filename().string();
            if (name == ".DS_Store" || entry.path().extension() == ".meta")
                continue;

            if (entry.is_directory())
            {
                ImGuiTreeNodeFlags nodeFlags =
                    ImGuiTreeNodeFlags_OpenOnArrow |
                    ImGuiTreeNodeFlags_SpanAvailWidth;
                bool open = ImGui::TreeNodeEx(entry.path().string().c_str(), nodeFlags, "%s", name.c_str());

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                    {
                        const AssetDragData *data = static_cast<const AssetDragData *>(payload->Data);

                        fs::path src = data->path;

                        AssetManager::MoveAsset(src.string(), entry.path().string() + "/" + src.filename().string());
                    }
                    ImGui::EndDragDropTarget();
                }

                // right click
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::MenuItem("Create 2D Scene"))
                    {
                        // copy scene template
                        std::string from = "assets/defaults/templates/scenes/2d_scene.scene";
                        std::string to = entry.path().string() + "/new_2d_scene.scene";
                        std::error_code ec;
                        fs::copy_file(from, to, ec);

                        // add it to asset manager meta
                        if (!ec)
                            MetaFileAsset *meta = AssetManager::GetMetaFile(to);
                    }

                    if (ImGui::MenuItem("Create Material"))
                    {
                        const fs::path folderPath = entry.path();
                        const fs::path templatePath = "assets/defaults/materials/default.material";

                        fs::path targetPath = folderPath / "new_material.material";
                        int index = 1;
                        while (fs::exists(targetPath))
                        {
                            targetPath = folderPath / ("new_material_" + std::to_string(index) + ".material");
                            ++index;
                        }

                        std::error_code ec;
                        fs::copy_file(templatePath, targetPath, ec);
                        if (ec)
                        {
                            std::ofstream out(targetPath.string());
                            out << "color: [1, 1, 1, 1]\n";
                            out << "backFaceCulling: true\n";
                            out.close();
                        }

                        MetaFileAsset *meta = AssetManager::GetMetaFile(targetPath.string());
                    }

                    if (ImGui::MenuItem("Create Skybox"))
                    {
                        const fs::path folderPath = entry.path();

                        fs::path targetPath = folderPath / "new_skybox.skybox";
                        int index = 1;
                        while (fs::exists(targetPath))
                        {
                            targetPath = folderPath / ("new_skybox_" + std::to_string(index) + ".skybox");
                            ++index;
                        }

                        YAML::Node skyboxRoot(YAML::NodeType::Map);
                        auto makeFaceRef = []() -> YAML::Node
                        {
                            YAML::Node node(YAML::NodeType::Map);
                            node["uuid"] = (uint64_t)0;
                            return node;
                        };
                        skyboxRoot["right"] = makeFaceRef();
                        skyboxRoot["left"] = makeFaceRef();
                        skyboxRoot["top"] = makeFaceRef();
                        skyboxRoot["bottom"] = makeFaceRef();
                        skyboxRoot["front"] = makeFaceRef();
                        skyboxRoot["back"] = makeFaceRef();

                        std::ofstream out(targetPath.string());
                        out << skyboxRoot;
                        out.close();

                        MetaFileAsset *meta = AssetManager::GetMetaFile(targetPath.string());
                    }

                    ImGui::EndPopup();
                }

                if (open)
                {
                    DrawDirectoryRecursive(entry.path().string());
                    ImGui::TreePop();
                }
            }
            else if (entry.is_regular_file())
            {
                const std::string fullPath = entry.path().string();
                const bool isRenamingThis = m_isRenamingAsset && (m_renamingPath == fullPath);

                if (isRenamingThis)
                {
                    // rename input
                    ImGui::PushID(fullPath.c_str());
                    ImGui::SetNextItemWidth(-1.0f);

                    ImGuiInputTextFlags flags =
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_AutoSelectAll |
                        ImGuiInputTextFlags_CharsNoBlank;

                    if (ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer), flags))
                    {
                        CommitAssetRename();
                    }

                    // click elsewhere or escape will cancel
                    if (!ImGui::IsItemActive() &&
                        (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
                    {
                        m_isRenamingAsset = false;
                    }

                    ImGui::PopID();
                }
                else
                {
                    const bool selected = (m_selectedAssetPath == fullPath);
                    ImGui::Selectable(name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns);

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (MetaFileAsset *meta = AssetManager::GetMetaFile(fullPath))
                        {
                            if (meta->type == MetaFileAsset::FileType::MATERIAL ||
                                meta->type == MetaFileAsset::FileType::SKYBOX)
                                m_selectedAssetPath = fullPath;
                        }
                    }

                    // right click
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Rename"))
                        {
                            m_isRenamingAsset = true;
                            m_renamingPath = fullPath;

                            std::strncpy(m_renameBuffer, name.c_str(), sizeof(m_renameBuffer));
                            m_renameBuffer[sizeof(m_renameBuffer) - 1] = '\0';
                        }

                        // TODO: Delete, Reveal in Finder, etc.

                        ImGui::EndPopup();
                    }

                    // drag source (for moving + using UUID elsewhere)
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        MetaFileAsset *meta = AssetManager::GetMetaFile(entry.path().string());
                        if (meta)
                        {
                            AssetDragData data{};
                            data.uuid = meta->uuid;

                            std::string full = entry.path().string();
                            std::snprintf(data.path, sizeof(data.path), "%s", full.c_str());

                            ImGui::SetDragDropPayload("ASSET_DRAG", &data, sizeof(data));
                            ImGui::Text("Asset: %s", meta->name.c_str());
                        }
                        ImGui::EndDragDropSource();
                    }

                    // double-click handling (open scene / shader)
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        MetaFileAsset *meta = AssetManager::GetMetaFile(entry.path().string());
                        if (!meta)
                            continue;

                        if (meta->type == MetaFileAsset::FileType::SCENE && m_mode == EditorMode::EDIT)
                        {
                            m_scene->Unload();
                            m_scene->Init(m_app, m_window, &m_scene->GetInputManager(), meta->path);
                            m_scene->Load(m_app->GetScriptRegistry());
                        }
                        else if ((meta->type == MetaFileAsset::FileType::FRAGMENT ||
                                  meta->type == MetaFileAsset::FileType::VERTEX) &&
                                 m_mode == EditorMode::EDIT)
                        {
                            OpenInVSCode(std::string(SDL_GetBasePath()) + meta->path);
                        }
                    }
                }
            }
        }
    }

    void Editor::DrawAssetsPanel()
    {
        namespace fs = std::filesystem;

        ImGui::Begin("Assets");

        DrawDirectoryRecursive("assets");

        ImGui::End();
    }

    void Editor::DrawSystemPanel()
    {
        ImGui::Begin("Systems");

        if (m_scene == nullptr)
        {
            ImGui::TextUnformatted("No active scene.");
            ImGui::End();
            return;
        }

        const std::vector<Scene::SystemTiming>& timings = m_scene->GetSystemTimings();
        if (timings.empty())
        {
            ImGui::TextUnformatted("No systems registered.");
            ImGui::End();
            return;
        }

        float totalUpdateMs = 0.0f;
        float totalRenderMs = 0.0f;

        if (ImGui::BeginTable("SystemTimingTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("System");
            ImGui::TableSetupColumn("Update (ms)");
            ImGui::TableSetupColumn("Render (ms)");
            ImGui::TableSetupColumn("Total (ms)");
            ImGui::TableHeadersRow();

            for (const Scene::SystemTiming& timing : timings)
            {
                const float totalMs = timing.updateMs + timing.renderMs;
                totalUpdateMs += timing.updateMs;
                totalRenderMs += timing.renderMs;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", timing.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f", timing.updateMs);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", timing.renderMs);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f", totalMs);
            }

            ImGui::EndTable();
        }

        ImGui::Separator();
        ImGui::Text("Update Total: %.3f ms", totalUpdateMs);
        ImGui::Text("Render Total: %.3f ms", totalRenderMs);
        ImGui::Text("Frame System Total: %.3f ms", totalUpdateMs + totalRenderMs);

        ImGui::End();
    }

    void Editor::DrawProjectSettings()
    {
        ImGui::Begin("ProjectSettings");

        if (ImGui::Button("Save Project", ImVec2(-1.0f, 0.0f)))
        {
            Canis::SaveProjectConfig();
        }

        bool editorEnabled = Canis::GetProjectConfig().editor;
        ImGui::Text("editor mode");
        ImGui::SameLine();
        if (ImGui::Checkbox("##editorEnabled", &editorEnabled))
        {
            Canis::GetProjectConfig().editor = editorEnabled;
            Canis::SaveProjectConfig();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(restart required)");

        int targetGameWidth = Canis::GetProjectConfig().targetGameWidth;
        int targetGameHeight = Canis::GetProjectConfig().targetGameHeight;
        ImGui::Text("target game width");
        ImGui::SameLine();
        if (ImGui::InputInt("##targetGameWidth", &targetGameWidth, 0))
        {
            Canis::GetProjectConfig().targetGameWidth = std::max(1, targetGameWidth);
            Canis::SaveProjectConfig();
        }

        ImGui::Text("target game height");
        ImGui::SameLine();
        if (ImGui::InputInt("##targetGameHeight", &targetGameHeight, 0))
        {
            Canis::GetProjectConfig().targetGameHeight = std::max(1, targetGameHeight);
            Canis::SaveProjectConfig();
        }

        // display sync mode
        static const char* syncLabels[] = { "VSync On", "Sync Off", "Adaptive Sync" };
        int syncIndex = 1;
        switch (Canis::GetProjectConfig().syncMode)
        {
            case PROJECT_SYNC_VSYNC: syncIndex = 0; break;
            case PROJECT_SYNC_ADAPTIVE: syncIndex = 2; break;
            case PROJECT_SYNC_OFF:
            default: syncIndex = 1; break;
        }

        ImGui::Text("display sync");
        ImGui::SameLine();
        if (ImGui::Combo("##displaySync", &syncIndex, syncLabels, IM_ARRAYSIZE(syncLabels)))
        {
            static const int indexToSyncMode[] = { PROJECT_SYNC_VSYNC, PROJECT_SYNC_OFF, PROJECT_SYNC_ADAPTIVE };
            Canis::GetProjectConfig().syncMode = indexToSyncMode[syncIndex];
            m_window->SetSync(static_cast<Window::Sync>(Canis::GetProjectConfig().syncMode));
            Canis::SaveProjectConfig();
        }

        // fps limit checkbox
        ImGui::Text("in-game fps limit");
        ImGui::SameLine();
        if (ImGui::Checkbox("##useFPSLimit", &Canis::GetProjectConfig().useFrameLimit) && m_mode == EditorMode::PLAY)
        {
            if (Canis::GetProjectConfig().useFrameLimit)
                Time::SetTargetFPS(Canis::GetProjectConfig().frameLimit + 0.0f);
            else
                Time::SetTargetFPS(100000.0f);
        }

        // fps limit input
        if (Canis::GetProjectConfig().useFrameLimit)
        {
            ImGui::Text("    fps limit");
            ImGui::SameLine();
            if (ImGui::InputInt("##frameLimit", &Canis::GetProjectConfig().frameLimit, 0) && m_mode == EditorMode::PLAY)
                Time::SetTargetFPS(Canis::GetProjectConfig().frameLimit + 0.0f);
        }

        // editor fps limit input
        ImGui::Text("editor fps");
        ImGui::SameLine();
        if (ImGui::InputInt("##editorframeLimit", &Canis::GetProjectConfig().frameLimitEditor, 0) && m_mode == EditorMode::EDIT)
            Time::SetTargetFPS(Canis::GetProjectConfig().frameLimitEditor + 0.0f);

        // application icon
        ImGui::Text("icon");
        ImGui::SameLine();
        ImGui::Button(
            AssetManager::GetMetaFile(AssetManager::GetPath(Canis::GetProjectConfig().iconUUID))->name.c_str(),
            ImVec2(150, 0));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
            {
                const AssetDragData dropped = *static_cast<const AssetDragData *>(payload->Data);
                std::string path = AssetManager::GetPath(dropped.uuid);
                TextureAsset *asset = AssetManager::GetTexture(path);

                if (asset) // validate that this is a texture
                {
                    Canis::GetProjectConfig().iconUUID = dropped.uuid;
                    Canis::SaveProjectConfig();
                    m_window->SetWindowIcon(path);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::End();
    }

    void Editor::DrawEditorPanel()
    {
        static float hotKeyCoolDown = 0.0f;
        const float HOTKEYRESET = 0.1f;

        ImGui::Begin("Canis Editor");

        if (m_mode == EditorMode::EDIT)
        {
            if (ImGui::Button("Save##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_S) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
            {
                hotKeyCoolDown = HOTKEYRESET;
                m_scene->Save(m_app->GetScriptRegistry());
            }
            ImGui::SameLine();
            if (ImGui::Button("Play##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_P) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
            {
                hotKeyCoolDown = HOTKEYRESET;
                m_window->SetSync(static_cast<Window::Sync>(Canis::GetProjectConfig().syncMode));
                if (Canis::GetProjectConfig().useFrameLimit)
                    Time::SetTargetFPS(Canis::GetProjectConfig().frameLimit + 0.0f);
                else
                    Time::SetTargetFPS(100000.0f);
                // save copy of scene
                g_lastPlaySceneNode = m_scene->EncodeScene(m_app->GetScriptRegistry());

                m_mode = EditorMode::PLAY;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_R) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
            {
                hotKeyCoolDown = HOTKEYRESET;

                m_assetPaths = FindFilesInFolder("assets", "");

                // save copy of scene
                g_lastPlaySceneNode = m_scene->EncodeScene(m_app->GetScriptRegistry());

                // unload data
                m_scene->Unload();

                GameCodeObjectWatchFile(m_gameSharedLib, m_app);

                m_scene->LoadSceneNode(m_app->GetScriptRegistry(), g_lastPlaySceneNode);
            }
        }
        else
        {
            if (m_mode == EditorMode::PLAY)
            {
                if (ImGui::Button("Pause##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_P) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
                {
                    hotKeyCoolDown = HOTKEYRESET;
                    m_mode = EditorMode::PAUSE;
                }
            }
            else if (m_mode == EditorMode::PAUSE)
            {
                if (ImGui::Button("Resume##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_P) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
                {
                    hotKeyCoolDown = HOTKEYRESET;
                    m_mode = EditorMode::PLAY;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Stop##ScenePanel") || (ImGui::IsKeyDown(ImGuiKey_Q) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && hotKeyCoolDown < 0.0f))
            {
                hotKeyCoolDown = HOTKEYRESET;
                StopPlayMode();
            }
        }

        hotKeyCoolDown -= Time::DeltaTime();

        ImGui::SameLine();
        ImGui::Text("FPS: %s", std::to_string(m_app->FPS()).c_str());

        ImGui::SameLine();
        if (m_guizmoMode == GuizmoMode::LOCAL)
        {
            if (ImGui::Button("Local##ScenePanel"))
            {
                m_guizmoMode = GuizmoMode::WORLD;
            }
        }
        else
        {
            if (ImGui::Button("World##ScenePanel"))
            {
                m_guizmoMode = GuizmoMode::LOCAL;
            }
        }

        ImGui::SameLine();
        int sceneCameraMode = static_cast<int>(m_sceneCameraMode);
        const char* sceneCameraModeLabels[] = { "3D", "2D" };
        ImGui::SetNextItemWidth(70.0f);
        if (ImGui::Combo("##SceneCameraMode", &sceneCameraMode, sceneCameraModeLabels, IM_ARRAYSIZE(sceneCameraModeLabels)))
            m_sceneCameraMode = static_cast<SceneCameraMode>(sceneCameraMode);

        size_t entityCount = 0;

        for (Entity *e : m_scene->GetEntities())
            if (e != nullptr)
                entityCount++;

        ImGui::SameLine();
        ImGui::Text("Entity Count: %zu", entityCount);
        ImGui::SameLine();
        ImGui::Text("Update Time: %.3f ms", m_app->UpdateTimeMs());
        ImGui::SameLine();
        ImGui::Text("Draw Time: %.3f ms", m_app->RenderTimeMs());

        ImGui::End();
    }

    void Editor::SelectSprite2D()
    {
        if (ImGuizmo::IsOver() || ImGuizmo::IsUsing())
            return;

        if (!m_gameViewHovered || m_gameViewportWidth <= 0 || m_gameViewportHeight <= 0 ||
            m_gameViewportDrawWidth <= 0.0f || m_gameViewportDrawHeight <= 0.0f)
            return;

        ImVec2 mousePos = ImGui::GetMousePos();
        float localX = mousePos.x - m_gameViewportPosX;
        float localY = mousePos.y - m_gameViewportPosY;
        float logicalWidth = static_cast<float>((m_gameTextureWidth > 0) ? m_gameTextureWidth : m_window->GetWindowWidth());
        float logicalHeight = static_cast<float>((m_gameTextureHeight > 0) ? m_gameTextureHeight : m_window->GetWindowHeight());
        float scaleX = logicalWidth / m_gameViewportDrawWidth;
        float scaleY = logicalHeight / m_gameViewportDrawHeight;
        localX *= scaleX;
        localY *= scaleY;
        Vector2 mouse(localX, logicalHeight - localY);

        mouse = mouse - (Vector2(logicalWidth, logicalHeight) / 2.0f);

        Vector2 camPos(0.0f);
        float camScale = 1.0f;
        const bool useEditorSceneCamera2D =
            m_mode != EditorMode::HIDDEN &&
            m_sceneCameraMode == SceneCameraMode::SCENE_CAMERA_2D;

        if (useEditorSceneCamera2D)
        {
            camPos = m_editorCamera2DPosition;
            camScale = m_editorCamera2DScale;
        }
        else
        {
            std::vector<Entity *> &entities = m_scene->GetEntities();
            for (Entity *entity : entities)
            {
                if (!entity)
                    continue;
                if (Camera2D *camera = (entity != nullptr && entity->HasComponent<Camera2D>() ? &entity->GetComponent<Camera2D>() : nullptr))
                {
                    camPos = camera->GetPosition();
                    camScale = camera->GetScale();
                    break;
                }
            }
        }

        if (camScale != 0.0f)
            mouse = (mouse / camScale) + camPos;

        m_selectionMouseWorld = mouse;

        if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            return;

        bool mouseLock = false;

        for (int i = 0; i < m_scene->GetEntities().size(); i++)
        {
            if (m_scene->GetEntities()[i] == nullptr)
                continue;

            Entity* candidate = m_scene->GetEntities()[i];
            RectTransform *transform = (candidate != nullptr && candidate->HasComponent<RectTransform>())
                ? &candidate->GetComponent<RectTransform>()
                : nullptr;

            if (transform == nullptr)
                continue;

            const RectTransformRenderBounds bounds = GetRenderBounds(*candidate, *transform);
            float globalRotation = transform->GetRotation();
            Vector2 selectionMouse = mouse;

            if (globalRotation != 0.0f)
            {
                RotatePointAroundPivot(
                    selectionMouse,
                    bounds.rotationPivot,
                    globalRotation);
            }

            if (selectionMouse.x > bounds.min.x &&
                selectionMouse.x < bounds.min.x + bounds.size.x &&
                selectionMouse.y > bounds.min.y &&
                selectionMouse.y < bounds.min.y + bounds.size.y &&
                !mouseLock)
            {
                m_index = i;
            }
        }
    }

    void Editor::DrawSelectionMouseDebug(Camera2D *_camera2D)
    {
        if (_camera2D == nullptr)
            return;

        Matrix4 projection = Matrix4(1.0f);
        if (m_scene != nullptr && m_scene->HasEditorCamera2DOverride())
        {
            projection = m_scene->GetEditorCamera2DMatrix();
        }
        else
        {
            _camera2D->UpdateMatrix();
            projection = _camera2D->GetCameraMatrix();
        }

        static Canis::Shader debugLineShader("assets/shaders/debug_line.vs", "assets/shaders/debug_line.fs");

        const float halfSize = 7.0f;
        Vector2 vertices[4] = {
            Vector2(m_selectionMouseWorld.x - halfSize, m_selectionMouseWorld.y),
            Vector2(m_selectionMouseWorld.x + halfSize, m_selectionMouseWorld.y),
            Vector2(m_selectionMouseWorld.x, m_selectionMouseWorld.y - halfSize),
            Vector2(m_selectionMouseWorld.x, m_selectionMouseWorld.y + halfSize)
        };

        for (Vector2 &v : vertices)
            v = Vector2(projection * Vector4(v.x, v.y, 0.0f, 1.0f));

        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        debugLineShader.Use();
        glBindVertexArray(VAO);
        glDrawArrays(GL_LINES, 0, 4);
        debugLineShader.UnUse();

        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void Editor::DrawBoundingBox(Camera2D *_camera2D)
    {
        if (_camera2D == nullptr)
            return;

        Matrix4 projection = Matrix4(1.0f);
        if (m_scene != nullptr && m_scene->HasEditorCamera2DOverride())
        {
            projection = m_scene->GetEditorCamera2DMatrix();
        }
        else
        {
            _camera2D->UpdateMatrix();
            projection = _camera2D->GetCameraMatrix();
        }

        static Canis::Shader debugLineShader("assets/shaders/debug_line.vs", "assets/shaders/debug_line.fs");
        Entity &debugRectTransformEntity = *m_scene->GetEntities()[m_index];
        RectTransform &rtc = debugRectTransformEntity.GetComponent<RectTransform>();
        const RectTransformRenderBounds bounds = GetRenderBounds(debugRectTransformEntity, rtc);
        Vector2 vertices[4];

        vertices[0] = {bounds.min.x, bounds.min.y};
        vertices[1] = {bounds.min.x + bounds.size.x, bounds.min.y};
        vertices[2] = {bounds.min.x + bounds.size.x, bounds.min.y + bounds.size.y};
        vertices[3] = {bounds.min.x, bounds.min.y + bounds.size.y};


        for (Vector2 &v : vertices)
            RotatePointAroundPivot(
                v,
                bounds.rotationPivot,
                -rtc.GetRotation()
            );

        for (Vector2 &v : vertices)
            v = Vector2(projection * Vector4(v.x, v.y, 0.0f, 1.0f));

        GLuint VAO, VBO;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void *)0);
        glEnableVertexAttribArray(0);

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        debugLineShader.Use();

        glBindVertexArray(VAO);
        glDrawArrays(GL_LINE_LOOP, 0, 4);
        debugLineShader.UnUse();

        // clean up
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }
}
