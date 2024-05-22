#include <Canis/SceneManager.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>

#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>
#include <Canis/ECS/Components/UISliderKnobComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>

#include <SDL_keyboard.h>

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

    void SceneManager::FindEntityEditor(Entity &_entity, UUID &_uuid)
    {
        for (auto euuid : entityAndUUIDToConnect)
        {
            if (_uuid == euuid.uuid)
            {
                _entity.entityHandle = euuid.entity->entityHandle;
                return;
            }
        }

        _uuid = 0lu;
    }

    void SetUpIMGUI(Window *_window)
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // ImGui::StyleColorsLight();

        // Setup Platform/Renderer backends
        const char *glsl_version = "#version 330";
        ImGui_ImplSDL2_InitForOpenGL((SDL_Window *)_window->GetSDLWindow(), (SDL_GLContext)_window->GetGLContext());
        ImGui_ImplOpenGL3_Init(glsl_version);
    }

    // Function to copy text to the clipboard
    void CopyToClipboard(const std::string &text)
    {
        ImGui::SetClipboardText(text.c_str());
    }

    // Function to display clickable entity ID
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

    SceneManager::SceneManager() {}

    SceneManager::~SceneManager()
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            delete m_scenes[i].scene;
        }

        m_scenes.clear();
    }

    int SceneManager::Add(Scene *s)
    {
        SceneData sceneData;
        sceneData.scene = s;
        m_scenes.push_back(sceneData);

        return m_scenes.size() - 1;
    }

    int SceneManager::AddSplashScene(Scene *s)
    {
        SceneData sceneData;
        sceneData.scene = s;
        sceneData.splashScreen = true;
        m_scenes.push_back(sceneData);

        return m_scenes.size() - 1;
    }

    void SceneManager::PreLoadAll()
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].preloaded == false)
            {
                m_scenes[i].scene->window = window;
                m_scenes[i].scene->inputManager = inputManager;
                m_scenes[i].scene->sceneManager = (unsigned int *)this;
                m_scenes[i].scene->time = time;
                m_scenes[i].scene->camera = camera;
                m_scenes[i].scene->seed = seed;

                if (m_scenes[i].scene->path != "")
                {
                    YAML::Node root = YAML::LoadFile(m_scenes[i].scene->path);

                    m_scenes[i].scene->name = root["Scene"].as<std::string>();

                    // serialize systems
                    if (YAML::Node systems = root["Systems"])
                    {
                        for (int s = 0; s < systems.size(); s++)
                        {
                            std::string name = systems[s].as<std::string>();
                            for (int d = 0; d < decodeSystem.size(); d++)
                            {
                                if (decodeSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }

                    // serialize render systems
                    if (YAML::Node renderSystems = root["RenderSystems"])
                    {
                        for (int r = 0; r < renderSystems.size(); r++)
                        {
                            std::string name = renderSystems[r].as<std::string>();
                            for (int d = 0; d < decodeRenderSystem.size(); d++)
                            {
                                if (decodeRenderSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }
                }

                m_scenes[i].scene->PreLoad();
                m_scenes[i].preloaded = true;
            }
        }
    }

    void SceneManager::PreLoad(std::string _sceneName)
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].preloaded == false && m_scenes[i].scene->name == _sceneName)
            {
                m_scenes[i].scene->window = window;
                m_scenes[i].scene->inputManager = inputManager;
                m_scenes[i].scene->sceneManager = (unsigned int *)this;
                m_scenes[i].scene->time = time;
                m_scenes[i].scene->camera = camera;
                m_scenes[i].scene->seed = seed;

                if (m_scenes[i].scene->path != "")
                {
                    YAML::Node root = YAML::LoadFile(m_scenes[i].scene->path);

                    m_scenes[i].scene->name = root["Scene"].as<std::string>();

                    // serialize systems
                    if (YAML::Node systems = root["Systems"])
                    {
                        for (int s = 0; s < systems.size(); s++)
                        {
                            std::string name = systems[s].as<std::string>();
                            for (int d = 0; d < decodeSystem.size(); d++)
                            {
                                if (decodeSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }

                    // serialize render systems
                    if (YAML::Node renderSystems = root["RenderSystems"])
                    {
                        for (int r = 0; r < renderSystems.size(); r++)
                        {
                            std::string name = renderSystems[r].as<std::string>();
                            for (int d = 0; d < decodeRenderSystem.size(); d++)
                            {
                                if (decodeRenderSystem[d](name, m_scenes[i].scene))
                                    continue;
                            }
                        }
                    }
                }

                m_scenes[i].scene->PreLoad();
                m_scenes[i].preloaded = true;
            }
        }
    }

    void SceneManager::Load(int _index)
    {
        static bool notInit = true;

        if (notInit)
        {
            notInit = true;

            SetUpIMGUI(window);
        }

        if (m_scenes.size() < _index)
            FatalError("Failed to load scene at index " + std::to_string(_index));

        patientLoadIndex = -1;

        if (scene != nullptr)
        {
            scene->UnLoad();
            {
                // Clean up ScriptableEntity pointer
                scene->entityRegistry.view<ScriptComponent>().each([](auto entity, auto &scriptComponent)
                                                                   {
                    if (scriptComponent.Instance)
                    {
                        scriptComponent.Instance->OnDestroy();
                    } });
            }

            // swap maps
            message.swap(nextMessage);
            nextMessage.clear();

            scene->entityRegistry = entt::registry();
        }

        scene = m_scenes[_index].scene;
        m_sceneIndex = _index;

        if (!scene->IsPreLoaded())
        {
            scene->window = window;
            scene->inputManager = inputManager;
            scene->time = time;
            scene->camera = camera;

            scene->PreLoad();
        }

        scene->Load();

        // in case someone calls load in load
        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
        }

        if (scene->path != "")
        {
            YAML::Node root = YAML::LoadFile(scene->path);

            entityAndUUIDToConnect.clear();

            window->SetClearColor(
                root["ClearColor"].as<glm::vec4>(glm::vec4(0.05f, 0.05f, 0.05f, 1.0f)));
            window->ClearColor();

            auto entities = root["Entities"];

            if (entities)
            {
                for (auto e : entities)
                {
                    Canis::Entity entity = scene->CreateEntity();

                    entity.AddComponent<IDComponent>(UUID(e["Entity"].as<uint64_t>(0)));

                    for (int d = 0; d < decodeEntity.size(); d++)
                        decodeEntity[d](e, entity, this);

                    if (auto scriptComponent = e["Canis::ScriptComponent"])
                        for (int d = 0; d < decodeScriptableEntity.size(); d++)
                            if (decodeScriptableEntity[d](scriptComponent.as<std::string>(), entity))
                                continue;

                    if (auto prefab = e["Prefab"])
                    {
                        scene->Instantiate(prefab.as<std::string>());
                    }
                }

                for (auto euuid : entityAndUUIDToConnect)
                {
                    euuid.entity->scene = scene;

                    auto view = scene->entityRegistry.view<IDComponent>();

                    for (auto [entity, id] : view.each())
                    {
                        if (id.ID == euuid.uuid)
                        {
                            euuid.entity->entityHandle = entity;
                            break;
                        }
                    }
                }
            }
        }

        for (int i = 0; i < scene->systems.size(); i++)
        {
            if (!scene->systems[i]->IsCreated())
            {
                scene->systems[i]->Create();
                scene->systems[i]->m_isCreated = true;
            }
        }

        for (int i = 0; i < scene->systems.size(); i++)
        {
            scene->systems[i]->Ready();
        }
    }

    void SceneManager::ForceLoad(std::string _name)
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].scene->name == _name)
            {
                Load(i);
                return;
            }
        }

        FatalError("Scene not found with name " + _name);
    }

    void SceneManager::Load(std::string _name)
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].scene->name == _name)
            {
                patientLoadIndex = i;
                return;
            }
        }

        Warning("SceneManager: " + _name + " NOT FOUND");
    }

    void SceneManager::Save()
    {
        Log("Save Scene");
        YAML::Emitter out;
        out << YAML::BeginMap;
        out << YAML::Key << "Scene" << YAML::Value << scene->name;

        out << YAML::Key << "ClearColor" << YAML::Value << window->GetScreenColor();

        // Serialize System
        out << YAML::Key << "Systems";
        out << YAML::BeginSeq;
        for (System *s : scene->m_updateSystems)
        {
            out << YAML::Value << s->GetName();
        }
        out << YAML::EndSeq;

        // Serialize Render System
        out << YAML::Key << "RenderSystems";
        out << YAML::BeginSeq;
        for (System *s : scene->m_renderSystems)
        {
            out << YAML::Value << s->GetName();
        }
        out << YAML::EndSeq;

        out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;
        scene->entityRegistry.each([&](auto _entityHandle)
                                   {
			Entity entity = { _entityHandle, scene };
            
			if (!entity)
				return;

            if (!entity.HasComponent<IDComponent>())
                return;
            
            out << YAML::BeginMap;

            out << YAML::Key << "Entity" << YAML::Key << std::to_string(entity.GetUUID());

            for (int i = 0; i < encodeEntity.size(); i++)
                encodeEntity[i](out, entity);
            
            if (entity.HasComponent<ScriptComponent>())
                for (int i = 0; i < encodeScriptableEntity.size(); i++)
                    if (encodeScriptableEntity[i](out, entity.GetComponent<ScriptComponent>().Instance))
                        break;
            
            out << YAML::EndMap; });
        out << YAML::EndSeq;
        out << YAML::EndMap;

        if (scene->path.size() > 0)
        {
            std::ofstream fout(scene->path);
            fout << out.c_str();
        }
        else
        {
            std::ofstream fout(scene->name);
            fout << out.c_str();
        }
    }

    bool SceneManager::IsSplashScene(std::string _sceneName)
    {
        for (int i = 0; i < m_scenes.size(); i++)
        {
            if (m_scenes[i].scene->name == _sceneName && m_scenes[i].splashScreen == true)
            {
                return true;
            }
        }

        return false;
    }

    void SceneManager::HotReload()
    {
        if (scene == nullptr)
            Load(scene->name);
    }

    void SceneManager::Update()
    {
        m_updateStart = high_resolution_clock::now();
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");

        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
            patientLoadIndex = -1;
        }

        auto view = scene->entityRegistry.view<Canis::ScriptComponent>();

        for (auto [_entity, _scriptComponent] : view.each())
        {
            if (!_scriptComponent.Instance)
            {
                _scriptComponent.Instance = _scriptComponent.InstantiateScript();
                _scriptComponent.Instance->entity = Entity{_entity, this->scene};
                _scriptComponent.Instance->OnCreate();
            }
        }

        scene->Update();

        for (auto [_entity, _scriptComponent] : view.each())
        {
            if (!_scriptComponent.Instance->isOnReadyCalled)
            {
                _scriptComponent.Instance->isOnReadyCalled = true;
                _scriptComponent.Instance->OnReady();
            }

            _scriptComponent.Instance->OnUpdate(this->scene->deltaTime);
        }

        m_updateEnd = high_resolution_clock::now();
        updateTime = std::chrono::duration_cast<std::chrono::nanoseconds>(m_updateEnd - m_updateStart).count() / 1000000000.0f;
    }

    void SceneManager::LateUpdate()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");

        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
            patientLoadIndex = -1;
        }

        scene->LateUpdate();

        if (m_scenes[m_sceneIndex].splashScreenHasCalledLoadAll == false && m_scenes[m_sceneIndex].splashScreen)
        {
            PreLoadAll();
            m_scenes[m_sceneIndex].splashScreenHasCalledLoadAll = true;
        }
    }

    void SceneManager::Draw()
    {
        m_drawStart = high_resolution_clock::now();

        if (scene == nullptr)
            FatalError("A scene has not been loaded.");

        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
            patientLoadIndex = -1;
        }

        scene->Draw();

        /////////////////////////////////////

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;
            static int m_index = 0;
            static UUID id = 0;
            static Entity entity(scene);
            static bool refresh = true;

            int count = 0;

            ImGui::Begin("Hello, Editor!"); // Create a window called "Hello, world!" and append into it.

            if (ImGui::Button("Back"))
            {
                refresh = true;
                m_index--;
            }
            ImGui::SameLine();
            ShowEntityID((uint64_t)id);
            ImGui::SameLine();

            if (ImGui::Button("Next"))
            {
                refresh = true;
                m_index++;
            }

            auto view = scene->entityRegistry.view<IDComponent>();

            for (auto [entityID, idComponent] : view.each())
            {
                count++;
            }

            if (m_index < 0)
                m_index = count - 1;

            if (m_index >= count)
                m_index = 0;

            if (count == 0)
                return;

            int i = 0;

            for (auto [entityID, idComponent] : view.each())
            {
                if (i == m_index)
                {
                    id = idComponent.ID;
                    entity.entityHandle = entityID;
                }

                i++;
            }

            if (entity.HasComponent<TagComponent>())
            {
                if (ImGui::CollapsingHeader("Canis::Tag"))
                {
                    auto& tagComponent = entity.GetComponent<TagComponent>();

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
                        FindEntityEditor(bc.up, u);
                        tempIDUp = u.ID;
                    }

                    if (ImGui::InputScalar("Down", ImGuiDataType_U64, &tempIDDown))
                    {
                        UUID u = tempIDDown;
                        FindEntityEditor(bc.down, u);
                        tempIDDown = u.ID;
                    }

                    if (ImGui::InputScalar("Left", ImGuiDataType_U64, &tempIDLeft))
                    {
                        UUID u = tempIDLeft;
                        FindEntityEditor(bc.left, u);
                        tempIDLeft = u.ID;
                    }

                    if (ImGui::InputScalar("Right", ImGuiDataType_U64, &tempIDRight))
                    {
                        UUID u = tempIDRight;
                        FindEntityEditor(bc.right, u);
                        tempIDRight = u.ID;
                    }

                    ImGui::Checkbox("defaultSelected", &bc.defaultSelected);
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
                        FindEntityEditor(kc.slider, u);
                        tempID = u.ID;
                    }

                    ImGui::Checkbox("grabbed", &kc.grabbed);
                    ImGui::SliderFloat("value", &kc.value, 0.0f, 1.0f);
                }
            }

            refresh = false; // keep here
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        /////////////////////////////////////

        glFinish(); // make sure all queued command are finished

        m_drawEnd = high_resolution_clock::now();
        drawTime = std::chrono::duration_cast<std::chrono::nanoseconds>(m_drawEnd - m_drawStart).count() / 1000000000.0f;
    }

    void SceneManager::InputUpdate()
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");

        if (patientLoadIndex != -1)
        {
            Load(patientLoadIndex);
            patientLoadIndex = -1;
        }

        scene->InputUpdate();

        if (inputManager->JustPressedKey(SDLK_F12))
        {
            window->ToggleFullScreen();
        }
    }

    void SceneManager::SetDeltaTime(double _deltaTime)
    {
        if (scene == nullptr)
            FatalError("A scene has not been loaded.");

        scene->unscaledDeltaTime = _deltaTime;
        scene->deltaTime = _deltaTime * scene->timeScale;
    }
} // end of Canis namespace
