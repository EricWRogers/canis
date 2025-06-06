#include <Canis/Editor.hpp>
#include <Canis/Canis.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/SceneManager.hpp>
#include <Canis/Window.hpp>
#include <Canis/Time.hpp>
#include <Canis/Math.hpp>
#include <Canis/AudioManager.hpp>
#include <Canis/Camera2D.hpp>

#include <Canis/ECS/Components/Transform.hpp>
#include <Canis/ECS/Components/RectTransform.hpp>
#include <Canis/ECS/Components/Color.hpp>
#include <Canis/ECS/Components/Mesh.hpp>
#include <Canis/ECS/Components/SphereCollider.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>
#include <Canis/ECS/Components/UISliderKnobComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/DirectionalLight.hpp>
#include <Canis/ECS/Components/Camera2DComponent.hpp>

#include <Canis/External/OpenGl.hpp>

#include <SDL.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

namespace fs = std::filesystem;

std::vector<std::string> FindFilesInFolder(const std::string &_folder, const std::string &_extension)
{
    std::vector<std::string> files;

    for (const auto &entry : fs::recursive_directory_iterator(_folder))
    {
        if (entry.is_regular_file() && entry.path().extension() == _extension)
        {
            files.push_back(entry.path().string());
        }
    }

    return files;
}

namespace Canis
{
    bool FileExists(const char *filename)
    {
        FILE *file = fopen(filename, "r");
        if (file)
        {
            fclose(file);
            return true;
        }
        return false;
    }

    std::vector<const char *> ConvertVectorToCStringVector(const std::vector<std::string> &stringVector)
    {
        std::vector<const char *> cStringVector;
        for (const auto &str : stringVector)
        {
            cStringVector.push_back(str.c_str());
        }
        return cStringVector;
    }

    std::vector<const char *> ConvertSystemsVectorToCStringVector(const std::vector<std::string> &stringVector, std::vector<System *> &_system)
    {
        std::vector<const char *> cStringVector;
        for (const auto &str : stringVector)
        {
            bool shouldContinue = false;
            for (auto *system : _system)
                if (system->GetName() == str)
                    shouldContinue = true;

            if (shouldContinue)
                continue;

            cStringVector.push_back(str.c_str());
        }
        return cStringVector;
    }

    std::vector<const char *> ConvertComponentVectorToCStringVector(const std::vector<std::string> &stringVector, Entity &_entity)
    {
        std::vector<const char *> cStringVector;
        for (const auto &str : stringVector)
        {
            if (GetComponent().hasComponentFuncs[str](_entity))
                continue;

            cStringVector.push_back(str.c_str());
        }
        return cStringVector;
    }

    std::vector<const char *> HierarchyElementInfoToCString(std::vector<HierarchyElementInfo> &_hierarchyElementInfos)
    {
        std::vector<const char *> cStringVector;

        for (const auto &hei : _hierarchyElementInfos)
        {
            cStringVector.push_back(hei.name.c_str());
        }

        return cStringVector;
    }

    void CopyToClipboard(const std::string &_text)
    {
        ImGui::SetClipboardText(_text.c_str());
    }

