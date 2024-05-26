#include <Canis/Editor.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/SceneManager.hpp>

#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>
#include <Canis/ECS/Components/UISliderKnobComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>

#include <SDL.h>
#include <GL/glew.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <glm/gtc/type_ptr.hpp>

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

    SceneManager& Editor::GetSceneManager()
    {
        return *((SceneManager *)m_scene->sceneManager);
    }


    void Editor::Init(Window *_window)
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
        // io.ConfigViewportsNoAutoMerge = true;
        // io.ConfigViewportsNoTaskBarIcon = true;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        const char *glsl_version = "#version 330";
        ImGui_ImplSDL2_InitForOpenGL((SDL_Window *)_window->GetSDLWindow(), (SDL_GLContext)_window->GetGLContext());
        ImGui_ImplOpenGL3_Init(glsl_version);
    }

    void Editor::Draw(Scene *_scene)
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
        DrawScenePanel();

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        ImGuiIO& io = ImGui::GetIO();
        (void)io;

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
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

    void Editor::DrawInspectorPanel()
    {
        static float f = 0.0f;
        static int counter = 0;
        static int lastIndex = 0;
        static UUID id = 0;
        static Entity entity(m_scene);
        static bool refresh = true;

        int count = 0;

        if (m_index != lastIndex)
        {
            lastIndex = m_index;
            refresh = true;
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


            int i = 0;

            for (HierarchyElementInfo hei : GetSceneManager().hierarchyElements)
            {
                if (i == m_index)
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

            if (entity.HasComponent<RectTransformComponent>())
            {
                if (ImGui::CollapsingHeader("Canis::RectTransform", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    auto &rtc = entity.GetComponent<RectTransformComponent>();

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
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::RectTransformComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<ColorComponent>())
            {
                if (ImGui::CollapsingHeader("Canis::Color"))
                {
                    auto &cc = entity.GetComponent<ColorComponent>();
                    ImGui::InputFloat4("color", glm::value_ptr(cc.color), "%.3f");
                    ImGui::InputFloat3("emission", glm::value_ptr(cc.emission), "%.3f");
                    ImGui::InputFloat("emissionUsingAlbedoIntesity", &cc.emissionUsingAlbedoIntesity);
                }

                if (ImGui::BeginPopupContextItem("Menu##Canis::Color"))
                {
                    if (ImGui::MenuItem("Remove##Canis::Color"))
                    {
                        GetComponent().removeComponentFuncs[std::string(type_name<Canis::ColorComponent>())](entity);
                    }

                    ImGui::EndPopup();
                }
            }

            if (entity.HasComponent<UIImageComponent>())
            {
                static std::string path = "";
                static std::string lastFramePath = "";

                auto &ic = entity.GetComponent<UIImageComponent>();

                if (refresh)
                {
                    path = AssetManager::Get<TextureAsset>(ic.textureHandle.id)->GetPath();
                    lastFramePath = path;
                }

                if (ImGui::CollapsingHeader("Canis::Image"))
                {
                    if (ImGui::CollapsingHeader("Texture Asset"))
                    {
                        std::string tempPath = std::string(path);

                        ImGui::InputText("path", &tempPath);

                        path = std::string(tempPath);

                        if (path != lastFramePath)
                        {
                            if (!path.empty())
                            {
                                if (FileExists(path.c_str()))
                                {
                                    int newID = AssetManager::LoadTexture(path);

                                    if (newID != -1)
                                    {
                                        ic.textureHandle.id = newID;
                                        ic.texture = AssetManager::Get<TextureAsset>(ic.textureHandle.id)->GetTexture();

                                        path = AssetManager::Get<TextureAsset>(ic.textureHandle.id)->GetPath();
                                        lastFramePath = path;
                                    }
                                }
                                else
                                {
                                    Canis::Log("File does not exist: " + path);
                                }
                            }
                        }
                    }

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
                static std::string lastFramePath = "";
                static int lastFrameSize = 0;

                auto &tc = entity.GetComponent<TextComponent>();

                if (refresh)
                {
                    path = AssetManager::Get<TextAsset>(tc.assetId)->GetPath();
                    lastFramePath = path;

                    size = AssetManager::Get<TextAsset>(tc.assetId)->GetFontSize();
                    lastFrameSize = size;
                }

                if (ImGui::CollapsingHeader("Canis::Text"))
                {
                    if (ImGui::CollapsingHeader("Text Asset"))
                    {
                        std::string tempPath = std::string(path);
                        int tempSize = size;

                        ImGui::InputText("path", &tempPath);
                        ImGui::InputInt("size", &tempSize);

                        path = std::string(tempPath);
                        size = tempSize;

                        if (path != lastFramePath || size != lastFrameSize)
                        {
                            if (size < 0)
                                size = 12;

                            if (!path.empty())
                            {
                                if (FileExists(path.c_str()))
                                {
                                    int newID = AssetManager::LoadText(path, size);

                                    if (newID != -1)
                                    {
                                        tc.assetId = newID;

                                        path = AssetManager::Get<TextAsset>(tc.assetId)->GetPath();
                                        lastFramePath = path;

                                        size = AssetManager::Get<TextAsset>(tc.assetId)->GetFontSize();
                                        lastFrameSize = size;
                                    }
                                }
                                else
                                {
                                    Canis::Log("File does not exist: " + path);
                                }
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

                    if (ImGui::InputScalar("Up", ImGuiDataType_U64, &tempIDUp))
                    {
                        UUID u = tempIDUp;
                        GetSceneManager().FindEntityEditor(bc.up, u);
                        tempIDUp = u.ID;
                    }

                    if (ImGui::InputScalar("Down", ImGuiDataType_U64, &tempIDDown))
                    {
                        UUID u = tempIDDown;
                        GetSceneManager().FindEntityEditor(bc.down, u);
                        tempIDDown = u.ID;
                    }

                    if (ImGui::InputScalar("Left", ImGuiDataType_U64, &tempIDLeft))
                    {
                        UUID u = tempIDLeft;
                        GetSceneManager().FindEntityEditor(bc.left, u);
                        tempIDLeft = u.ID;
                    }

                    if (ImGui::InputScalar("Right", ImGuiDataType_U64, &tempIDRight))
                    {
                        UUID u = tempIDRight;
                        GetSceneManager().FindEntityEditor(bc.right, u);
                        tempIDRight = u.ID;
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
                        GetSceneManager().FindEntityEditor(kc.slider, u);
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
            if (ImGui::Selectable((uuidStr + selectableID).c_str(), m_index == i, ImGuiSelectableFlags_AllowOverlap))
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
                }
            }

            ImGui::SameLine();
            std::string removeButtonLabel = "x##" + std::to_string(i);
            removeButtonLabel += std::to_string(GetSceneManager().hierarchyElements[i].entity.GetUUID());
            if (ImGui::Button(removeButtonLabel.c_str()))
            {
                GetSceneManager().hierarchyElements.erase(GetSceneManager().hierarchyElements.begin() + i);
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
        }

        ImGui::End();
    }

    void Editor::DrawScenePanel()
    {
        ImGui::Begin("Scene");

        if (m_mode == EditorMode::EDIT)
        {
            if (ImGui::Button("Play##ScenePanel"))
            {
                m_mode = EditorMode::PLAY;

                GetSceneManager().Load(m_scene->name);

                ImGui::End();

                return;
            }

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

                return;
            }
        }

        ImGui::End();
    }
}