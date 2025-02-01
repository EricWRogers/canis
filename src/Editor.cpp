#include <Canis/Editor.hpp>
#include <Canis/Canis.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/SceneManager.hpp>
#include <Canis/Window.hpp>
#include <Canis/Time.hpp>
#include <Canis/Math.hpp>
#include <Canis/AudioManager.hpp>

#include <Canis/ECS/Components/Transform.hpp>
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

#include <Canis/External/OpenGl.hpp>

#include <SDL.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

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

            // Rendering
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
                if (ImGui::CollapsingHeader("Canis::RectTransform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto &rtc = entity.GetComponent<RectTransform>();

                    ImGui::Checkbox("active", &rtc.active);

                    ImGui::Combo("anchor", &rtc.anchor, RectAnchorLabels, IM_ARRAYSIZE(RectAnchorLabels));

                    ImGui::InputFloat2("position", glm::value_ptr(rtc.position), "%.3f");
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

                    static const char *AlignmentLabels[] = {
                        "Left", "Right", "Center"};

                    int *a = ((int *)&tc.alignment); // this might be bad because imgui uses -1 as error

                    ImGui::Combo("alignment", a, AlignmentLabels, IM_ARRAYSIZE(AlignmentLabels));
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
            // Convert UUID to string
            std::string uuidStr = std::to_string(GetSceneManager().hierarchyElements[i].entity.GetUUID());

            // Create a unique ID for each selectable item to ensure ImGui can differentiate them
            std::string selectableID = "##" + uuidStr;

            // Use ImGui::Selectable to create a clickable text item
            ImGui::InputText((selectableID).c_str(), &GetSceneManager().hierarchyElements[i].name); // ImGui::Selectable((uuidStr + selectableID).c_str(), m_index == i, ImGuiSelectableFlags_AllowOverlap))

            if (ImGui::IsItemFocused())
            {
                m_index = i;
            }

            ImGui::SameLine();
            ImGui::Text(" ");

            if (i != 0)
            {
                ImGui::SameLine();
                std::string upButtonLabel = "^##" + std::to_string(i);
                upButtonLabel += std::to_string(GetSceneManager().hierarchyElements[i].entity.GetUUID());
                if (ImGui::Button(upButtonLabel.c_str()))
                {
                    HierarchyElementInfo temp = GetSceneManager().hierarchyElements[i - 1];
                    GetSceneManager().hierarchyElements[i - 1] = GetSceneManager().hierarchyElements[i];
                    GetSceneManager().hierarchyElements[i] = temp;
                    m_forceRefresh = true;
                }
            }

            if (i != GetSceneManager().hierarchyElements.size() - 1)
            {
                ImGui::SameLine();
                std::string downButtonLabel = "v##" + std::to_string(i);
                downButtonLabel += std::to_string(GetSceneManager().hierarchyElements[i].entity.GetUUID());
                if (ImGui::Button(downButtonLabel.c_str()))
                {
                    HierarchyElementInfo temp = GetSceneManager().hierarchyElements[i];
                    GetSceneManager().hierarchyElements[i] = GetSceneManager().hierarchyElements[i + 1];
                    GetSceneManager().hierarchyElements[i + 1] = temp;
                    m_forceRefresh = true;
                }
            }

            ImGui::SameLine();
            std::string removeButtonLabel = "x##" + std::to_string(i);
            removeButtonLabel += std::to_string(GetSceneManager().hierarchyElements[i].entity.GetUUID());
            if (ImGui::Button(removeButtonLabel.c_str()))
            {
                GetSceneManager().hierarchyElements[i].entity.Destroy();
                GetSceneManager().hierarchyElements.erase(GetSceneManager().hierarchyElements.begin() + i);
                m_forceRefresh = true;
                continue;
            }

            ImGui::SameLine();
            std::string duplicateButtonLabel = "d##" + std::to_string(i);
            duplicateButtonLabel += std::to_string(GetSceneManager().hierarchyElements[i].entity.GetUUID());
            if (ImGui::Button(duplicateButtonLabel.c_str()))
            {
                Canis::HierarchyElementInfo hei;
                hei.entity = GetSceneManager().hierarchyElements[i].entity.Duplicate();
                hei.name = GetSceneManager().hierarchyElements[i].name + " copy";
                GetSceneManager().hierarchyElements.insert(GetSceneManager().hierarchyElements.begin() + i + 1, hei);
                m_forceRefresh = true;
                continue;
            }
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

    void Editor::DrawScenePanel(Window *_window, Time *_time)
    {
        ImGui::Begin("Scene");

        if (m_mode == EditorMode::EDIT)
        {
            if (ImGui::Button("Play##ScenePanel"))
            {
                m_mode = EditorMode::PLAY;

                GetSceneManager().nextMessage.clear();

                GetSceneManager().Load(m_scene->name);

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
}