    void ShowEntityID(uint64_t id)
    {
        // Convert the entity ID to a string
        std::string entityIDStr = std::to_string(id);

        // Create a unique ID for ImGui to differentiate this item
        std::string buttonLabel = "##EntityIDButton";
        buttonLabel += entityIDStr;

        // Display the entity ID as a button
        ImGui::Text("Entity ID:");
        ImGui::SameLine();
        if (ImGui::Button(entityIDStr.c_str()))
        {
            CopyToClipboard(entityIDStr);
        }

        // Add a tooltip to indicate the click functionality
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Click to copy to clipboard");
        }
    }

    SceneManager &Editor::GetSceneManager()
    {
        if (m_scene->sceneManager == nullptr)
            Canis::Error("sceneManager is null");

        return *((SceneManager *)m_scene->sceneManager);
    }

    HierarchyElementInfo GetHierarchyElementInfo(SceneManager &_sceneManager, Entity &_entity)
    {
        Canis::UUID eid = _entity.GetComponent<IDComponent>().ID;
        for (HierarchyElementInfo hei : _sceneManager.hierarchyElements)
        {
            if (eid == hei.entity.GetUUID().ID)
            {
                return hei;
            }
        }

        HierarchyElementInfo h = {};
        h.name = "[NONE]";
        return h;
    }

    void Editor::Init(Window *_window)
    {
#if CANIS_EDITOR
        if (GetProjectConfig().editor)
        {
            // Setup Dear ImGui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO &io = ImGui::GetIO();
            (void)io;
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
            io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

#ifdef __EMSCRIPTEN__

#else
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
#endif

            // io.ConfigViewportsNoAutoMerge = true;
            // io.ConfigViewportsNoTaskBarIcon = true;

            // Setup Dear ImGui style
            ImGui::StyleColorsDark();
            // ImGui::StyleColorsLight();

            // Setup Platform/Renderer backends
            ImGui_ImplSDL2_InitForOpenGL((SDL_Window *)_window->GetSDLWindow(), (SDL_GLContext)_window->GetGLContext());
            ImGui_ImplOpenGL3_Init(OPENGLVERSION);
        }
#endif
    }

    void Editor::Draw(Scene *_scene, Window *_window, Time *_time)
    {
#if CANIS_EDITOR
        if (GetProjectConfig().editor)
        {
            if (m_scene != _scene)
            {
                Log("new scene");
            }
            m_scene = _scene;

            // Start the Dear ImGui frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplSDL2_NewFrame();
            ImGui::NewFrame();

            DrawInspectorPanel();
            DrawSystemPanel();
            DrawHierarchyPanel();
            DrawScenePanel(_window, _time);

            if (m_debugDraw == DebugDraw::RECT)
            {
                SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
                SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
                SDL_GL_MakeCurrent(backup_current_window, backup_current_context);

                ImGuizmo::BeginFrame();

                ImGuiViewport *mainViewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(mainViewport->WorkPos);
                ImGui::SetNextWindowSize(mainViewport->WorkSize);
                ImGui::SetNextWindowViewport(mainViewport->ID);

                ImGui::Begin("##GuizmoWindow", nullptr,
                             ImGuiWindowFlags_NoTitleBar |
                                 ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoScrollbar |
                                 ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoBackground);

                // === Gizmo operation selector ===
                static ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
                
                if (ImGui::IsKeyPressed(ImGuiKey_W))
                    operation = ImGuizmo::TRANSLATE;
                
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                    operation = ImGuizmo::ROTATE;
                
                if (ImGui::IsKeyPressed(ImGuiKey_R))
                    operation = ImGuizmo::SCALE;

                Camera2D camera2D;
                camera2D.Init((int)_window->GetScreenWidth(), (int)_window->GetScreenHeight());

                bool camFound = false;
                auto cam = _scene->entityRegistry.view<const Camera2DComponent>();
                for (auto [entity, camera] : cam.each())
                {
                    camera2D.SetPosition(camera.position);
                    camera2D.SetScale(camera.scale);
                    camera2D.Update();
                    camFound = true;
                    break;
                }

                glm::mat4 projection = camFound
                                           ? camera2D.GetProjectionMatrix()
                                           : glm::ortho(0.0f, static_cast<float>(_window->GetScreenWidth()), 0.0f, static_cast<float>(_window->GetScreenHeight()));

                RectTransform &rtc = debugRectTransformEntity.GetComponent<RectTransform>();
                glm::vec2 pos = rtc.GetGlobalPosition(_window->GetScreenWidth(), _window->GetScreenHeight());
                pos += rtc.originOffset;

                // Align to bottom-left
                pos += rtc.rotationOriginOffset;

                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(pos, 0.0f));
                model = glm::rotate(model, -rtc.rotation, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, glm::vec3(rtc.size * rtc.scale, 1.0f)); // Scale affects size

                ImGuizmo::SetOrthographic(true);
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(mainViewport->WorkPos.x, mainViewport->WorkPos.y, mainViewport->WorkSize.x, mainViewport->WorkSize.y);
                ImGuizmo::Enable(true);

                glm::mat4 view = camera2D.GetViewMatrix();

                ImGuizmo::Manipulate(
                    glm::value_ptr(view),
                    glm::value_ptr(projection),
                    operation,
                    ImGuizmo::LOCAL,
                    glm::value_ptr(model));

                if (ImGuizmo::IsUsing())
                {
                    glm::vec3 translation, rotation, scale;
                    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(model), glm::value_ptr(translation), glm::value_ptr(rotation), glm::value_ptr(scale));

                    // update position
                    glm::vec2 newPos = glm::vec2(translation.x, translation.y);
                    glm::vec2 oldPos = rtc.GetGlobalPosition(_window->GetScreenWidth(), _window->GetScreenHeight()) + rtc.originOffset;
                    oldPos += rtc.rotationOriginOffset;
                    rtc.position += newPos - oldPos;

                    // update rotation
                    rtc.rotation = -glm::radians(rotation.z);

                    // update size (scale stays constant, we resize the actual size)
                    rtc.size = glm::vec2(scale.x, scale.y) / rtc.scale;
                }

                ImGui::End();
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

            // debug draw
            if (m_debugDraw == DebugDraw::RECT)
            {
                Camera2D camera2D;
                camera2D.Init((int)_window->GetScreenWidth(), (int)_window->GetScreenHeight());
                bool camFound = false;
                auto cam = _scene->entityRegistry.view<const Camera2DComponent>();
                for (auto [entity, camera] : cam.each())
                {
                    camera2D.SetPosition(camera.position);
                    camera2D.SetScale(camera.scale);
                    camera2D.Update();
                    camFound = true;
                    continue;
                }

                glm::mat4 projection = glm::mat4(1.0f);

                if (camFound)
                    projection = camera2D.GetCameraMatrix();
                else
                    projection = glm::ortho(0.0f, static_cast<float>(_window->GetScreenWidth()), 0.0f, static_cast<float>(_window->GetScreenHeight()));

                static Canis::Shader debugLineShader("assets/shaders/debug_line.vs", "assets/shaders/debug_line.fs");
                RectTransform &debugRectTransform = debugRectTransformEntity.GetComponent<RectTransform>();
                glm::vec2 pos = debugRectTransform.GetGlobalPosition(_window->GetScreenWidth(), _window->GetScreenHeight());
                pos += debugRectTransform.originOffset;
                glm::vec2 vertices[] = {
                    {pos.x, pos.y},
                    {pos.x + (debugRectTransform.size.x * debugRectTransform.scale), pos.y},
                    {pos.x + (debugRectTransform.size.x * debugRectTransform.scale), pos.y + (debugRectTransform.size.y * debugRectTransform.scale)},
                    {pos.x, pos.y + (debugRectTransform.size.y * debugRectTransform.scale)}};
                
                for (glm::vec2 &v : vertices)
                    RotatePointAroundPivot(
                        v,
                        vertices[0] + debugRectTransform.originOffset + debugRectTransform.rotationOriginOffset,
                        debugRectTransform.GetGlobalRotation()
                    );

                for (glm::vec2 &v : vertices)
                    v = glm::vec2(projection * glm::vec4(v.x, v.y, 0.0f, 1.0f));

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

            // Save
            if (m_mode == EditorMode::EDIT && GetSceneManager().inputManager->JustPressedKey(SDLK_F5))
            {
                GetSceneManager().Save();
            }
        }
#endif
    }

    void Editor::DrawInspectorPanel()
    {
        static float f = 0.0f;
        static int counter = 0;
        static int lastIndex = 0;
        static UUID id = 0;
        static Entity entity(m_scene);
        static bool refresh = true;

        m_debugDraw = DebugDraw::NONE;

        static std::vector<std::string> pngFilePaths = FindFilesInFolder("assets", ".png");
        static std::vector<std::string> materialFilePaths = FindFilesInFolder("assets", ".material");
        static std::vector<std::string> objFilePaths = FindFilesInFolder("assets", ".obj");
        static std::vector<std::string> ttfFilePaths = FindFilesInFolder("assets", ".ttf");

        int count = 0;

        if (m_index != lastIndex || m_forceRefresh)
        {
            lastIndex = m_index;
            refresh = true;
            m_forceRefresh = false;

            pngFilePaths = FindFilesInFolder("assets", ".png");
            ttfFilePaths = FindFilesInFolder("assets", ".ttf");
        }

        ImGui::Begin("Inspector"); // Create a window called "Hello, world!" and append into it.

        ImGui::SameLine();

        ShowEntityID((uint64_t)id);

        if (GetSceneManager().hierarchyElements.size() != 0)
        {

            if (m_index < 0)
                m_index = GetSceneManager().hierarchyElements.size() - 1;

            if (m_index >= GetSceneManager().hierarchyElements.size())
                m_index = 0;

            ImGui::InputText("##InspectorObjectName", &GetSceneManager().hierarchyElements[m_index].name);

            int i = 0;

            for (HierarchyElementInfo hei : GetSceneManager().hierarchyElements)
            {
                if (i == m_index && hei.entity.HasComponent<IDComponent>())
                {
                    id = hei.entity.GetUUID().ID;
                    entity = hei.entity;
                }

                i++;
            }

            if (entity.HasComponent<TagComponent>())
            {
                if (ImGui::CollapsingHeader("Canis::Tag"))
                {
                    auto &tagComponent = entity.GetComponent<TagComponent>();

                    char newTag[20];
                    std::strncpy(newTag, tagComponent.tag, sizeof(newTag));

                    if (ImGui::InputText("Tag", newTag, sizeof(newTag)))
                    {
                        if (strlen(newTag) > 0)
                        {
                            std::strncpy(tagComponent.tag, newTag, sizeof(tagComponent.tag));
                        }
                    }
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Tag"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Tag"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::TagComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<Transform>())
            {
                if (ImGui::CollapsingHeader("Canis::Transform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto &tc = entity.GetComponent<Transform>();

                    bool update = false;

                    ImGui::Checkbox("active", &tc.active);
                    if (ImGui::InputFloat3("position", glm::value_ptr(tc.position), "%.3f"))
                        update = true;
                    glm::vec3 r = GetLocalRotation(tc) * RAD2DEG;
                    if (ImGui::InputFloat3("rotation", glm::value_ptr(r), "%.3f"))
                    {
                        update = true;
                        SetTransformRotation(tc, r * DEG2RAD);
                    }
                    if (ImGui::InputFloat3("scale", glm::value_ptr(tc.scale), "%.3f"))
                        update = true;

                    if (update)
                        UpdateModelMatrix(tc);
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Transform"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Transform"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::Transform>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<DirectionalLight>())
            {
                if (ImGui::CollapsingHeader("Canis::DirectionalLight", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto &dlc = entity.GetComponent<DirectionalLight>();

                    ImGui::InputFloat3("direction", glm::value_ptr(dlc.direction), "%.3f");
                    ImGui::InputFloat3("ambient", glm::value_ptr(dlc.ambient), "%.3f");
                    ImGui::InputFloat3("diffuse", glm::value_ptr(dlc.diffuse), "%.3f");
                    ImGui::InputFloat3("specular", glm::value_ptr(dlc.specular), "%.3f");
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::DirectionalLight"))
                {
                    if (ImGui::MenuItem("Remove##Canis::DirectionalLight"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::DirectionalLight>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<SphereCollider>())
            {
                if (ImGui::CollapsingHeader("Canis::SphereCollider", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto &scc = entity.GetComponent<SphereCollider>();

                    ImGui::InputFloat3("center", glm::value_ptr(scc.center), "%.3f");
                    ImGui::InputFloat("radius", &scc.radius);
                    ImGui::InputScalar("layer", ImGuiDataType_U32, &scc.layer);
                    ImGui::InputScalar("mask", ImGuiDataType_U32, &scc.mask);
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::SphereCollider"))
                {
                    if (ImGui::MenuItem("Remove##Canis::SphereCollider"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::SphereCollider>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<RectTransform>())
            {
                auto &rtc = entity.GetComponent<RectTransform>();

                {
                    m_debugDraw = DebugDraw::RECT;

                    debugRectTransformEntity = entity;
                }

                if (ImGui::CollapsingHeader("Canis::RectTransform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Checkbox("active", &rtc.active);

                    {
                        static uint64_t tempIDparent = (rtc.parent) ? ((rtc.parent.HasComponent<IDComponent>()) ? (uint64_t)rtc.parent.GetComponent<IDComponent>().ID : 0lu) : 0lu;

                        if (refresh)
                        {
                            tempIDparent = (rtc.parent) ? ((rtc.parent.HasComponent<IDComponent>()) ? (uint64_t)rtc.parent.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                        }

                        int parentIndex = 0;

                        std::vector<HierarchyElementInfo> otherRectTransformEntities = {};

                        HierarchyElementInfo h = {};
                        h.name = "[NONE]";
                        otherRectTransformEntities.push_back(h);

                        int i = 1;
                        for (HierarchyElementInfo hei : GetSceneManager().hierarchyElements)
                        {
                            if (id != hei.entity.GetUUID().ID)
                            {
                                if (hei.entity.HasComponent<RectTransform>())
                                {
                                    otherRectTransformEntities.push_back(hei);

                                    if (tempIDparent != 0lu)
                                    {
                                        if (rtc.parent)
                                        {
                                            if (rtc.parent.GetUUID() == hei.entity.GetUUID())
                                            {
                                                parentIndex = i;
                                            }
                                        }
                                    }

                                    i++;
                                }
                            }
                        }

                        std::vector<const char *> otherRectTransformNames = HierarchyElementInfoToCString(otherRectTransformEntities);

                        if (ImGui::Combo("parent", &parentIndex, otherRectTransformNames.data(), static_cast<int>(otherRectTransformNames.size())))
                        {
                            if (otherRectTransformEntities.size() > 0)
                            {
                                tempIDparent = otherRectTransformEntities[parentIndex].entity.GetUUID();

                                if (rtc.parent != otherRectTransformEntities[parentIndex].entity)
                                {

                                    Entity oldParent = rtc.parent;
                                    rtc.parent = otherRectTransformEntities[parentIndex].entity;

                                    if (oldParent)
                                    {
                                        // remove from parent children vector
                                        for (int c = 0; c < oldParent.GetComponent<RectTransform>().children.size(); c++)
                                        {
                                            if (entity == oldParent.GetComponent<RectTransform>().children[c])
                                            {
                                                oldParent.GetComponent<RectTransform>().children.erase(
                                                    oldParent.GetComponent<RectTransform>().children.begin() + c);
                                                break;
                                            }
                                        }
                                    }

                                    // add to new parent vector
                                    rtc.parent.GetComponent<RectTransform>().children.push_back(entity);
                                }
                            }
                        }
                    }

                    if (rtc.children.size() > 0)
                    {
                        if (ImGui::CollapsingHeader("children"))
                        {
                            std::vector<HierarchyElementInfo> childrenEntity = {};

                            for (int c = 0; c < rtc.children.size(); c++)
                            {
                                rtc.children[c].scene = entity.scene;
                                childrenEntity.push_back(GetHierarchyElementInfo(GetSceneManager(), rtc.children[c]));
                            }

                            std::vector<const char *> childrenEntityNames = HierarchyElementInfoToCString(childrenEntity);

                            for (int c = 0; c < childrenEntityNames.size(); c++)
                            {
                                ImGui::Text("%s", childrenEntityNames[c]);
                            }
                        }
                    }

                    ImGui::Combo("anchor", &rtc.anchor, RectAnchorLabels, IM_ARRAYSIZE(RectAnchorLabels));

                    ImGui::InputFloat2("position", glm::value_ptr(rtc.position), "%.3f");
                    ImGui::Checkbox("inheritWidth", &rtc.inheritWidth);
                    ImGui::Checkbox("inheritHeight", &rtc.inheritHeight);

                    if (!rtc.inheritWidth || !rtc.inheritHeight)
                        ImGui::InputFloat2("size", glm::value_ptr(rtc.size), "%.3f");

                    ImGui::InputFloat2("originOffset", glm::value_ptr(rtc.originOffset), "%.3f");
                    ImGui::InputFloat("rotation", &rtc.rotation);
                    ImGui::InputFloat("scale", &rtc.scale);
                    ImGui::InputFloat("depth", &rtc.depth);

                    ImGui::Combo("scaleWithScreen", &rtc.scaleWithScreen, ScaleWithScreenLabels, IM_ARRAYSIZE(ScaleWithScreenLabels));

                    ImGui::InputFloat2("rotationOriginOffset", glm::value_ptr(rtc.rotationOriginOffset), "%.3f");
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Rect"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Rect"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::RectTransform>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<Color>())
            {
                if (ImGui::CollapsingHeader("Canis::Color"))
                {
                    auto &cc = entity.GetComponent<Color>();
                    ImGui::InputFloat4("color", glm::value_ptr(cc.color), "%.3f");
                    ImGui::InputFloat3("emission", glm::value_ptr(cc.emission), "%.3f");
                    ImGui::InputFloat("emissionUsingAlbedoIntesity", &cc.emissionUsingAlbedoIntesity);
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Color"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Color"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::Color>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<Mesh>())
            {
                static std::string path = "";
                static int selectedPath = 0;
                static std::string materialPath = "";
                static int materialSelectedPath = 0;

                auto &mc = entity.GetComponent<Mesh>();

                if (refresh)
                {
                    if (mc.modelHandle.id == -1)
                    {
                        if (objFilePaths.size() == 0)
                        {
                            FatalError("No .obj not found in the asset folder!");
                        }

                        mc.modelHandle.id = AssetManager::LoadModel(objFilePaths[0]);
                    }

                    path = AssetManager::GetPath(mc.modelHandle.id);

                    int index = 0;
                    for (std::string &s : pngFilePaths)
                    {
                        if (path == s)
                        {
                            selectedPath = index;
                        }

                        index++;
                    }

                    // material
                    if (mc.material == -1)
                    {
                        if (materialFilePaths.size() == 0)
                        {
                            FatalError("No .material not found in the asset folder!");
                        }

                        mc.material = AssetManager::LoadMaterial(materialFilePaths[0]);
                    }

                    materialPath = AssetManager::GetPath(mc.material);

                    index = 0;
                    for (std::string &s : materialFilePaths)
                    {
                        if (path == s)
                        {
                            materialSelectedPath = index;
                        }

                        index++;
                    }
                }

                if (ImGui::CollapsingHeader("Canis::Model"))
                {
                    std::vector<const char *> cStringItems = ConvertVectorToCStringVector(objFilePaths);

                    if (ImGui::Combo("##Canis::Model", &selectedPath, cStringItems.data(), static_cast<int>(cStringItems.size())))
                    {
                        int newID = AssetManager::LoadModel(objFilePaths[selectedPath]);

                        if (newID != -1)
                        {
                            mc.modelHandle.id = newID;
                        }
                    }

                    ImGui::SameLine();
                    ImGui::Text("model");

                    std::vector<const char *> cmStringItems = ConvertVectorToCStringVector(materialFilePaths);

                    if (ImGui::Combo("##Canis::Material", &materialSelectedPath, cmStringItems.data(), static_cast<int>(cmStringItems.size())))
                    {
                        int newID = AssetManager::LoadMaterial(materialFilePaths[materialSelectedPath]);

                        if (newID != -1)
                        {
                            mc.material = newID;
                        }
                    }

                    ImGui::SameLine();
                    ImGui::Text("material");

                    ImGui::Checkbox("cast shadow", &mc.castShadow);
                    ImGui::Checkbox("cast depth", &mc.castDepth);
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Mesh"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Mesh"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::UIImageComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<UIImageComponent>())
            {
                static std::string path = "";
                static int selectedPath = 0;

                auto &ic = entity.GetComponent<UIImageComponent>();

                if (refresh)
                {
                    if (ic.textureHandle.id == -1)
                    {
                        if (pngFilePaths.size() == 0)
                        {
                            FatalError("No .png not found in the asset folder!");
                        }

                        ic.textureHandle = AssetManager::GetTextureHandle(pngFilePaths[0]);
                    }

                    path = AssetManager::Get<TextureAsset>(ic.textureHandle.id)->GetPath();

                    int index = 0;
                    for (std::string &s : pngFilePaths)
                    {
                        if (path == s)
                        {
                            selectedPath = index;
                        }

                        index++;
                    }
                }

                if (ImGui::CollapsingHeader("Canis::Image"))
                {
                    std::vector<const char *> cStringItems = ConvertVectorToCStringVector(pngFilePaths);

                    if (ImGui::Combo("##Canis::Image", &selectedPath, cStringItems.data(), static_cast<int>(cStringItems.size())))
                    {
                        int newID = AssetManager::LoadTexture(pngFilePaths[selectedPath]);

                        if (newID != -1)
                        {
                            ic.textureHandle = AssetManager::GetTextureHandle(newID);
                        }
                    }

                    ImGui::SameLine();
                    ImGui::Text("image");

                    ImGui::InputFloat4("uv", glm::value_ptr(ic.uv));
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Image"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Image"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::UIImageComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<TextComponent>())
            {
                static std::string path = "";
                static int size = 0;
                static int lastFrameSize = 0;
                static int selectedPath = 0;

                auto &tc = entity.GetComponent<TextComponent>();

                if (refresh)
                {
                    if (tc.assetId == -1) // if the asset id has not been set
                    {
                        if (ttfFilePaths.size() == 0)
                        {
                            FatalError("No .ttf not found in the asset folder!");
                        }

                        tc.assetId = AssetManager::LoadText(ttfFilePaths[0], 24);
                    }

                    path = AssetManager::Get<TextAsset>(tc.assetId)->GetPath();

                    size = AssetManager::Get<TextAsset>(tc.assetId)->GetFontSize();
                    lastFrameSize = size;

                    int index = 0;
                    for (std::string &s : ttfFilePaths)
                    {
                        if (path == s)
                        {
                            selectedPath = index;
                        }

                        index++;
                    }
                }

                if (ImGui::CollapsingHeader("Canis::Text"))
                {
                    if (ImGui::CollapsingHeader("Text Asset"))
                    {
                        int tempSize = size;
                        bool newPath = false;

                        std::vector<const char *> cStringItems = ConvertVectorToCStringVector(ttfFilePaths);

                        if (ImGui::Combo("##Canis::Text", &selectedPath, cStringItems.data(), static_cast<int>(cStringItems.size())))
                        {
                            newPath = true;
                            path = ttfFilePaths[selectedPath];
                        }

                        ImGui::SameLine();
                        ImGui::Text("text");

                        ImGui::InputInt("size", &tempSize);

                        size = tempSize;

                        if (newPath || size != lastFrameSize)
                        {
                            if (size < 0)
                                size = 12;

                            int newID = AssetManager::LoadText(path, size);

                            if (newID != -1)
                            {
                                tc.assetId = newID;

                                path = AssetManager::Get<TextAsset>(tc.assetId)->GetPath();

                                size = AssetManager::Get<TextAsset>(tc.assetId)->GetFontSize();
                                lastFrameSize = size;
                            }
                        }
                    }

                    ImGui::InputText("text", &tc.text);                    

                    int *a = ((int *)&tc.alignment); // this might be bad because imgui uses -1 as error
                    ImGui::Combo("alignment", a, Text::AlignmentLabels, IM_ARRAYSIZE(Text::AlignmentLabels));

                    int *hb = ((int *)&tc.horizontalBoundary); // this might be bad because imgui uses -1 as error
                    ImGui::Combo("horizontalBoundary", hb, Text::HorizontalBoundaryLabels, IM_ARRAYSIZE(Text::HorizontalBoundaryLabels));
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Text"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Text"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::TextComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<ButtonComponent>())
            {
                auto &bc = entity.GetComponent<ButtonComponent>();

                static uint64_t tempIDUp = (bc.up) ? ((bc.up.HasComponent<IDComponent>()) ? (uint64_t)bc.up.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                static uint64_t tempIDDown = (bc.down) ? ((bc.down.HasComponent<IDComponent>()) ? (uint64_t)bc.down.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                static uint64_t tempIDLeft = (bc.left) ? ((bc.left.HasComponent<IDComponent>()) ? (uint64_t)bc.left.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                static uint64_t tempIDRight = (bc.right) ? ((bc.right.HasComponent<IDComponent>()) ? (uint64_t)bc.right.GetComponent<IDComponent>().ID : 0lu) : 0lu;

                static int upIndex = 0;
                static int downIndex = 0;
                static int leftIndex = 0;
                static int rightIndex = 0;

                if (refresh)
                {
                    tempIDUp = (bc.up) ? ((bc.up.HasComponent<IDComponent>()) ? (uint64_t)bc.up.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                    tempIDDown = (bc.down) ? ((bc.down.HasComponent<IDComponent>()) ? (uint64_t)bc.down.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                    tempIDLeft = (bc.left) ? ((bc.left.HasComponent<IDComponent>()) ? (uint64_t)bc.left.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                    tempIDRight = (bc.right) ? ((bc.right.HasComponent<IDComponent>()) ? (uint64_t)bc.right.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                }

                if (ImGui::CollapsingHeader("Canis::Button"))
                {
                    ImGui::InputText("eventName", &bc.eventName);
                    ImGui::InputFloat4("baseColor", glm::value_ptr(bc.baseColor), "%.3f");
                    ImGui::InputFloat4("hoverColor", glm::value_ptr(bc.hoverColor), "%.3f");

                    static const char *ActionLabels[] = {
                        "Clicked", "Released"};

                    int actionIndex = (int)bc.action;
                    ImGui::Combo("action", &actionIndex, ActionLabels, IM_ARRAYSIZE(ActionLabels));
                    bc.action = actionIndex;

                    ImGui::Checkbox("mouseOver", &bc.mouseOver);
                    ImGui::InputFloat("scale", &bc.scale);
                    ImGui::InputFloat("hoverScale", &bc.hoverScale);

                    upIndex = 0;
                    downIndex = 0;
                    leftIndex = 0;
                    rightIndex = 0;

                    std::vector<HierarchyElementInfo> otherButtonEntities = {};

                    HierarchyElementInfo h = {};
                    h.name = "[NONE]";
                    otherButtonEntities.push_back(h);

                    int i = 1;
                    for (HierarchyElementInfo hei : GetSceneManager().hierarchyElements)
                    {
                        if (id != hei.entity.GetUUID().ID)
                        {
                            if (hei.entity.HasComponent<ButtonComponent>())
                            {
                                otherButtonEntities.push_back(hei);

                                if (tempIDUp != 0lu)
                                {
                                    if (bc.up)
                                    {
                                        if (bc.up.GetUUID() == hei.entity.GetUUID())
                                        {
                                            upIndex = i;
                                        }
                                    }
                                }

                                if (tempIDDown != 0lu)
                                {
                                    if (bc.down)
                                    {
                                        if (bc.down.GetUUID() == hei.entity.GetUUID())
                                        {
                                            downIndex = i;
                                        }
                                    }
                                }

                                if (tempIDLeft != 0lu)
                                {
                                    if (bc.left)
                                    {
                                        if (bc.left.GetUUID() == hei.entity.GetUUID())
                                        {
                                            leftIndex = i;
                                        }
                                    }
                                }

                                if (tempIDRight != 0lu)
                                {
                                    if (bc.right)
                                    {
                                        if (bc.right.GetUUID() == hei.entity.GetUUID())
                                        {
                                            rightIndex = i;
                                        }
                                    }
                                }

                                i++;
                            }
                        }
                    }

                    std::vector<const char *> otherButtonNames = HierarchyElementInfoToCString(otherButtonEntities);

                    if (ImGui::Combo("nav up", &upIndex, otherButtonNames.data(), static_cast<int>(otherButtonNames.size())))
                    {
                        if (otherButtonEntities.size() > 0)
                        {
                            tempIDUp = otherButtonEntities[upIndex].entity.GetUUID();
                            bc.up = otherButtonEntities[upIndex].entity;
                        }
                    }

                    if (ImGui::Combo("nav down", &downIndex, otherButtonNames.data(), static_cast<int>(otherButtonNames.size())))
                    {
                        if (otherButtonEntities.size() > 0)
                        {
                            tempIDDown = otherButtonEntities[downIndex].entity.GetUUID();
                            bc.down = otherButtonEntities[downIndex].entity;
                        }
                    }

                    if (ImGui::Combo("nav left", &leftIndex, otherButtonNames.data(), static_cast<int>(otherButtonNames.size())))
                    {
                        if (otherButtonEntities.size() > 0)
                        {
                            tempIDLeft = otherButtonEntities[leftIndex].entity.GetUUID();
                            bc.left = otherButtonEntities[leftIndex].entity;
                        }
                    }

                    if (ImGui::Combo("nav right", &rightIndex, otherButtonNames.data(), static_cast<int>(otherButtonNames.size())))
                    {
                        if (otherButtonEntities.size() > 0)
                        {
                            tempIDRight = otherButtonEntities[rightIndex].entity.GetUUID();
                            bc.right = otherButtonEntities[rightIndex].entity;
                        }
                    }

                    ImGui::Checkbox("defaultSelected", &bc.defaultSelected);
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Button"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Button"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::ButtonComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<Sprite2DComponent>())
            {
                static std::string path = "";
                static int selectedPath = 0;

                auto &ic = entity.GetComponent<Sprite2DComponent>();

                if (refresh)
                {
                    path = AssetManager::Get<TextureAsset>(ic.textureHandle.id)->GetPath();

                    int index = 0;
                    for (std::string &s : pngFilePaths)
                    {
                        if (path == s)
                        {
                            selectedPath = index;
                        }

                        index++;
                    }
                }

                if (ImGui::CollapsingHeader("Canis::Sprite"))
                {
                    std::vector<const char *> cStringItems = ConvertVectorToCStringVector(pngFilePaths);

                    if (ImGui::Combo("##Canis::Sprite", &selectedPath, cStringItems.data(), static_cast<int>(cStringItems.size())))
                    {
                        int newID = AssetManager::LoadTexture(pngFilePaths[selectedPath]);

                        if (newID != -1)
                        {
                            ic.textureHandle = AssetManager::GetTextureHandle(newID);

                            path = AssetManager::Get<TextureAsset>(ic.textureHandle.id)->GetPath();
                        }
                    }

                    ImGui::SameLine();
                    ImGui::Text("image");

                    ImGui::InputFloat4("uv", glm::value_ptr(ic.uv));
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Image"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Image"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::UIImageComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<UISliderComponent>())
            {
                if (ImGui::CollapsingHeader("Canis::UISlider"))
                {
                    auto &sc = entity.GetComponent<UISliderComponent>();
                    ImGui::InputFloat("maxWidth", &sc.maxWidth);
                    ImGui::InputFloat("minUVX", &sc.minUVX);
                    ImGui::InputFloat("maxUVX", &sc.maxUVX);
                    ImGui::SliderFloat("value", &sc.value, 0.0f, 1.0f);
                    ImGui::SliderFloat("targetValue", &sc.targetValue, 0.0f, 1.0f);
                    ImGui::InputFloat("timeToMoveFullBar", &sc.timeToMoveFullBar);
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::UISlider"))
                {
                    if (ImGui::MenuItem("Remove##Canis::UISlider"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::UISliderComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<UISliderKnobComponent>())
            {
                auto &kc = entity.GetComponent<UISliderKnobComponent>();

                static uint64_t tempID = (kc.slider) ? ((kc.slider.HasComponent<IDComponent>()) ? (uint64_t)kc.slider.GetComponent<IDComponent>().ID : 0lu) : 0lu;

                if (refresh)
                {
                    tempID = (kc.slider) ? ((kc.slider.HasComponent<IDComponent>()) ? (uint64_t)kc.slider.GetComponent<IDComponent>().ID : 0lu) : 0lu;
                }

                if (ImGui::CollapsingHeader("Canis::Knob"))
                {
                    ImGui::InputText("eventName", &kc.eventName);

                    if (ImGui::InputScalar("slider", ImGuiDataType_U64, &tempID))
                    {
                        UUID u = tempID;

                        for (auto hei : GetSceneManager().hierarchyElements)
                        {
                            if (hei.entity.GetUUID() == tempID)
                            {
                                u = tempID;
                                kc.slider = hei.entity;
                                break;
                            }
                            else
                            {
                                u = 0ul;
                            }
                        }

                        tempID = u.ID;
                    }

                    ImGui::Checkbox("grabbed", &kc.grabbed);
                    ImGui::SliderFloat("value", &kc.value, 0.0f, 1.0f);
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Knob"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Knob"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::UISliderKnobComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            static int componentToAdd = 0;

            if (refresh)
            {
                componentToAdd = 0;
            }

            {
                std::vector<const char *> cStringItems = ConvertComponentVectorToCStringVector(GetComponent().names, entity);

                if (cStringItems.size() > 0)
                {
                    ImGui::Combo("##Components", &componentToAdd, cStringItems.data(), static_cast<int>(cStringItems.size()));

                    ImGui::SameLine();

                    if (ImGui::Button("+##AddComponent"))
                    {
                        GetComponent().addComponentFuncs[cStringItems[componentToAdd]](entity);
                        componentToAdd = 0;
                        m_forceRefresh = true;
                    }
                }
            }

            static int scriptComponentSelect = 0;
            static std::vector<std::string> scriptComponentDropdown = {};

            if (refresh)
            {
                scriptComponentSelect = 0;
                scriptComponentDropdown.clear();
                scriptComponentDropdown.push_back("NONE");

                for (std::string name : GetScriptableComponentRegistry().names)
                {
                    scriptComponentDropdown.push_back(name);
                }

                if (entity.HasComponent<ScriptComponent>())
                {
                    for (int i = 1; i < scriptComponentDropdown.size(); i++)
                    {
                        if (GetScriptableComponentRegistry().hasScriptableComponent[scriptComponentDropdown[i]](entity))
                        {
                            scriptComponentSelect = i;
                        }
                    }
                }
            }

            if (ImGui::CollapsingHeader("ScriptComponent"))
            {
                int wasScriptComponentSelect = scriptComponentSelect;

                std::vector<const char *> cStringItems = ConvertVectorToCStringVector(scriptComponentDropdown);

                ImGui::Combo("##ScriptableComponents", &scriptComponentSelect, cStringItems.data(), static_cast<int>(cStringItems.size()));

                if (scriptComponentSelect != wasScriptComponentSelect)
                {
                    if (scriptComponentSelect == 0)
                    {
                        if (entity.HasComponent<ScriptComponent>())
                        {
                            if (GetScriptableComponentRegistry().hasScriptableComponent[scriptComponentDropdown[wasScriptComponentSelect]](entity))
                            {
                                GetScriptableComponentRegistry().removeScriptableComponent[scriptComponentDropdown[wasScriptComponentSelect]](entity);
                            }
                        }
                    }
                    else
                    {
                        if (entity.HasComponent<ScriptComponent>())
                        {
                            if (GetScriptableComponentRegistry().hasScriptableComponent[scriptComponentDropdown[wasScriptComponentSelect]](entity))
                            {
                                GetScriptableComponentRegistry().removeScriptableComponent[scriptComponentDropdown[wasScriptComponentSelect]](entity);
                            }
                        }

                        GetScriptableComponentRegistry().addScriptableComponent[scriptComponentDropdown[scriptComponentSelect]](entity);
                    }
                }
            }
        }

        refresh = false; // keep here
        ImGui::End();
    }

    void Editor::DrawSystemPanel()
    {
        ImGui::Begin("Systems");
        if (ImGui::CollapsingHeader("Update"))
        {
            for (int i = 0; i < m_scene->m_updateSystems.size(); i++)
            {
                ImGui::Text("%s", m_scene->m_updateSystems[i]->GetName().c_str());

                if (i != 0)
                {
                    ImGui::SameLine();
                    std::string upButtonLabel = "^##" + std::to_string(i);
                    if (ImGui::Button(upButtonLabel.c_str()))
                    {
                        System *temp = m_scene->m_updateSystems[i - 1];
                        m_scene->m_updateSystems[i - 1] = m_scene->m_updateSystems[i];
                        m_scene->m_updateSystems[i] = temp;
                    }
                }

                if (i != m_scene->m_updateSystems.size() - 1)
                {
                    ImGui::SameLine();
                    std::string downButtonLabel = "v##" + std::to_string(i);
                    if (ImGui::Button(downButtonLabel.c_str()))
                    {
                        System *temp = m_scene->m_updateSystems[i];
                        m_scene->m_updateSystems[i] = m_scene->m_updateSystems[i + 1];
                        m_scene->m_updateSystems[i + 1] = temp;
                    }
                }

                ImGui::SameLine();
                std::string removeButtonLabel = "x##" + std::to_string(i);
                if (ImGui::Button(removeButtonLabel.c_str()))
                {
                    for (int s = 0; s < m_scene->systems.size(); s++)
                    {
                        if (m_scene->systems[s]->GetName() == m_scene->m_updateSystems[i]->GetName())
                        {
                            m_scene->systems.erase(m_scene->systems.begin() + s);
                            break;
                        }
                    }

                    delete (m_scene->m_updateSystems[i]);
                    m_scene->m_updateSystems.erase(m_scene->m_updateSystems.begin() + i);

                    i--;
                }
            }

            static int updateToAdd = 0;

            std::vector<const char *> cStringItems = ConvertSystemsVectorToCStringVector(GetSystemRegistry().updateSystems, m_scene->m_updateSystems);

            if (cStringItems.size() > 0)
            {
                ImGui::Combo("##UpdateSystem", &updateToAdd, cStringItems.data(), static_cast<int>(cStringItems.size()));
                ImGui::SameLine();
                if (ImGui::Button("+##UpdateSystem"))
                {
                    for (int d = 0; d < GetSceneManager().decodeSystem.size(); d++)
                    {
                        if (GetSceneManager().decodeSystem[d](cStringItems[updateToAdd], m_scene))
                            continue;
                    }

                    for (int i = 0; i < m_scene->systems.size(); i++)
                    {
                        if (!m_scene->systems[i]->IsCreated())
                        {
                            m_scene->systems[i]->Create();
                            m_scene->systems[i]->m_isCreated = true;
                        }
                    }

                    updateToAdd = 0;
                }
            }
        }

        if (ImGui::CollapsingHeader("Render"))
        {
            for (int i = 0; i < m_scene->m_renderSystems.size(); i++)
            {
                ImGui::Text("%s", m_scene->m_renderSystems[i]->GetName().c_str());

                if (i != 0)
                {
                    ImGui::SameLine();
                    std::string upButtonLabel = "^##" + std::to_string(i);
                    upButtonLabel += std::string("Render");
                    if (ImGui::Button(upButtonLabel.c_str()))
                    {
                        System *temp = m_scene->m_renderSystems[i - 1];
                        m_scene->m_renderSystems[i - 1] = m_scene->m_renderSystems[i];
                        m_scene->m_renderSystems[i] = temp;
                    }
                }

                if (i != m_scene->m_renderSystems.size() - 1)
                {
                    ImGui::SameLine();
                    std::string downButtonLabel = "v##" + std::to_string(i);
                    downButtonLabel += std::string("Render");
                    if (ImGui::Button(downButtonLabel.c_str()))
                    {
                        System *temp = m_scene->m_renderSystems[i];
                        m_scene->m_renderSystems[i] = m_scene->m_renderSystems[i + 1];
                        m_scene->m_renderSystems[i + 1] = temp;
                    }
                }

                ImGui::SameLine();
                std::string removeButtonLabel = "x##" + std::to_string(i);
                removeButtonLabel += std::string("Render");
                if (ImGui::Button(removeButtonLabel.c_str()))
                {
                    for (int s = 0; s < m_scene->systems.size(); s++)
                    {
                        if (m_scene->systems[s]->GetName() == m_scene->m_renderSystems[i]->GetName())
                        {
                            m_scene->systems.erase(m_scene->systems.begin() + s);
                            break;
                        }
                    }

                    delete (m_scene->m_renderSystems[i]);
                    m_scene->m_renderSystems.erase(m_scene->m_renderSystems.begin() + i);

                    i--;
                }
            }

            static int renderToAdd = 0;

            std::vector<const char *> cStringItems = ConvertSystemsVectorToCStringVector(GetSystemRegistry().renderSystems, m_scene->m_renderSystems);

            if (cStringItems.size() > 0)
            {
                ImGui::Combo("##RenderSystem", &renderToAdd, cStringItems.data(), static_cast<int>(cStringItems.size()));
                ImGui::SameLine();
                if (ImGui::Button("+##RenderSystem"))
                {
                    for (int d = 0; d < GetSceneManager().decodeRenderSystem.size(); d++)
                    {
                        if (GetSceneManager().decodeRenderSystem[d](cStringItems[renderToAdd], m_scene))
                            continue;
                    }

                    for (int i = 0; i < m_scene->systems.size(); i++)
                    {
                        if (!m_scene->systems[i]->IsCreated())
                        {
                            m_scene->systems[i]->Create();
                            m_scene->systems[i]->m_isCreated = true;
                        }
                    }

                    renderToAdd = 0;
                }
            }
        }
        ImGui::End();
    }

    void Editor::DrawHierarchyPanel()
    {
        ImGui::Begin("Hierarchy");

        for (int i = 0; i < GetSceneManager().hierarchyElements.size(); i++)
        {
            Entity entity = GetSceneManager().hierarchyElements[i].entity;

            if (entity.HasComponent<Canis::RectTransform>())
                if (entity.GetComponent<Canis::RectTransform>().parent)
                    continue;

            bool skip = DrawHierarchyElement(i);

            if (skip)
                break;
        }

        if (ImGui::Button("New Entity"))
        {
            Entity e = m_scene->CreateEntity();
            e.AddComponent<IDComponent>();

            HierarchyElementInfo hei;
            hei.entity.entityHandle = e.entityHandle;
            hei.entity.scene = m_scene;

            GetSceneManager().hierarchyElements.push_back(hei);
            m_forceRefresh = true;
        }

        ImGui::End();
    }

    // returns true if it should skip
    bool Editor::DrawHierarchyElement(int _index)
    {
        Canis::HierarchyElementInfo elementInfo = GetSceneManager().hierarchyElements[_index];
        Entity entity = elementInfo.entity;
        std::string uuidStr = std::to_string(entity.GetUUID());
        std::string label = "##he_node" + uuidStr;

        bool hasChildren = entity.HasComponent<Canis::RectTransform>() &&
                           !entity.GetComponent<Canis::RectTransform>().children.empty();

        // ─────────────────────────────────────────────────────
        // DROP ZONE BEFORE THIS NODE
        // ─────────────────────────────────────────────────────
        std::string dropBeforeID = "##drop_before_" + uuidStr;
        ImGui::PushID(dropBeforeID.c_str());
        ImGui::Selectable(" ", false, ImGuiSelectableFlags_AllowItemOverlap, ImVec2(ImGui::GetContentRegionAvail().x, 0.1f));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY"))
            {
                Entity dropped = *(Entity *)payload->Data;
                if (dropped != entity && !IsDescendantOf(entity, dropped))
                {
                    if (dropped.HasComponent<RectTransform>())
                    {
                        Entity oldParent = dropped.GetComponent<RectTransform>().parent;
                        auto &hierarchy = GetSceneManager().hierarchyElements;

                        // remove from old parent
                        if (oldParent)
                        {
                            auto &siblings = oldParent.GetComponent<RectTransform>().children;
                            siblings.erase(std::remove(siblings.begin(), siblings.end(), dropped), siblings.end());
                        }

                        // remove old parent
                        dropped.GetComponent<RectTransform>().parent.entityHandle = entt::null;

                        // set new parent (same as current entity’s parent)
                        if (entity.HasComponent<RectTransform>())
                            if (entity.GetComponent<RectTransform>().parent)
                                dropped.GetComponent<RectTransform>().parent = entity.GetComponent<RectTransform>().parent;

                        // Find parent’s child list or root list
                        std::vector<Entity> *list = nullptr;
                        if (entity.HasComponent<RectTransform>() && entity.GetComponent<RectTransform>().parent)
                        {
                            list = &entity.GetComponent<RectTransform>().parent.GetComponent<RectTransform>().children;
                        }
                        else
                        {
                            int dropIndex = 0;

                            for (int i = 0; i < GetSceneManager().hierarchyElements.size(); i++)
                            {
                                if (dropped == GetSceneManager().hierarchyElements[i].entity)
                                {
                                    dropIndex = i;
                                    break;
                                }
                            }

                            HierarchyElementInfo tempDropped = GetSceneManager().hierarchyElements[dropIndex];

                            GetSceneManager().hierarchyElements.erase(GetSceneManager().hierarchyElements.begin() + dropIndex);
                            GetSceneManager().hierarchyElements.insert(GetSceneManager().hierarchyElements.begin() + _index, tempDropped);
                        }

                        // Insert before current entity
                        if (list)
                        {
                            auto it = std::find(list->begin(), list->end(), entity);
                            if (it != list->end())
                                list->insert(it, dropped);
                        }

                        m_forceRefresh = true;
                    }
                    else
                    {
                        int dropIndex = 0;

                        for (int i = 0; i < GetSceneManager().hierarchyElements.size(); i++)
                        {
                            if (dropped == GetSceneManager().hierarchyElements[i].entity)
                            {
                                dropIndex = i;
                                break;
                            }
                        }

                        HierarchyElementInfo tempDropped = GetSceneManager().hierarchyElements[dropIndex];

                        GetSceneManager().hierarchyElements.erase(GetSceneManager().hierarchyElements.begin() + dropIndex);
                        GetSceneManager().hierarchyElements.insert(GetSceneManager().hierarchyElements.begin() + _index, tempDropped);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();

        // ─────────────────────────────────────────────────────
        // TREE NODE + DRAG SOURCE + DROP TARGET
        // ─────────────────────────────────────────────────────
        ImGuiTreeNodeFlags flags = (hasChildren ? 0 : ImGuiTreeNodeFlags_Leaf) | ImGuiTreeNodeFlags_FramePadding;
        bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

        if (ImGui::BeginDragDropSource())
        {
            ImGui::SetDragDropPayload("ENTITY_HIERARCHY", &entity, sizeof(Entity));
            ImGui::Text("Moving %s", GetSceneManager().hierarchyElements[_index].name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("ENTITY_HIERARCHY"))
            {
                Entity dropped = *(Entity *)payload->Data;

                if (entity.HasComponent<RectTransform>() && dropped.HasComponent<RectTransform>())
                {
                    if (dropped != entity && !IsDescendantOf(entity, dropped))
                    {
                        if (dropped.HasComponent<RectTransform>())
                        {
                            Entity oldParent = dropped.GetComponent<RectTransform>().parent;
                            if (oldParent)
                            {
                                auto &siblings = oldParent.GetComponent<RectTransform>().children;
                                siblings.erase(std::remove(siblings.begin(), siblings.end(), dropped), siblings.end());
                            }
                        }

                        dropped.GetComponent<RectTransform>().parent = entity;
                        entity.GetComponent<RectTransform>().children.push_back(dropped);

                        m_forceRefresh = true;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // ─────────────────────────────────────────────────────
        // UI ELEMENTS (name input, delete, duplicate)
        // ─────────────────────────────────────────────────────
        ImGui::SameLine();
        std::string inputID = "##input" + uuidStr;
        ImGui::InputText(inputID.c_str(), &GetSceneManager().hierarchyElements[_index].name);

        if (ImGui::IsItemFocused())
            m_index = _index;

        ImGui::SameLine();
        if (ImGui::Button(("x##" + uuidStr).c_str()))
        {
            entity.Destroy();
            m_forceRefresh = true;
            ImGui::TreePop();
            return true;
        }

        ImGui::SameLine();
        if (ImGui::Button(("d##" + uuidStr).c_str()))
        {
            HierarchyElementInfo hei;
            hei.entity = entity.Duplicate();
            hei.name = GetSceneManager().hierarchyElements[_index].name + " copy";
            GetSceneManager().hierarchyElements.insert(GetSceneManager().hierarchyElements.begin() + _index + 1, hei);
            m_forceRefresh = true;
            ImGui::TreePop();
            return true;
        }

        

        // ─────────────────────────────────────────────────────
        // CHILDREN RECURSION
        // ─────────────────────────────────────────────────────
        if (opened && entity.HasComponent<Canis::RectTransform>())
        {
            auto &rtc = entity.GetComponent<Canis::RectTransform>();
            for (Canis::Entity child : rtc.children)
            {
                for (int i = 0; i < GetSceneManager().hierarchyElements.size(); i++)
                {
                    if (child.entityHandle == GetSceneManager().hierarchyElements[i].entity.entityHandle)
                    {
                        DrawHierarchyElement(i);
                        break;
                    }
                }
            }
        }

        if (hasChildren == false || opened)
            ImGui::TreePop();
        
        return false;
    }

    void Editor::DrawScenePanel(Window *_window, Time *_time)
    {
        ImGui::Begin("Scene");

        if (m_mode == EditorMode::EDIT)
        {
            if (ImGui::Button("Play##ScenePanel"))
            {
                GetSceneManager().nextMessage.clear();

                GetSceneManager().ForceLoad(m_scene->name);

                m_mode = EditorMode::PLAY;

                ImGui::End();

                return;
            }

            ImGui::SameLine();

            std::vector<std::string> sceneNames = {};
            int currentSceneIndex = 0;
            int i = 0;

            for (SceneData sceneData : GetSceneManager().m_scenes)
            {
                sceneNames.push_back(sceneData.scene->name);

                if (sceneData.scene->name == m_scene->name)
                {
                    currentSceneIndex = i;
                }

                i++;
            }

            int wasSceneSelect = currentSceneIndex;

            std::vector<const char *> cStringItems = ConvertVectorToCStringVector(sceneNames);

            ImGui::Combo("##SceneList", &currentSceneIndex, cStringItems.data(), static_cast<int>(cStringItems.size()));

            if (wasSceneSelect != currentSceneIndex)
            {
                GetSceneManager().Load(sceneNames[currentSceneIndex]);

                ImGui::End();

                return;
            }
        }

        if (m_mode == EditorMode::PLAY)
        {
            if (ImGui::Button("Stop##ScenePanel"))
            {
                m_mode = EditorMode::EDIT;

                GetSceneManager().Load(m_scene->name);

                ImGui::End();

                // reset camera

                // stop audio when leaving play mode
                AudioManager::StopMusic();
                AudioManager::StopAllSounds();

                return;
            }
        }

        std::string fps = std::to_string(_time->fps);
        std::string deltaTime = std::to_string(m_scene->deltaTime);
        std::string cpuTime = std::to_string(GetSceneManager().updateTime);
        std::string gpuTime = std::to_string(GetSceneManager().drawTime);

        ImGui::Text("FPS: %s", fps.c_str());
        ImGui::Text("Delta Time: %s", deltaTime.c_str());
        ImGui::Text("CPU Time: %s", cpuTime.c_str());
        ImGui::Text("GPU Time: %s", gpuTime.c_str());

        ImGui::End();
    }

    bool Editor::IsDescendantOf(Entity _potentialAncestor, Entity _entity)
    {
        if (!_entity.HasComponent<Canis::RectTransform>())
            return false;

        Entity current = _entity.GetComponent<Canis::RectTransform>().parent;
        while (current)
        {
            if (current == _potentialAncestor)
                return true;

            if (!current.HasComponent<Canis::RectTransform>())
                break;

            current = current.GetComponent<Canis::RectTransform>().parent;
        }
        return false;
    }

}