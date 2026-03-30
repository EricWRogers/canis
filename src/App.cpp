#include "yaml-cpp/emittermanip.h"
#include <Canis/App.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_init.h>

#include <Canis/Canis.hpp>
#include <Canis/GameCodeObject.hpp>
#include <Canis/Time.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Window.hpp>
#include <Canis/Editor.hpp>
#include <Canis/IOManager.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/ConfigHelper.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <filesystem>
#include <memory>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace Canis
{
    struct App::RuntimeContext
    {
        std::unique_ptr<Window> window;
        std::unique_ptr<Editor> editor;
        std::unique_ptr<InputManager> inputManager;
        GameCodeObject gameCodeObject = {};
        bool editorRuntimeEnabled = false;
    };

    namespace
    {
        namespace fs = std::filesystem;

        const char *GetGameCodeSharedObjectPath()
        {
#if defined(__EMSCRIPTEN__)
            return "";
#elif defined(_WIN32)
            return "./libGameCode.dll";
#elif defined(__APPLE__)
            return "./libGameCode.dylib";
#elif defined(__linux__)
            return "./libGameCode.so";
#else
            return "";
#endif
        }

        bool HasAssetsFolder(const fs::path &_path)
        {
            const fs::path assetsPath = _path / "assets";
            return fs::exists(assetsPath) && fs::is_directory(assetsPath);
        }

        void ResolveProjectWorkingDirectory()
        {
            if (HasAssetsFolder(fs::current_path()))
                return;

            std::vector<fs::path> candidatePaths = {};
            if (const char *basePath = SDL_GetBasePath())
            {
                candidatePaths.emplace_back(basePath);
                SDL_free(const_cast<char *>(basePath));
            }

            candidatePaths.push_back(fs::current_path() / "project");

            for (const fs::path &candidatePath : candidatePaths)
            {
                if (!HasAssetsFolder(candidatePath))
                    continue;

                fs::current_path(candidatePath);
                break;
            }
        }

        void LoadProjectAssetMetadata()
        {
            std::vector<std::string> paths = FindFilesInFolder("assets", "");
            for (const std::string &path : paths)
                (void)AssetManager::GetMetaFile(path);
        }

#if CANIS_EDITOR
        std::vector<ScriptConf*> GetConnectedUIScriptOptions(App& _app, Entity* _targetEntity)
        {
            std::vector<ScriptConf*> options = {};

            if (_targetEntity == nullptr)
                return options;

            for (ScriptConf& conf : _app.GetScriptRegistry())
            {
                if (conf.uiActions.empty() || conf.Has == nullptr || conf.Get == nullptr)
                    continue;

                if (!conf.Has(*_targetEntity))
                    continue;

                if (conf.Get(*_targetEntity) == nullptr)
                    continue;

                options.push_back(&conf);
            }

            std::sort(options.begin(), options.end(), [](const ScriptConf* _left, const ScriptConf* _right) -> bool
            {
                if (_left == nullptr || _right == nullptr)
                    return _left != nullptr;

                return _left->name < _right->name;
            });

            return options;
        }

        std::vector<std::string> GetConnectedUIActionOptions(ScriptConf* _scriptConf)
        {
            std::vector<std::string> options = {};

            if (_scriptConf == nullptr)
                return options;

            options.reserve(_scriptConf->uiActions.size());
            for (const auto& [actionName, actionInvoker] : _scriptConf->uiActions)
            {
                (void)actionInvoker;
                options.push_back(actionName);
            }

            std::sort(options.begin(), options.end());
            return options;
        }

        void DrawConnectedUIActionSelector(App& _app, Entity* _targetEntity, std::string& _targetScript, std::string& _actionName, const char* _idSuffix)
        {
            std::vector<ScriptConf*> scriptOptions = GetConnectedUIScriptOptions(_app, _targetEntity);

            ScriptConf* selectedScript = nullptr;
            for (ScriptConf* option : scriptOptions)
            {
                if (option != nullptr && option->name == _targetScript)
                {
                    selectedScript = option;
                    break;
                }
            }

            if (selectedScript == nullptr)
            {
                _targetScript.clear();
                _actionName.clear();
            }

            const std::string scriptLabel = BuildInspectorFieldLabel("targetScript", _idSuffix);
            const char* scriptPreview = _targetEntity == nullptr ? "<Select Target Entity>" :
                (_targetScript.empty() ? "<Select Script>" : _targetScript.c_str());

            ImGui::BeginDisabled(_targetEntity == nullptr);
            if (ImGui::BeginCombo(scriptLabel.c_str(), scriptPreview))
            {
                const bool noneSelected = _targetScript.empty();
                if (ImGui::Selectable("<None>", noneSelected))
                {
                    _targetScript.clear();
                    _actionName.clear();
                    selectedScript = nullptr;
                }

                if (noneSelected)
                    ImGui::SetItemDefaultFocus();

                for (ScriptConf* option : scriptOptions)
                {
                    if (option == nullptr)
                        continue;

                    const bool isSelected = (_targetScript == option->name);
                    if (ImGui::Selectable(option->name.c_str(), isSelected))
                    {
                        _targetScript = option->name;
                        selectedScript = option;
                        _actionName.clear();
                    }

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::EndDisabled();

            std::vector<std::string> actionOptions = GetConnectedUIActionOptions(selectedScript);
            const bool hasSelectedAction = std::find(actionOptions.begin(), actionOptions.end(), _actionName) != actionOptions.end();
            if (!hasSelectedAction)
                _actionName.clear();

            const std::string actionLabel = BuildInspectorFieldLabel("actionName", _idSuffix);
            const bool disableActions = selectedScript == nullptr;
            const char* actionPreview = disableActions ? "<Select Script First>" :
                (_actionName.empty() ? "<Select Action>" : _actionName.c_str());

            ImGui::BeginDisabled(disableActions);
            if (ImGui::BeginCombo(actionLabel.c_str(), actionPreview))
            {
                const bool noneSelected = _actionName.empty();
                if (ImGui::Selectable("<None>", noneSelected))
                    _actionName.clear();

                if (noneSelected)
                    ImGui::SetItemDefaultFocus();

                for (const std::string& actionName : actionOptions)
                {
                    const bool isSelected = (_actionName == actionName);
                    if (ImGui::Selectable(actionName.c_str(), isSelected))
                        _actionName = actionName;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }
            ImGui::EndDisabled();
        }
#endif
    }

    App::~App()
    {
        ShutdownRuntime();
    }

    void App::InitializeRuntime()
    {
        if (m_runtime != nullptr)
            return;

        Debug::Log("App Run");
        ResolveProjectWorkingDirectory();
        LoadProjectAssetMetadata();
        Canis::Init();

        m_runtime = new RuntimeContext();
        RuntimeContext &runtime = *m_runtime;

#if CANIS_EDITOR
        runtime.editorRuntimeEnabled = Canis::GetProjectConfig().editor;
#endif

        const int startupWidth = std::max(320, runtime.editorRuntimeEnabled ? GetProjectConfig().editorWindowWidth
                                                                             : GetProjectConfig().targetGameWidth);
        const int startupHeight = std::max(240, runtime.editorRuntimeEnabled ? GetProjectConfig().editorWindowHeight
                                                                              : GetProjectConfig().targetGameHeight);
        runtime.window = std::make_unique<Window>("Canis Beta", startupWidth, startupHeight);
        runtime.window->SetClearColor(Color(1.0f));
        runtime.window->SetSync(static_cast<Window::Sync>(GetProjectConfig().syncMode));

        if (GetProjectConfig().iconUUID == UUID(0))
        {
            if (MetaFileAsset *iconMeta = AssetManager::GetMetaFile("assets/defaults/textures/engine_icon.png"))
            {
                GetProjectConfig().iconUUID = iconMeta->uuid;
                SaveProjectConfig();
            }
        }

        const std::string iconPath = AssetManager::GetPath(GetProjectConfig().iconUUID);
        if (iconPath != "Path was not found in AssetLibrary")
            runtime.window->SetWindowIcon(iconPath);

        runtime.editor = std::make_unique<Editor>();
        m_editor = runtime.editor.get();
#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled)
            runtime.editor->Init(runtime.window.get());
#endif

        RegisterDefaults(*runtime.editor);

        runtime.inputManager = std::make_unique<InputManager>();
        runtime.inputManager->SetGameInputWindowID(SDL_GetWindowID((SDL_Window*)runtime.window->GetSDLWindow()));

        if (Canis::GetProjectConfig().useFrameLimit)
            Time::Init(Canis::GetProjectConfig().frameLimit + 0.0f);
        else
            Time::Init(100000.0f);

#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled)
            Time::SetTargetFPS(Canis::GetProjectConfig().frameLimitEditor + 0.0f);
#endif

        scene.Init(this, runtime.window.get(), runtime.inputManager.get(), "assets/scenes/game_loop.scene");

        runtime.gameCodeObject = GameCodeObjectInit(GetGameCodeSharedObjectPath());
        GameCodeObjectInitFunction(&runtime.gameCodeObject, this);

        scene.Load(m_scriptRegistry);
    }

    bool App::RunFrame()
    {
        if (m_runtime == nullptr)
            return false;

        RuntimeContext &runtime = *m_runtime;
        Window &window = *runtime.window;
        Editor &editor = *runtime.editor;
        InputManager &inputManager = *runtime.inputManager;
        GameCodeObject &gameCodeObject = runtime.gameCodeObject;

        if (!inputManager.Update((void *)&window))
            return false;

        if (window.ShouldClose())
            return false;

        if (window.IsResized())
        {
            if (runtime.editorRuntimeEnabled)
            {
                GetProjectConfig().editorWindowWidth = window.GetWindowWidth();
                GetProjectConfig().editorWindowHeight = window.GetWindowHeight();
            }
            else
            {
                GetProjectConfig().targetGameWidth = window.GetWindowWidth();
                GetProjectConfig().targetGameHeight = window.GetWindowHeight();
            }
        }

        f32 deltaTime = Time::StartFrame();

        bool runGameTick = true;
#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled)
            runGameTick = (editor.m_mode == EditorMode::PLAY);
#endif

        if (runGameTick)
        {
            Uint64 sceneUpdateStart = SDL_GetTicksNS();
            scene.Update(deltaTime);
            m_sceneUpdateTimeMs = static_cast<float>(SDL_GetTicksNS() - sceneUpdateStart) / 1000000.0f;

            Uint64 gameCodeUpdateStart = SDL_GetTicksNS();
            GameCodeObjectUpdateFunction(&gameCodeObject, this, deltaTime);
            m_gameCodeUpdateTimeMs = static_cast<float>(SDL_GetTicksNS() - gameCodeUpdateStart) / 1000000.0f;

            m_updateTimeMs = m_sceneUpdateTimeMs + m_gameCodeUpdateTimeMs;
        }
        else
        {
            m_updateTimeMs = 0.0f;
            m_sceneUpdateTimeMs = 0.0f;
            m_gameCodeUpdateTimeMs = 0.0f;
        }

        Uint64 renderStart = SDL_GetTicksNS();
        window.Clear();
#if CANIS_EDITOR
        if (runtime.editorRuntimeEnabled)
        {
            editor.Draw(&scene, &window, this, &gameCodeObject, deltaTime);
            inputManager.SetGameInputWindowID(editor.GetGameInputWindowID());
        }
        else
#endif
        {
            scene.Render(deltaTime);
            inputManager.SetGameInputWindowID(SDL_GetWindowID((SDL_Window*)window.GetSDLWindow()));
        }
        window.SwapBuffer();
        m_renderTimeMs = static_cast<float>(SDL_GetTicksNS() - renderStart) / 1000000.0f;

        Time::EndFrame();

        return true;
    }

    void App::ShutdownRuntime()
    {
        if (m_runtime == nullptr)
            return;

        RuntimeContext *runtime = m_runtime;
        m_runtime = nullptr;

        if (runtime->window != nullptr)
        {
            if (runtime->editorRuntimeEnabled)
            {
                GetProjectConfig().editorWindowWidth = runtime->window->GetWindowWidth();
                GetProjectConfig().editorWindowHeight = runtime->window->GetWindowHeight();
            }
            else
            {
                GetProjectConfig().targetGameWidth = runtime->window->GetWindowWidth();
                GetProjectConfig().targetGameHeight = runtime->window->GetWindowHeight();
            }

            SaveProjectConfig();
        }

        scene.Unload();
        Time::Quit();
        GameCodeObjectShutdownFunction(&runtime->gameCodeObject, this);
        GameCodeObjectDestroy(&runtime->gameCodeObject);
        m_editor = nullptr;
        delete runtime;
    }

#if defined(__EMSCRIPTEN__)
    void App::WebMainLoop(void *_appPtr)
    {
        App *app = static_cast<App *>(_appPtr);
        if (app == nullptr)
            return;

        if (!app->RunFrame())
        {
            app->ShutdownRuntime();
            emscripten_cancel_main_loop();
        }
    }
#endif

    void App::Run()
    {
        InitializeRuntime();

#if defined(__EMSCRIPTEN__)
        emscripten_set_main_loop_arg(&App::WebMainLoop, this, 0, true);
#else
        while (RunFrame())
        {
        }
        ShutdownRuntime();
#endif
    }

    void App::RegisterDefaults(Editor& _editor)
    {
        _editor.RegisterInspectorFieldDrawer<Canis::Entity*>([](Editor& _editor, const char* _label, const char* _idSuffix, Canis::Entity*& _value)
        {
            _editor.InputEntity(_label, _idSuffix, _value);
        });

        _editor.RegisterInspectorFieldDrawer<Canis::SceneAssetHandle>([](Editor& _editor, const char* _label, const char* _idSuffix, Canis::SceneAssetHandle& _value)
        {
            _editor.InputSceneAsset(_label, _idSuffix, _value);
        });

        ScriptConf canvasConf = {
            .name = "Canis::Canvas",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                _entity.AddComponent<Canvas>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Canvas>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Canvas>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Canvas>() ? (void*)(&_entity.GetComponent<Canvas>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Canvas>())
                {
                    Canvas& canvas = _entity.GetComponent<Canvas>();

                    YAML::Node comp;
                    comp["active"] = canvas.active;
                    comp["renderMode"] = canvas.renderMode;
                    _node["Canis::Canvas"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto canvasNode = _node["Canis::Canvas"])
                {
                    auto &canvas = *_entity.AddComponent<Canvas>();
                    canvas.active = canvasNode["active"].as<bool>(true);
                    canvas.renderMode = canvasNode["renderMode"].as<unsigned int>(CanvasRenderMode::SCREEN_SPACE_OVERLAY);

                    if (_callCreate)
                        canvas.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Canvas* canvas = nullptr;
                if (_entity.HasComponent<Canvas>() && ((canvas = &_entity.GetComponent<Canvas>()), true))
                {
                    ImGui::Checkbox("active", &canvas->active);

                    int renderMode = static_cast<int>(canvas->renderMode);
                    if (ImGui::Combo("renderMode", &renderMode, CanvasRenderModeLabels, IM_ARRAYSIZE(CanvasRenderModeLabels)))
                        canvas->renderMode = static_cast<unsigned int>(renderMode);
                }
            },
        };

        RegisterScript(canvasConf);

        ScriptConf rectTransformConf = {
            .name = "Canis::RectTransform",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                _entity.AddComponent<RectTransform>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<RectTransform>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<RectTransform>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<RectTransform>() ? (void*)(&_entity.GetComponent<RectTransform>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<RectTransform>())
                {
                    RectTransform& transform = _entity.GetComponent<RectTransform>();

                    YAML::Node comp;
                    comp["active"] = transform.active;
                    comp["position"] = transform.position;
                    comp["size"] = transform.size;
                    comp["scale"] = transform.scale;
                    comp["anchorMin"] = transform.anchorMin;
                    comp["anchorMax"] = transform.anchorMax;
                    comp["pivot"] = transform.pivot;
                    comp["originOffset"] = transform.originOffset;
                    comp["depth"] = transform.depth;
                    comp["rotation"] = transform.rotation;
                    comp["rotationOriginOffset"] = transform.rotationOriginOffset;
                    comp["parent"] = (transform.parent == nullptr) ? Canis::UUID(0) : transform.parent->uuid;
                    // children
                    YAML::Node children = YAML::Node(YAML::NodeType::Sequence);

                    for (Canis::Entity* c : transform.children)
                    {
                        children.push_back(c->uuid);
                    }

                    comp["children"] = children;

                    _node["Canis::RectTransform"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto rectTransform = _node["Canis::RectTransform"])
                {
                    auto &rt = *_entity.AddComponent<RectTransform>();
                    rt.active = rectTransform["active"].as<bool>(true);
                    rt.position = rectTransform["position"].as<Vector2>(rt.position);
                    rt.size = rectTransform["size"].as<Vector2>(rt.size);
                    rt.scale = rectTransform["scale"].as<Vector2>(rt.scale);
                    rt.anchorMin = rectTransform["anchorMin"].as<Vector2>(rt.anchorMin);
                    rt.anchorMax = rectTransform["anchorMax"].as<Vector2>(rt.anchorMax);
                    rt.pivot = rectTransform["pivot"].as<Vector2>(rt.pivot);
                    rt.originOffset = rectTransform["originOffset"].as<Vector2>(rt.originOffset);
                    rt.depth = rectTransform["depth"].as<float>(rt.depth);
                    rt.rotation = rectTransform["rotation"].as<float>(rt.rotation);
                    rt.rotationOriginOffset = rectTransform["rotationOriginOffset"].as<Vector2>(rt.rotationOriginOffset);

                    if (rectTransform["parent"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(rectTransform["parent"].as<Canis::UUID>(0), rt.parent);
                    
                    if (auto children = rectTransform["children"]; children && children.IsSequence())
                    {
                        const std::size_t count = children.size();
                        rt.children.clear();
                        rt.children.resize(count);

                        std::size_t i = 0;
                        for (const auto &e : children)
                        {
                            auto uuid = e.as<Canis::UUID>(Canis::UUID(0));
                            _entity.scene.GetEntityAfterLoad(uuid, rt.children[i++]);
                        }
                    }
                    
                        //rt.scaleWithScreen = (ScaleWithScreen)rectTransform["scaleWithScreen"].as<int>(0);
                    if (_callCreate)
                        rt.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                RectTransform* transform = nullptr;
                if (_entity.HasComponent<RectTransform>() && ((transform = &_entity.GetComponent<RectTransform>()), true))
                {
                    ImGui::Checkbox("active", &transform->active);
                    ImGui::InputFloat2("position", &transform->position.x, "%.3f");
                    ImGui::InputFloat2("size", &transform->size.x, "%.3f");
                    ImGui::InputFloat2("scale", &transform->scale.x, "%.3f");
                    int anchorPreset = transform->GetAnchorPreset();
                    int anchorPresetSelection = anchorPreset < 0 ? 0 : anchorPreset + 1;
                    static const char* anchorPresetLabels[] = {
                        "Custom",
                        "Top Left", "Top Center", "Top Right",
                        "Center Left", "Center", "Center Right",
                        "Bottom Left", "Bottom Center", "Bottom Right"
                    };
                    if (ImGui::Combo("anchorPreset", &anchorPresetSelection, anchorPresetLabels, IM_ARRAYSIZE(anchorPresetLabels)) && anchorPresetSelection > 0)
                        transform->SetAnchorPreset(static_cast<RectAnchor>(anchorPresetSelection - 1));

                    ImGui::InputFloat2("anchorMin", &transform->anchorMin.x, "%.3f");
                    ImGui::InputFloat2("anchorMax", &transform->anchorMax.x, "%.3f");
                    ImGui::InputFloat2("pivot", &transform->pivot.x, "%.3f");
                    ImGui::InputFloat2("originOffset", &transform->originOffset.x, "%.3f");
                    ImGui::InputFloat2("rotationOriginOffset", &transform->rotationOriginOffset.x, "%.3f");
                    ImGui::InputFloat("depth", &transform->depth);
                    // let user work with degrees
                    float degrees = RAD2DEG * transform->rotation;
                    ImGui::InputFloat("rotation", &degrees);
                    transform->rotation = DEG2RAD * degrees;

                    auto clamp01 = [](float value) -> float
                    {
                        return std::clamp(value, 0.0f, 1.0f);
                    };

                    transform->anchorMin.x = clamp01(transform->anchorMin.x);
                    transform->anchorMin.y = clamp01(transform->anchorMin.y);
                    transform->anchorMax.x = std::clamp(transform->anchorMax.x, transform->anchorMin.x, 1.0f);
                    transform->anchorMax.y = std::clamp(transform->anchorMax.y, transform->anchorMin.y, 1.0f);
                    transform->pivot.x = clamp01(transform->pivot.x);
                    transform->pivot.y = clamp01(transform->pivot.y);
                }
            },
        };

        RegisterScript(rectTransformConf);

        ScriptConf sprite2DConf = {
            .name = "Canis::Sprite2D",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                // TODO: require a RectTransform component
                Sprite2D& sprite = *_entity.AddComponent<Sprite2D>();
                sprite.textureHandle = Canis::AssetManager::GetTextureHandle("assets/defaults/textures/square.png");
                //sprite->size.x = sprite->textureHandle.texture.width;
                //sprite->size.y = sprite->textureHandle.texture.height;
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Sprite2D>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Sprite2D>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Sprite2D>() ? (void*)(&_entity.GetComponent<Sprite2D>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Sprite2D>())
                {
                    Sprite2D& sprite = _entity.GetComponent<Sprite2D>();

                    YAML::Node comp;
                    comp["color"] = sprite.color;
                    comp["uv"] = sprite.uv;
                    comp["flipX"] = sprite.flipX;
                    comp["flipY"] = sprite.flipY;

                    YAML::Node textureAsset;
                    const std::string texturePath = AssetManager::GetPath(sprite.textureHandle.id);
                    if (texturePath != "Path was not found in AssetLibrary")
                    {
                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(texturePath))
                            textureAsset["uuid"] = (uint64_t)meta->uuid;
                    }
                    
                    comp["TextureAsset"] = textureAsset;
                    _node["Canis::Sprite2D"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::Sprite2D"])
                {
                    auto &sprite = *_entity.AddComponent<Sprite2D>();
                    sprite.color = comp["color"].as<Vector4>();
                    sprite.uv = comp["uv"].as<Vector4>();
                    sprite.flipX = comp["flipX"].as<bool>(false);
                    sprite.flipY = comp["flipY"].as<bool>(false);
                    sprite.textureHandle = AssetManager::GetTextureHandle("assets/defaults/textures/square.png");
                    if (auto textureAsset = comp["TextureAsset"])
                    {
                        std::string path = "";

                        if (YAML::Node uuidNode = textureAsset["uuid"])
                        {
                            const UUID uuid = uuidNode.as<uint64_t>(0);
                            if ((uint64_t)uuid != 0)
                            {
                                path = AssetManager::GetPath(uuid);
                                if (path == "Path was not found in AssetLibrary")
                                    path.clear();
                            }
                        }

                        if (path.empty())
                            path = textureAsset["path"].as<std::string>("");

                        if (!path.empty())
                            sprite.textureHandle = AssetManager::GetTextureHandle(path);
                    }
                    //sprite.textureHandle = sprite2DComponent["TextureHandle"].as<TextureHandle>();//AssetManager::GetTextureHandle(sprite2DComponent["textureHandle"].as<std::string>());
                    if (_callCreate)
                        sprite.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Sprite2D* sprite = nullptr;
                if (_entity.HasComponent<Sprite2D>() && ((sprite = &_entity.GetComponent<Sprite2D>()), true))
                {
                    // textureHandle
                    ImGui::ColorEdit4("color", &sprite->color.r);
                    ImGui::InputFloat4("uv", &sprite->uv.x, "%.3f");

                    bool updateUV = false;
                    
                    if (ImGui::Checkbox("flipX", &sprite->flipX))
                        updateUV = true;
                    if (ImGui::Checkbox("flipY", &sprite->flipY))
                        updateUV = true;
                    
                    if (updateUV)
                    {
                        if (SpriteAnimation* animation = _entity.HasComponent<SpriteAnimation>() ? &_entity.GetComponent<SpriteAnimation>() : nullptr)
                        {
                            if (SpriteAnimationAsset* animationAsset = AssetManager::Get<SpriteAnimationAsset>(animation->id))
                            {
                                sprite->GetSpriteFromTextureAtlas(
                                    animationAsset->frames[animation->index].offsetX,
                                    animationAsset->frames[animation->index].offsetY,
                                    animationAsset->frames[animation->index].row,
                                    animationAsset->frames[animation->index].col,
                                    animationAsset->frames[animation->index].width,
                                    animationAsset->frames[animation->index].height);
                            }
                        }
                        else
                        {
                            sprite->GetSpriteFromTextureAtlas(0, 0, 0, 0, sprite->textureHandle.texture.width, sprite->textureHandle.texture.height);
                        }
                    }

                    ImGui::Text("texture");

                    ImGui::SameLine();

                    const std::string texturePath = AssetManager::GetPath(sprite->textureHandle.id);
                    std::string textureLabel = "[missing texture]";
                    if (texturePath != "Path was not found in AssetLibrary")
                    {
                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(texturePath))
                            textureLabel = meta->name;
                        else
                            textureLabel = texturePath;
                    }

                    ImGui::Button(textureLabel.c_str(), ImVec2(150, 0));

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                        {
                            const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                            std::string path = AssetManager::GetPath(dropped.uuid);
                            TextureAsset* asset = AssetManager::GetTexture(path);

                            if (asset)
                            {
                                sprite->textureHandle = AssetManager::GetTextureHandle(path);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            },
        };

        RegisterScript(sprite2DConf);

        ScriptConf textConf = {
            .name = "Canis::Text",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {

                _entity.AddComponent<RectTransform>();

                Text& text = *_entity.AddComponent<Text>();
                text.assetId = AssetManager::LoadText("assets/fonts/Antonio-Bold.ttf", 32);
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Text>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Text>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Text>() ? (void*)(&_entity.GetComponent<Text>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Text>())
                {
                    Text& text = _entity.GetComponent<Text>();
                    YAML::Node comp;
                    comp["text"] = text.text;
                    comp["color"] = text.color;
                    comp["alignment"] = text.alignment;
                    comp["horizontalBoundary"] = text.horizontalBoundary;

                    if (text.assetId > -1)
                    {
                        if (TextAsset* textAsset = AssetManager::GetText(text.assetId))
                        {
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(textAsset->GetPath()))
                            {
                                YAML::Node fontAsset;
                                fontAsset["uuid"] = (uint64_t)meta->uuid;
                                fontAsset["fontSize"] = textAsset->GetFontSize();
                                comp["FontAsset"] = fontAsset;
                            }
                        }
                    }

                    _node["Canis::Text"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::Text"])
                {
                    Text& text = *_entity.AddComponent<Text>();
                    text.text = comp["text"].as<std::string>("");
                    text.color = comp["color"].as<Vector4>(Color(1.0f));
                    text.alignment = comp["alignment"].as<unsigned int>(Canis::TextAlignment::LEFT);
                    text.horizontalBoundary = comp["horizontalBoundary"].as<unsigned int>(Canis::TextBoundary::TB_OVERFLOW);
                    text._status = BIT::ONE;

                    if (auto fontAsset = comp["FontAsset"])
                    {
                        UUID uuid = fontAsset["uuid"].as<uint64_t>();
                        std::string path = AssetManager::GetPath(uuid);
                        const unsigned int fontSize = fontAsset["fontSize"].as<unsigned int>(32u);
                        text.assetId = AssetManager::LoadText(path, fontSize);
                    }

                    if (_callCreate)
                        text.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Text* text = nullptr;
                if (_entity.HasComponent<Text>() && ((text = &_entity.GetComponent<Text>()), true))
                {
                    static const char *alignmentLabels[] = {"Left", "Right", "Center"};
                    static const char *horizontalBoundaryLabels[] = {"Overflow", "Wrap"};

                    if (ImGui::InputText("text", &text->text))
                    {
                        text->_status |= BIT::ONE;
                    }

                    ImGui::ColorEdit4("color", &text->color.r);

                    int alignment = (int)text->alignment;
                    if (ImGui::Combo("alignment", &alignment, alignmentLabels, IM_ARRAYSIZE(alignmentLabels)))
                    {
                        text->alignment = (unsigned int)alignment;
                        text->_status |= BIT::ONE;
                    }

                    int boundary = (int)text->horizontalBoundary;
                    if (ImGui::Combo("horizontalBoundary", &boundary, horizontalBoundaryLabels, IM_ARRAYSIZE(horizontalBoundaryLabels)))
                    {
                        text->horizontalBoundary = (unsigned int)boundary;
                        text->_status |= BIT::ONE;
                    }

                    if (text->assetId > -1)
                    {
                        ImGui::Text("font");
                        ImGui::SameLine();
                        TextAsset* textAsset = AssetManager::GetText(text->assetId);

                        if (textAsset != nullptr)
                        {
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(textAsset->GetPath()))
                            {
                                ImGui::Button(meta->name.c_str(), ImVec2(150, 0));
                            }
                            else
                            {
                                ImGui::Button("Missing Font", ImVec2(150, 0));
                            }
                        }

                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                            {
                                const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                                std::string path = AssetManager::GetPath(dropped.uuid);
                                const unsigned int fontSize = (textAsset == nullptr) ? 32u : textAsset->GetFontSize();
                                text->assetId = AssetManager::LoadText(path, fontSize);
                                text->_status |= BIT::ONE;
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                }
            },
        };

        RegisterScript(textConf);

        ScriptConf buttonConf = {
            .name = "Canis::UIButton",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                _entity.AddComponent<UIButton>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<UIButton>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<UIButton>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<UIButton>() ? (void*)(&_entity.GetComponent<UIButton>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (UIButton* button = _entity.HasComponent<UIButton>() ? &_entity.GetComponent<UIButton>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = button->active;
                    comp["targetEntity"] = (button->targetEntity == nullptr) ? Canis::UUID(0) : button->targetEntity->uuid;
                    comp["targetScript"] = button->targetScript;
                    comp["actionName"] = button->actionName;
                    comp["baseColor"] = button->baseColor;
                    comp["hoverColor"] = button->hoverColor;
                    comp["pressedColor"] = button->pressedColor;
                    comp["baseScale"] = button->baseScale;
                    comp["hoverScale"] = button->hoverScale;
                    comp["pressedScale"] = button->pressedScale;
                    _node["Canis::UIButton"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (YAML::Node comp = _node["Canis::UIButton"])
                {
                    UIButton& button = *_entity.AddComponent<UIButton>();
                    button.active = comp["active"].as<bool>(true);
                    button.targetScript = comp["targetScript"].as<std::string>("");
                    button.actionName = comp["actionName"].as<std::string>("");
                    button.baseColor = comp["baseColor"].as<Vector4>(Color(1.0f));
                    button.hoverColor = comp["hoverColor"].as<Vector4>(Color(1.0f));
                    button.pressedColor = comp["pressedColor"].as<Vector4>(Color(0.85f, 0.85f, 0.85f, 1.0f));
                    button.baseScale = comp["baseScale"].as<float>(1.0f);
                    button.hoverScale = comp["hoverScale"].as<float>(1.03f);
                    button.pressedScale = comp["pressedScale"].as<float>(0.98f);

                    if (comp["targetEntity"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(comp["targetEntity"].as<Canis::UUID>(0), button.targetEntity);

                    if (_callCreate)
                        button.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                UIButton* button = _entity.HasComponent<UIButton>() ? &_entity.GetComponent<UIButton>() : nullptr;
                if (button == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), button->active);
                DrawInspectorField(_editor, "targetEntity", _conf.name.c_str(), button->targetEntity);
#if CANIS_EDITOR
                DrawConnectedUIActionSelector(*this, button->targetEntity, button->targetScript, button->actionName, _conf.name.c_str());
#else
                DrawInspectorField(_editor, "targetScript", _conf.name.c_str(), button->targetScript);
                DrawInspectorField(_editor, "actionName", _conf.name.c_str(), button->actionName);
#endif
                DrawInspectorField(_editor, "baseColor", _conf.name.c_str(), button->baseColor);
                DrawInspectorField(_editor, "hoverColor", _conf.name.c_str(), button->hoverColor);
                DrawInspectorField(_editor, "pressedColor", _conf.name.c_str(), button->pressedColor);
                DrawInspectorField(_editor, "baseScale", _conf.name.c_str(), button->baseScale);
                DrawInspectorField(_editor, "hoverScale", _conf.name.c_str(), button->hoverScale);
                DrawInspectorField(_editor, "pressedScale", _conf.name.c_str(), button->pressedScale);
            },
        };

        RegisterScript(buttonConf);

        ScriptConf dragSourceConf = {
            .name = "Canis::UIDragSource",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                _entity.AddComponent<UIDragSource>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<UIDragSource>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<UIDragSource>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<UIDragSource>() ? (void*)(&_entity.GetComponent<UIDragSource>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (UIDragSource* dragSource = _entity.HasComponent<UIDragSource>() ? &_entity.GetComponent<UIDragSource>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = dragSource->active;
                    comp["payloadType"] = dragSource->payloadType;
                    comp["payloadValue"] = dragSource->payloadValue;
                    _node["Canis::UIDragSource"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (YAML::Node comp = _node["Canis::UIDragSource"])
                {
                    UIDragSource& dragSource = *_entity.AddComponent<UIDragSource>();
                    dragSource.active = comp["active"].as<bool>(true);
                    dragSource.payloadType = comp["payloadType"].as<std::string>("");
                    dragSource.payloadValue = comp["payloadValue"].as<std::string>("");

                    if (_callCreate)
                        dragSource.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                UIDragSource* dragSource = _entity.HasComponent<UIDragSource>() ? &_entity.GetComponent<UIDragSource>() : nullptr;
                if (dragSource == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), dragSource->active);
                DrawInspectorField(_editor, "payloadType", _conf.name.c_str(), dragSource->payloadType);
                DrawInspectorField(_editor, "payloadValue", _conf.name.c_str(), dragSource->payloadValue);
            },
        };

        RegisterScript(dragSourceConf);

        ScriptConf dropTargetConf = {
            .name = "Canis::UIDropTarget",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<RectTransform>())
                    _entity.AddComponent<RectTransform>();
                _entity.AddComponent<UIDropTarget>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<UIDropTarget>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<UIDropTarget>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<UIDropTarget>() ? (void*)(&_entity.GetComponent<UIDropTarget>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (UIDropTarget* dropTarget = _entity.HasComponent<UIDropTarget>() ? &_entity.GetComponent<UIDropTarget>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = dropTarget->active;
                    comp["targetEntity"] = (dropTarget->targetEntity == nullptr) ? Canis::UUID(0) : dropTarget->targetEntity->uuid;
                    comp["targetScript"] = dropTarget->targetScript;
                    comp["actionName"] = dropTarget->actionName;
                    comp["acceptedPayloadType"] = dropTarget->acceptedPayloadType;
                    comp["baseColor"] = dropTarget->baseColor;
                    comp["hoverColor"] = dropTarget->hoverColor;
                    _node["Canis::UIDropTarget"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (YAML::Node comp = _node["Canis::UIDropTarget"])
                {
                    UIDropTarget& dropTarget = *_entity.AddComponent<UIDropTarget>();
                    dropTarget.active = comp["active"].as<bool>(true);
                    dropTarget.targetScript = comp["targetScript"].as<std::string>("");
                    dropTarget.actionName = comp["actionName"].as<std::string>("");
                    dropTarget.acceptedPayloadType = comp["acceptedPayloadType"].as<std::string>("");
                    dropTarget.baseColor = comp["baseColor"].as<Vector4>(Color(1.0f));
                    dropTarget.hoverColor = comp["hoverColor"].as<Vector4>(Color(1.0f));

                    if (comp["targetEntity"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(comp["targetEntity"].as<Canis::UUID>(0), dropTarget.targetEntity);

                    if (_callCreate)
                        dropTarget.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                UIDropTarget* dropTarget = _entity.HasComponent<UIDropTarget>() ? &_entity.GetComponent<UIDropTarget>() : nullptr;
                if (dropTarget == nullptr)
                    return;

                DrawInspectorField(_editor, "active", _conf.name.c_str(), dropTarget->active);
                DrawInspectorField(_editor, "targetEntity", _conf.name.c_str(), dropTarget->targetEntity);
#if CANIS_EDITOR
                DrawConnectedUIActionSelector(*this, dropTarget->targetEntity, dropTarget->targetScript, dropTarget->actionName, _conf.name.c_str());
#else
                DrawInspectorField(_editor, "targetScript", _conf.name.c_str(), dropTarget->targetScript);
                DrawInspectorField(_editor, "actionName", _conf.name.c_str(), dropTarget->actionName);
#endif
                DrawInspectorField(_editor, "acceptedPayloadType", _conf.name.c_str(), dropTarget->acceptedPayloadType);
                DrawInspectorField(_editor, "baseColor", _conf.name.c_str(), dropTarget->baseColor);
                DrawInspectorField(_editor, "hoverColor", _conf.name.c_str(), dropTarget->hoverColor);
            },
        };

        RegisterScript(dropTargetConf);

        ScriptConf camera2DConf = {
            .name = "Canis::Camera2D",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                Camera2D& camera = *_entity.AddComponent<Camera2D>();
                camera.Create();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Camera2D>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Camera2D>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Camera2D>() ? (void*)(&_entity.GetComponent<Camera2D>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Camera2D>())
                {
                    Camera2D& camera = _entity.GetComponent<Camera2D>();

                    YAML::Node comp;
                    comp["position"] = camera.GetPosition();
                    comp["scale"] = camera.GetScale();

                    _node["Canis::Camera2D"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto camera2DComponent = _node["Canis::Camera2D"])
                {
                    Camera2D& camera = *_entity.AddComponent<Camera2D>();
                    camera.SetPosition(camera2DComponent["position"].as<Vector2>(camera.GetPosition()));
                    camera.SetScale(camera2DComponent["scale"].as<float>(camera.GetScale()));
                    if (_callCreate)
                        camera.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Camera2D* camera = nullptr;
                if (_entity.HasComponent<Camera2D>() && ((camera = &_entity.GetComponent<Camera2D>()), true))
                {
                    Vector2 lastPosition = camera->GetPosition();
                    float lastScale = camera->GetScale();

                    ImGui::InputFloat2(("position##" + _conf.name).c_str(), &lastPosition.x, "%.3f");
                    ImGui::InputFloat(("scale##" + _conf.name).c_str(), &lastScale);

                    if (lastPosition != camera->GetPosition())
                        camera->SetPosition(lastPosition);
                    
                    if (lastScale != camera->GetScale())
                        camera->SetScale(lastScale);
                }
            },
        };

        RegisterScript(camera2DConf);

        ScriptConf transformConf = {
            .name = "Canis::Transform",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void { _entity.AddComponent<Transform>(); },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Transform>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Transform>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Transform>() ? (void*)(&_entity.GetComponent<Transform>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Transform>())
                {
                    Transform& transform = _entity.GetComponent<Transform>();
                    YAML::Node comp;
                    comp["active"] = transform.active;
                    comp["position"] = transform.position;
                    comp["rotation"] = transform.rotation;
                    comp["scale"] = transform.scale;
                    comp["parent"] = (transform.parent == nullptr) ? Canis::UUID(0) : transform.parent->uuid;

                    YAML::Node children = YAML::Node(YAML::NodeType::Sequence);
                    for (Canis::Entity* child : transform.children)
                    {
                        children.push_back(child ? child->uuid : Canis::UUID(0));
                    }
                    comp["children"] = children;

                    _node["Canis::Transform"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Transform"];
                if (!comp)
                    comp = _node["Canis::Transform"];

                if (comp)
                {
                    auto &transform = *_entity.AddComponent<Transform>();
                    transform.active = comp["active"].as<bool>(true);
                    transform.position = comp["position"].as<Vector3>(Vector3(0.0f));
                    transform.rotation = comp["rotation"].as<Vector3>(Vector3(0.0f));
                    transform.scale = comp["scale"].as<Vector3>(Vector3(1.0f));

                    if (comp["parent"].as<Canis::UUID>(0) != Canis::UUID(0))
                        _entity.scene.GetEntityAfterLoad(comp["parent"].as<Canis::UUID>(0), transform.parent);

                    if (auto children = comp["children"]; children && children.IsSequence())
                    {
                        const std::size_t count = children.size();
                        transform.children.clear();
                        transform.children.resize(count);

                        std::size_t i = 0;
                        for (const auto &entry : children)
                        {
                            auto uuid = entry.as<Canis::UUID>(Canis::UUID(0));
                            _entity.scene.GetEntityAfterLoad(uuid, transform.children[i++]);
                        }
                    }

                    if (_callCreate)
                        transform.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Transform* transform = nullptr;
                if (_entity.HasComponent<Transform>() && ((transform = &_entity.GetComponent<Transform>()), true))
                {
                    ImGui::InputFloat3("position", &transform->position.x, "%.3f");

                    Vector3 degrees = transform->rotation * RAD2DEG;
                    if (ImGui::InputFloat3("rotation", &degrees.x, "%.3f"))
                    {
                        transform->rotation = degrees * DEG2RAD;
                    }

                    ImGui::InputFloat3("scale", &transform->scale.x, "%.3f");

                    if (transform->parent != nullptr)
                    {
                        ImGui::Text("parent: %s", transform->parent->name.c_str());
                        if (ImGui::Button("Unparent##Transform"))
                            transform->Unparent();
                    }
                    else
                    {
                        ImGui::Text("parent: [none]");
                    }
                }
            },
        };

        RegisterScript(transformConf);

        ScriptConf rigidbodyConf = {
            .name = "Canis::Rigidbody",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                _entity.AddComponent<Transform>();

                if (!_entity.HasComponent<BoxCollider>()
                    && !_entity.HasComponent<SphereCollider>()
                    && !_entity.HasComponent<CapsuleCollider>()
                    && !_entity.HasComponent<MeshCollider>())
                {
                    _entity.AddComponent<BoxCollider>();
                }

                _entity.AddComponent<Rigidbody>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<Rigidbody>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<Rigidbody>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<Rigidbody>() ? (void*)(&_entity.GetComponent<Rigidbody>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (Rigidbody *rigidbody = _entity.HasComponent<Rigidbody>() ? &_entity.GetComponent<Rigidbody>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = rigidbody->active;
                    comp["motionType"] = rigidbody->motionType;
                    comp["mass"] = rigidbody->mass;
                    comp["friction"] = rigidbody->friction;
                    comp["restitution"] = rigidbody->restitution;
                    comp["linearDamping"] = rigidbody->linearDamping;
                    comp["angularDamping"] = rigidbody->angularDamping;
                    comp["useGravity"] = rigidbody->useGravity;
                    comp["isSensor"] = rigidbody->isSensor;
                    comp["layer"] = rigidbody->layer;
                    comp["mask"] = rigidbody->mask;
                    comp["allowSleeping"] = rigidbody->allowSleeping;
                    comp["lockRotationX"] = rigidbody->lockRotationX;
                    comp["lockRotationY"] = rigidbody->lockRotationY;
                    comp["lockRotationZ"] = rigidbody->lockRotationZ;
                    comp["linearVelocity"] = rigidbody->linearVelocity;
                    comp["angularVelocity"] = rigidbody->angularVelocity;
                    _node["Canis::Rigidbody"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Rigidbody"];
                if (!comp)
                    comp = _node["Canis::Rigidbody"];

                if (comp)
                {
                    auto &rigidbody = *_entity.AddComponent<Rigidbody>();
                    rigidbody.active = comp["active"].as<bool>(true);
                    rigidbody.motionType = comp["motionType"].as<int>(RigidbodyMotionType::DYNAMIC);
                    rigidbody.mass = comp["mass"].as<float>(1.0f);
                    rigidbody.friction = comp["friction"].as<float>(0.2f);
                    rigidbody.restitution = comp["restitution"].as<float>(0.0f);
                    rigidbody.linearDamping = comp["linearDamping"].as<float>(0.05f);
                    rigidbody.angularDamping = comp["angularDamping"].as<float>(0.05f);
                    rigidbody.useGravity = comp["useGravity"].as<bool>(true);
                    rigidbody.isSensor = comp["isSensor"].as<bool>(false);
                    rigidbody.layer = comp["layer"].as<Mask>(
                        comp["collisionLayer"].as<Mask>(
                            comp["raycastMask"].as<Mask>(Rigidbody::DefaultLayer)));
                    rigidbody.mask = comp["mask"].as<Mask>(
                        comp["collisionMask"].as<Mask>(Rigidbody::DefaultMask));
                    rigidbody.allowSleeping = comp["allowSleeping"].as<bool>(true);
                    rigidbody.lockRotationX = comp["lockRotationX"].as<bool>(false);
                    rigidbody.lockRotationY = comp["lockRotationY"].as<bool>(false);
                    rigidbody.lockRotationZ = comp["lockRotationZ"].as<bool>(false);
                    rigidbody.linearVelocity = comp["linearVelocity"].as<Vector3>(Vector3(0.0f));
                    rigidbody.angularVelocity = comp["angularVelocity"].as<Vector3>(Vector3(0.0f));
                    if (_callCreate)
                        rigidbody.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                Rigidbody *rigidbody = _entity.HasComponent<Rigidbody>() ? &_entity.GetComponent<Rigidbody>() : nullptr;
                if (rigidbody == nullptr)
                    return;

                const char *motionTypeLabels[] = {"Static", "Kinematic", "Dynamic"};
                if (rigidbody->motionType < RigidbodyMotionType::STATIC
                    || rigidbody->motionType > RigidbodyMotionType::DYNAMIC)
                {
                    rigidbody->motionType = RigidbodyMotionType::DYNAMIC;
                }

                ImGui::Checkbox(("active##" + _conf.name).c_str(), &rigidbody->active);
                ImGui::Combo(("motionType##" + _conf.name).c_str(), &rigidbody->motionType, motionTypeLabels, IM_ARRAYSIZE(motionTypeLabels));
                ImGui::InputFloat(("mass##" + _conf.name).c_str(), &rigidbody->mass);
                ImGui::InputFloat(("friction##" + _conf.name).c_str(), &rigidbody->friction);
                ImGui::InputFloat(("restitution##" + _conf.name).c_str(), &rigidbody->restitution);
                ImGui::InputFloat(("linearDamping##" + _conf.name).c_str(), &rigidbody->linearDamping);
                ImGui::InputFloat(("angularDamping##" + _conf.name).c_str(), &rigidbody->angularDamping);
                ImGui::Checkbox(("useGravity##" + _conf.name).c_str(), &rigidbody->useGravity);
                ImGui::Checkbox(("isSensor##" + _conf.name).c_str(), &rigidbody->isSensor);
                DrawInspectorField("layer", _conf.name.c_str(), rigidbody->layer);
                DrawInspectorField("mask", _conf.name.c_str(), rigidbody->mask);
                ImGui::Checkbox(("allowSleeping##" + _conf.name).c_str(), &rigidbody->allowSleeping);
                ImGui::Checkbox(("lockRotationX##" + _conf.name).c_str(), &rigidbody->lockRotationX);
                ImGui::Checkbox(("lockRotationY##" + _conf.name).c_str(), &rigidbody->lockRotationY);
                ImGui::Checkbox(("lockRotationZ##" + _conf.name).c_str(), &rigidbody->lockRotationZ);
                ImGui::InputFloat3(("linearVelocity##" + _conf.name).c_str(), &rigidbody->linearVelocity.x, "%.3f");
                ImGui::InputFloat3(("angularVelocity##" + _conf.name).c_str(), &rigidbody->angularVelocity.x, "%.3f");
            },
        };

        RegisterScript(rigidbodyConf);

        ScriptConf boxColliderConf = {
            .name = "Canis::BoxCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                _entity.AddComponent<Transform>();

                _entity.RemoveComponent<SphereCollider>();
                _entity.RemoveComponent<CapsuleCollider>();
                _entity.RemoveComponent<MeshCollider>();
                _entity.AddComponent<BoxCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<BoxCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<BoxCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<BoxCollider>() ? (void*)(&_entity.GetComponent<BoxCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (BoxCollider *boxCollider = _entity.HasComponent<BoxCollider>() ? &_entity.GetComponent<BoxCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = boxCollider->active;
                    comp["size"] = boxCollider->size;
                    _node["Canis::BoxCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::BoxCollider"];
                if (!comp)
                    comp = _node["Canis::BoxCollider"];

                if (comp)
                {
                    auto &boxCollider = *_entity.AddComponent<BoxCollider>();
                    boxCollider.active = comp["active"].as<bool>(true);
                    boxCollider.size = comp["size"].as<Vector3>(Vector3(1.0f));
                    if (_callCreate)
                        boxCollider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                BoxCollider *boxCollider = _entity.HasComponent<BoxCollider>() ? &_entity.GetComponent<BoxCollider>() : nullptr;
                if (boxCollider == nullptr)
                    return;

                ImGui::Checkbox(("active##" + _conf.name).c_str(), &boxCollider->active);
                ImGui::InputFloat3(("size##" + _conf.name).c_str(), &boxCollider->size.x, "%.3f");
            },
        };

        RegisterScript(boxColliderConf);

        ScriptConf sphereColliderConf = {
            .name = "Canis::SphereCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.RemoveComponent<BoxCollider>();
                _entity.RemoveComponent<CapsuleCollider>();
                _entity.RemoveComponent<MeshCollider>();
                _entity.AddComponent<SphereCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<SphereCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<SphereCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<SphereCollider>() ? (void*)(&_entity.GetComponent<SphereCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (SphereCollider *sphereCollider = _entity.HasComponent<SphereCollider>() ? &_entity.GetComponent<SphereCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = sphereCollider->active;
                    comp["radius"] = sphereCollider->radius;
                    _node["Canis::SphereCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::SphereCollider"];
                if (!comp)
                    comp = _node["Canis::SphereCollider"];

                if (comp)
                {
                    auto &sphereCollider = *_entity.AddComponent<SphereCollider>();
                    sphereCollider.active = comp["active"].as<bool>(true);
                    sphereCollider.radius = comp["radius"].as<float>(0.5f);
                    if (_callCreate)
                        sphereCollider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                SphereCollider *sphereCollider = _entity.HasComponent<SphereCollider>() ? &_entity.GetComponent<SphereCollider>() : nullptr;
                if (sphereCollider == nullptr)
                    return;

                ImGui::Checkbox(("active##" + _conf.name).c_str(), &sphereCollider->active);
                ImGui::InputFloat(("radius##" + _conf.name).c_str(), &sphereCollider->radius);
            },
        };

        RegisterScript(sphereColliderConf);

        ScriptConf capsuleColliderConf = {
            .name = "Canis::CapsuleCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.RemoveComponent<BoxCollider>();
                _entity.RemoveComponent<SphereCollider>();
                _entity.RemoveComponent<MeshCollider>();
                _entity.AddComponent<CapsuleCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<CapsuleCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<CapsuleCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<CapsuleCollider>() ? (void*)(&_entity.GetComponent<CapsuleCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (CapsuleCollider *capsuleCollider = _entity.HasComponent<CapsuleCollider>() ? &_entity.GetComponent<CapsuleCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = capsuleCollider->active;
                    comp["halfHeight"] = capsuleCollider->halfHeight;
                    comp["radius"] = capsuleCollider->radius;
                    _node["Canis::CapsuleCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::CapsuleCollider"];
                if (!comp)
                    comp = _node["Canis::CapsuleCollider"];

                if (comp)
                {
                    auto &capsuleCollider = *_entity.AddComponent<CapsuleCollider>();
                    capsuleCollider.active = comp["active"].as<bool>(true);
                    capsuleCollider.halfHeight = comp["halfHeight"].as<float>(0.5f);
                    capsuleCollider.radius = comp["radius"].as<float>(0.25f);
                    if (_callCreate)
                        capsuleCollider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                CapsuleCollider *capsuleCollider = _entity.HasComponent<CapsuleCollider>() ? &_entity.GetComponent<CapsuleCollider>() : nullptr;
                if (capsuleCollider == nullptr)
                    return;

                ImGui::Checkbox(("active##" + _conf.name).c_str(), &capsuleCollider->active);
                ImGui::InputFloat(("halfHeight##" + _conf.name).c_str(), &capsuleCollider->halfHeight);
                ImGui::InputFloat(("radius##" + _conf.name).c_str(), &capsuleCollider->radius);
            },
        };

        RegisterScript(capsuleColliderConf);

        ScriptConf meshColliderConf = {
            .name = "Canis::MeshCollider",
            .Construct = nullptr,
            .Add = [this](Entity &_entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.RemoveComponent<BoxCollider>();
                _entity.RemoveComponent<SphereCollider>();
                _entity.RemoveComponent<CapsuleCollider>();
                _entity.AddComponent<MeshCollider>();
            },
            .Has = [this](Entity &_entity) -> bool { return _entity.HasComponent<MeshCollider>(); },
            .Remove = [this](Entity &_entity) -> void { _entity.RemoveComponent<MeshCollider>(); },
            .Get = [this](Entity &_entity) -> void* { return _entity.HasComponent<MeshCollider>() ? (void*)(&_entity.GetComponent<MeshCollider>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (MeshCollider *meshCollider = _entity.HasComponent<MeshCollider>() ? &_entity.GetComponent<MeshCollider>() : nullptr)
                {
                    YAML::Node comp;
                    comp["active"] = meshCollider->active;
                    comp["useAttachedModel"] = meshCollider->useAttachedModel;
                    if (!meshCollider->modelPath.empty())
                    {
                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(meshCollider->modelPath))
                            comp["modelUUID"] = (uint64_t)meta->uuid;
                    }
                    _node["Canis::MeshCollider"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::MeshCollider"];
                if (!comp)
                    comp = _node["Canis::MeshCollider"];

                if (comp)
                {
                    auto &meshCollider = *_entity.AddComponent<MeshCollider>();
                    meshCollider.active = comp["active"].as<bool>(true);
                    meshCollider.useAttachedModel = comp["useAttachedModel"].as<bool>(true);
                    meshCollider.modelPath.clear();
                    if (YAML::Node modelUUIDNode = comp["modelUUID"])
                    {
                        const UUID uuid = modelUUIDNode.as<uint64_t>(0);
                        if ((uint64_t)uuid != 0)
                        {
                            const std::string path = AssetManager::GetPath(uuid);
                            if (path.rfind("Path was not found", 0) != 0)
                                meshCollider.modelPath = path;
                        }
                    }

                    if (meshCollider.modelPath.empty())
                        meshCollider.modelPath = comp["modelPath"].as<std::string>("");

                    meshCollider.modelId = -1;
                    if (!meshCollider.modelPath.empty())
                        meshCollider.modelId = AssetManager::LoadModel(meshCollider.modelPath);
                    if (_callCreate)
                        meshCollider.Create();
                }
            },
            .DrawInspector = [this](Editor &_editor, Entity &_entity, const ScriptConf &_conf) -> void {
                MeshCollider *meshCollider = _entity.HasComponent<MeshCollider>() ? &_entity.GetComponent<MeshCollider>() : nullptr;
                if (meshCollider == nullptr)
                    return;

                ImGui::Checkbox(("active##" + _conf.name).c_str(), &meshCollider->active);
                ImGui::Checkbox(("useAttachedModel##" + _conf.name).c_str(), &meshCollider->useAttachedModel);
                std::string modelPath = meshCollider->modelPath;
                if (ImGui::InputText(("modelPath##" + _conf.name).c_str(), &modelPath))
                {
                    meshCollider->modelPath = modelPath;
                    meshCollider->modelId = meshCollider->modelPath.empty() ? -1 : AssetManager::LoadModel(meshCollider->modelPath);
                }
            },
        };

        RegisterScript(meshColliderConf);

        ScriptConf cameraConf = {
            .name = "Canis::Camera",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.AddComponent<Camera>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Camera>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Camera>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Camera>() ? (void*)(&_entity.GetComponent<Camera>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Camera>())
                {
                    Camera& camera = _entity.GetComponent<Camera>();
                    YAML::Node comp;
                    comp["primary"] = camera.primary;
                    comp["fovDegrees"] = camera.fovDegrees;
                    comp["nearClip"] = camera.nearClip;
                    comp["farClip"] = camera.farClip;
                    _node["Canis::Camera"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Camera"];
                if (!comp)
                    comp = _node["Canis::Camera"];

                if (comp)
                {
                    auto &camera = *_entity.AddComponent<Camera>();
                    camera.primary = comp["primary"].as<bool>(true);
                    camera.fovDegrees = comp["fovDegrees"].as<float>(60.0f);
                    camera.nearClip = comp["nearClip"].as<float>(0.1f);
                    camera.farClip = comp["farClip"].as<float>(1000.0f);
                    if (_callCreate)
                        camera.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Camera* camera = nullptr;
                if (_entity.HasComponent<Camera>() && ((camera = &_entity.GetComponent<Camera>()), true))
                {
                    ImGui::Checkbox("primary", &camera->primary);
                    ImGui::InputFloat("fovDegrees", &camera->fovDegrees);
                    ImGui::InputFloat("nearClip", &camera->nearClip);
                    ImGui::InputFloat("farClip", &camera->farClip);
                }
            },
        };

        RegisterScript(cameraConf);

        ScriptConf directionalLightConf = {
            .name = "Canis::DirectionalLight",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                _entity.AddComponent<DirectionalLight>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<DirectionalLight>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<DirectionalLight>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<DirectionalLight>() ? (void*)(&_entity.GetComponent<DirectionalLight>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<DirectionalLight>())
                {
                    DirectionalLight& light = _entity.GetComponent<DirectionalLight>();
                    YAML::Node comp;
                    comp["enabled"] = light.enabled;
                    comp["color"] = light.color;
                    comp["intensity"] = light.intensity;
                    comp["direction"] = light.direction;
                    _node["Canis::DirectionalLight"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::DirectionalLight"])
                {
                    auto &light = *_entity.AddComponent<DirectionalLight>();
                    light.enabled = comp["enabled"].as<bool>(true);
                    light.color = comp["color"].as<Vector4>(Color(1.0f));
                    light.intensity = comp["intensity"].as<float>(1.0f);
                    light.direction = comp["direction"].as<Vector3>(Vector3(-0.4f, -1.0f, -0.25f));
                    if (_callCreate)
                        light.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                DirectionalLight* light = nullptr;
                if (_entity.HasComponent<DirectionalLight>() && ((light = &_entity.GetComponent<DirectionalLight>()), true))
                {
                    ImGui::Checkbox("enabled", &light->enabled);
                    ImGui::ColorEdit3("color", &light->color.r);
                    ImGui::InputFloat("intensity", &light->intensity);
                    ImGui::InputFloat3("direction", &light->direction.x, "%.3f");
                }
            },
        };

        RegisterScript(directionalLightConf);

        ScriptConf pointLightConf = {
            .name = "Canis::PointLight",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                _entity.AddComponent<PointLight>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<PointLight>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<PointLight>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<PointLight>() ? (void*)(&_entity.GetComponent<PointLight>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<PointLight>())
                {
                    PointLight& light = _entity.GetComponent<PointLight>();
                    YAML::Node comp;
                    comp["enabled"] = light.enabled;
                    comp["color"] = light.color;
                    comp["intensity"] = light.intensity;
                    comp["range"] = light.range;
                    _node["Canis::PointLight"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::PointLight"])
                {
                    auto &light = *_entity.AddComponent<PointLight>();
                    light.enabled = comp["enabled"].as<bool>(true);
                    light.color = comp["color"].as<Vector4>(Color(1.0f));
                    light.intensity = comp["intensity"].as<float>(1.2f);
                    light.range = comp["range"].as<float>(12.0f);
                    if (_callCreate)
                        light.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                PointLight* light = nullptr;
                if (_entity.HasComponent<PointLight>() && ((light = &_entity.GetComponent<PointLight>()), true))
                {
                    ImGui::Checkbox("enabled", &light->enabled);
                    ImGui::ColorEdit3("color", &light->color.r);
                    ImGui::InputFloat("intensity", &light->intensity);
                    ImGui::InputFloat("range", &light->range);
                }
            },
        };

        RegisterScript(pointLightConf);

        ScriptConf materialConf = {
            .name = "Canis::Material",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Model>())
                {
                    if (!_entity.HasComponent<Transform>())
                        _entity.AddComponent<Transform>();

                    Model* model = _entity.AddComponent<Model>();
                    model->modelId = AssetManager::LoadModel("assets/models/dq.gltf");
                }

                Material* material = _entity.AddComponent<Material>();
                material->materialId = AssetManager::LoadMaterial("assets/defaults/materials/default.material");
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Material>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<Material>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Material>() ? (void*)(&_entity.GetComponent<Material>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Material>())
                {
                    Material& material = _entity.GetComponent<Material>();
                    YAML::Node comp;
                    comp["color"] = material.color;

                    if (material.materialId > -1)
                    {
                        const std::string materialPath = AssetManager::GetPath(material.materialId);
                        if (materialPath.rfind("Path was not found", 0) != 0)
                        {
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(materialPath))
                            {
                                YAML::Node materialAssetNode;
                                materialAssetNode["uuid"] = (uint64_t)meta->uuid;
                                comp["MaterialAsset"] = materialAssetNode;
                            }
                        }
                    }

                    YAML::Node slotAssets = YAML::Node(YAML::NodeType::Sequence);
                    for (i32 slotMaterialId : material.materialIds)
                    {
                        if (slotMaterialId < 0)
                        {
                            slotAssets.push_back(YAML::Node());
                            continue;
                        }

                        const std::string slotPath = AssetManager::GetPath(slotMaterialId);
                        if (slotPath.rfind("Path was not found", 0) == 0)
                        {
                            slotAssets.push_back(YAML::Node());
                            continue;
                        }

                        YAML::Node slotAssetNode;
                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(slotPath))
                        {
                            slotAssetNode["uuid"] = (uint64_t)meta->uuid;
                            slotAssets.push_back(slotAssetNode);
                        }
                        else
                        {
                            slotAssets.push_back(YAML::Node());
                        }
                    }

                    if (!slotAssets.IsNull() && slotAssets.size() > 0)
                        comp["MaterialSlots"] = slotAssets;

                    _node["Canis::Material"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::Material"])
                {
                    auto &material = *_entity.AddComponent<Material>();
                    material.color = comp["color"].as<Vector4>(Color(1.0f));

                    std::string path = "";
                    if (auto materialAsset = comp["MaterialAsset"])
                    {
                        if (auto uuidNode = materialAsset["uuid"])
                        {
                            UUID uuid = uuidNode.as<uint64_t>(0);
                            path = AssetManager::GetPath(uuid);
                            if (path.rfind("Path was not found", 0) == 0)
                                path.clear();
                        }

                        if (path.empty())
                            path = materialAsset["path"].as<std::string>("");
                    }

                    if (!path.empty())
                        material.materialId = AssetManager::LoadMaterial(path);

                    material.materialIds.clear();
                    if (auto slotAssets = comp["MaterialSlots"]; slotAssets && slotAssets.IsSequence())
                    {
                        material.materialIds.resize(slotAssets.size(), -1);
                        for (size_t i = 0; i < slotAssets.size(); ++i)
                        {
                            const YAML::Node slotNode = slotAssets[i];
                            if (!slotNode || slotNode.IsNull())
                                continue;

                            std::string slotPath = "";
                            if (auto uuidNode = slotNode["uuid"])
                            {
                                UUID uuid = uuidNode.as<uint64_t>(0);
                                slotPath = AssetManager::GetPath(uuid);
                                if (slotPath.rfind("Path was not found", 0) == 0)
                                    slotPath.clear();
                            }

                            if (slotPath.empty())
                                slotPath = slotNode["path"].as<std::string>("");

                            if (!slotPath.empty())
                                material.materialIds[i] = AssetManager::LoadMaterial(slotPath);
                        }
                    }

                    if (_callCreate)
                        material.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Material* material = nullptr;
                if (_entity.HasComponent<Material>() && ((material = &_entity.GetComponent<Material>()), true))
                {
                    auto getMaterialLabel = [](i32 _materialId) -> std::string
                    {
                        if (_materialId < 0)
                            return "[ empty ]";

                        std::string path = AssetManager::GetPath(_materialId);
                        if (path.rfind("Path was not found", 0) == 0)
                            return "[ missing ]";

                        if (MetaFileAsset* meta = AssetManager::GetMetaFile(path))
                            return meta->name;

                        return path;
                    };

                    auto handleMaterialDrop = [](i32 &_materialId) -> void
                    {
                        if (!ImGui::BeginDragDropTarget())
                            return;

                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                        {
                            const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                            std::string path = std::string(dropped.path);
                            if (path.empty() || !FileExists(path.c_str()))
                                path = AssetManager::GetPath(dropped.uuid);

                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(path))
                            {
                                if (meta->type == MetaFileAsset::FileType::MATERIAL)
                                    _materialId = AssetManager::LoadMaterial(path);
                            }
                        }
                        ImGui::EndDragDropTarget();
                    };

                    ImGui::ColorEdit4("material color", &material->color.r);

                    std::string materialLabel = getMaterialLabel(material->materialId);

                    ImGui::Text("material");
                    ImGui::SameLine();
                    ImGui::Button(materialLabel.c_str(), ImVec2(150, 0));
                    handleMaterialDrop(material->materialId);

                    Model* model = _entity.HasComponent<Model>() ? &_entity.GetComponent<Model>() : nullptr;
                    ModelAsset* modelAsset = nullptr;
                    if (model != nullptr && model->modelId >= 0)
                        modelAsset = AssetManager::GetModel(model->modelId);

                    const i32 slotCount = (modelAsset != nullptr) ? modelAsset->GetMaterialSlotCount() : 0;
                    ImGui::Text("material slots: %d", slotCount);
                    if (slotCount > 0)
                    {
                        material->materialIds.resize(static_cast<size_t>(slotCount), -1);

                        for (i32 slotIndex = 0; slotIndex < slotCount; ++slotIndex)
                        {
                            const std::string slotName = modelAsset->GetMaterialSlotName(slotIndex);
                            const std::string slotLabel = slotName.empty()
                                ? ("slot " + std::to_string(slotIndex))
                                : ("slot " + std::to_string(slotIndex) + " (" + slotName + ")");
                            ImGui::Text("%s", slotLabel.c_str());
                            ImGui::SameLine();

                            std::string buttonLabel = getMaterialLabel(material->materialIds[static_cast<size_t>(slotIndex)]);
                            buttonLabel += "##material_slot_" + std::to_string(slotIndex);
                            ImGui::Button(buttonLabel.c_str(), ImVec2(180, 0));
                            handleMaterialDrop(material->materialIds[static_cast<size_t>(slotIndex)]);
                        }
                    }
                }
            },
        };

        RegisterScript(materialConf);

        ScriptConf modelConf = {
            .name = "Canis::Model",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Transform>())
                    _entity.AddComponent<Transform>();

                Model* model = _entity.AddComponent<Model>();
                model->modelId = AssetManager::LoadModel("assets/defaults/models/cube.glb");

                if (!_entity.HasComponent<Material>())
                {
                    Material* material = _entity.AddComponent<Material>();
                    material->materialId = AssetManager::LoadMaterial("assets/defaults/materials/default.material");
                }
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<Model>(); },
            .Remove = [this](Entity& _entity) -> void {
                _entity.RemoveComponent<ModelAnimation>();
                _entity.RemoveComponent<Model>();
            },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<Model>() ? (void*)(&_entity.GetComponent<Model>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<Model>())
                {
                    Model& model = _entity.GetComponent<Model>();
                    YAML::Node comp;
                    comp["color"] = model.color;

                    if (model.modelId > -1)
                    {
                        if (ModelAsset* modelAsset = AssetManager::GetModel(model.modelId))
                        {
                            YAML::Node modelAssetNode;
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(modelAsset->GetPath()))
                                modelAssetNode["uuid"] = (uint64_t)meta->uuid;

                            comp["ModelAsset"] = modelAssetNode;
                        }
                    }

                    _node["Canis::Model"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::Model"];
                if (!comp)
                    comp = _node["Canis::Model"];

                if (comp)
                {
                    auto &model = *_entity.AddComponent<Model>();
                    model.color = comp["color"].as<Vector4>(Color(1.0f));

                    std::string path = "";
                    if (auto modelAsset = comp["ModelAsset"])
                    {
                        if (auto uuidNode = modelAsset["uuid"])
                        {
                            UUID uuid = uuidNode.as<uint64_t>(0);
                            path = AssetManager::GetPath(uuid);
                            if (path.rfind("Path was not found", 0) == 0)
                                path.clear();
                        }

                        if (path.empty())
                            path = modelAsset["path"].as<std::string>("");
                    }

                    if (!path.empty())
                        model.modelId = AssetManager::LoadModel(path);

                    // Backward compatibility: migrate legacy animation fields on Canis::Model.
                    //if (!_node["Canis::ModelAnimation"])
                    //{
                    //    const bool hasLegacyAnimation =
                    //        comp["playAnimation"].IsDefined() ||
                    //        comp["loop"].IsDefined() ||
                    //        comp["animationSpeed"].IsDefined() ||
                    //        comp["animationTime"].IsDefined() ||
                    //        comp["animationIndex"].IsDefined();
//
                    //    if (hasLegacyAnimation && !_entity.HasComponent<ModelAnimation>())
                    //    {
                    //        auto &animation = *_entity.AddComponent<ModelAnimation>();
                    //        animation.playAnimation = comp["playAnimation"].as<bool>(true);
                    //        animation.loop = comp["loop"].as<bool>(true);
                    //        animation.animationSpeed = comp["animationSpeed"].as<float>(1.0f);
                    //        animation.animationTime = comp["animationTime"].as<float>(0.0f);
                    //        animation.animationIndex = comp["animationIndex"].as<i32>(0);
//
                    //        if (_callCreate)
                    //            animation.Create();
                    //    }
                    //}

                    if (_callCreate)
                        model.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                Model* model = nullptr;
                if (_entity.HasComponent<Model>() && ((model = &_entity.GetComponent<Model>()), true))
                {
                    ImGui::ColorEdit4("color", &model->color.r);

                    std::string modelLabel = "[ empty ]";
                    ModelAsset* modelAsset = nullptr;
                    if (model->modelId > -1)
                    {
                        modelAsset = AssetManager::GetModel(model->modelId);
                        if (modelAsset != nullptr)
                        {
                            if (MetaFileAsset* meta = AssetManager::GetMetaFile(modelAsset->GetPath()))
                                modelLabel = meta->name;
                            else
                                modelLabel = modelAsset->GetPath();
                        }
                    }

                    ImGui::Text("model");
                    ImGui::SameLine();
                    ImGui::Button(modelLabel.c_str(), ImVec2(150, 0));

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_DRAG"))
                        {
                            const AssetDragData dropped = *static_cast<const AssetDragData*>(payload->Data);
                            std::string path = AssetManager::GetPath(dropped.uuid);
                            std::string extension = GetFileExtension(path);

                            if (extension == "gltf" || extension == "glb")
                            {
                                model->modelId = AssetManager::LoadModel(path);
                                if (ModelAnimation* animation = _entity.HasComponent<ModelAnimation>() ? &_entity.GetComponent<ModelAnimation>() : nullptr)
                                {
                                    animation->animationTime = 0.0f;
                                    animation->animationIndex = 0;
                                    animation->poseModelId = -1;
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                }
            },
        };

        RegisterScript(modelConf);

        ScriptConf modelAnimationConf = {
            .name = "Canis::ModelAnimation",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Model>())
                {
                    if (!_entity.HasComponent<Transform>())
                        _entity.AddComponent<Transform>();

                    Model* model = _entity.AddComponent<Model>();
                    model->modelId = AssetManager::LoadModel("assets/models/dq.gltf");
                }

                ModelAnimation* animation = _entity.AddComponent<ModelAnimation>();
                animation->animationIndex = 0;
                animation->animationTime = 0.0f;
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<ModelAnimation>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<ModelAnimation>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<ModelAnimation>() ? (void*)(&_entity.GetComponent<ModelAnimation>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<ModelAnimation>())
                {
                    ModelAnimation& animation = _entity.GetComponent<ModelAnimation>();
                    YAML::Node comp;
                    comp["playAnimation"] = animation.playAnimation;
                    comp["loop"] = animation.loop;
                    comp["animationSpeed"] = animation.animationSpeed;
                    comp["animationTime"] = animation.animationTime;
                    comp["animationIndex"] = animation.animationIndex;
                    _node["Canis::ModelAnimation"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                YAML::Node comp = _node["Canis::ModelAnimation"];
                if (!comp)
                    comp = _node["Canis::ModelAnimation"];

                if (comp)
                {
                    auto &animation = *_entity.AddComponent<ModelAnimation>();
                    animation.playAnimation = comp["playAnimation"].as<bool>(true);
                    animation.loop = comp["loop"].as<bool>(true);
                    animation.animationSpeed = comp["animationSpeed"].as<float>(1.0f);
                    animation.animationTime = comp["animationTime"].as<float>(0.0f);
                    animation.animationIndex = comp["animationIndex"].as<i32>(0);

                    if (_callCreate)
                        animation.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                ModelAnimation* animation = nullptr;
                if (_entity.HasComponent<ModelAnimation>() && ((animation = &_entity.GetComponent<ModelAnimation>()), true))
                {
                    ImGui::Checkbox("playAnimation", &animation->playAnimation);
                    ImGui::Checkbox("loop", &animation->loop);
                    ImGui::InputFloat("animationSpeed", &animation->animationSpeed);
                    ImGui::InputFloat("animationTime", &animation->animationTime);

                    ModelAsset* modelAsset = nullptr;
                    if (Model* model = _entity.HasComponent<Model>() ? &_entity.GetComponent<Model>() : nullptr)
                    {
                        if (model->modelId > -1)
                            modelAsset = AssetManager::GetModel(model->modelId);
                    }

                    if (modelAsset != nullptr)
                    {
                        const i32 animationCount = modelAsset->GetAnimationCount();
                        ImGui::Text("animations: %d", animationCount);

                        if (animationCount > 0)
                        {
                            animation->animationIndex = std::clamp(animation->animationIndex, 0, animationCount - 1);
                            ImGui::InputInt("animationIndex", &animation->animationIndex);
                            animation->animationIndex = std::clamp(animation->animationIndex, 0, animationCount - 1);

                            ImGui::Text("clip: %s", modelAsset->GetAnimationName(animation->animationIndex).c_str());
                        }
                    }
                    else
                    {
                        ImGui::Text("Model is required.");
                    }
                }
            },
        };

        RegisterScript(modelAnimationConf);

        ScriptConf spriteAnimationConf = {
            .name = "Canis::SpriteAnimation",
            .Construct = nullptr,
            .Add = [this](Entity& _entity) -> void {
                if (!_entity.HasComponent<Sprite2D>())
                {
                    Sprite2D* sprite = _entity.AddComponent<Sprite2D>();
                    sprite->textureHandle = Canis::AssetManager::GetTextureHandle("assets/defaults/textures/square.png");
                }
                
                SpriteAnimation* anim = _entity.AddComponent<SpriteAnimation>();
            },
            .Has = [this](Entity& _entity) -> bool { return _entity.HasComponent<SpriteAnimation>(); },
            .Remove = [this](Entity& _entity) -> void { _entity.RemoveComponent<SpriteAnimation>(); },
            .Get = [this](Entity& _entity) -> void* { return _entity.HasComponent<SpriteAnimation>() ? (void*)(&_entity.GetComponent<SpriteAnimation>()) : nullptr; },
            .Encode = [](YAML::Node &_node, Entity &_entity) -> void {
                if (_entity.HasComponent<SpriteAnimation>())
                {
                    SpriteAnimation& animation = _entity.GetComponent<SpriteAnimation>();

                    YAML::Node comp;
                    comp["id"] = (uint64_t)AssetManager::GetMetaFile(AssetManager::GetPath(animation.id))->uuid;
                    comp["speed"] = animation.speed;

                    _node["Canis::SpriteAnimation"] = comp;
                }
            },
            .Decode = [](YAML::Node &_node, Entity &_entity, bool _callCreate) -> void {
                if (auto comp = _node["Canis::SpriteAnimation"])
                {
                    auto &animation = *_entity.AddComponent<SpriteAnimation>();
                    
                    Canis::UUID uuid = comp["id"].as<u64>();
                    AssetManager::GetSpriteAnimation(AssetManager::GetPath(uuid));
                    animation.id = AssetManager::GetID(uuid);
                    animation.speed = comp["speed"].as<f32>(1.0f);
                    
                    if (_callCreate)
                        animation.Create();
                }
            },
            .DrawInspector = [this](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void {
                SpriteAnimation* animation = nullptr;
                if (_entity.HasComponent<SpriteAnimation>() && ((animation = &_entity.GetComponent<SpriteAnimation>()), true))
                {
                    
                    _editor.InputAnimationClip("animation", animation->id);
                    ImGui::InputFloat("speed", &animation->speed);
                }
            },
        };

        RegisterScript(spriteAnimationConf);

        // register inspector items
        InspectorItemRightClick inspectorCreateSquare = {
            .name = "Create Square",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *entityOne = _app.scene.CreateEntity("Square");
                RectTransform * transform = entityOne->AddComponent<RectTransform>();
                Canis::Sprite2D *sprite = entityOne->AddComponent<Sprite2D>();

                sprite->textureHandle = Canis::AssetManager::GetTextureHandle("assets/defaults/textures/square.png");
                transform->size = Vector2(64.0f);
            }
        };

        RegisterInspectorItem(inspectorCreateSquare);

        InspectorItemRightClick inspectorCreateCircle = {
            .name = "Create Circle",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *entityOne = _app.scene.CreateEntity("Circle");
                RectTransform * transform = entityOne->AddComponent<RectTransform>();
                Canis::Sprite2D *sprite = entityOne->AddComponent<Sprite2D>();

                sprite->textureHandle = Canis::AssetManager::GetTextureHandle("assets/defaults/textures/circle.png");
                transform->size = Vector2(64.0f);
            }
        };

        RegisterInspectorItem(inspectorCreateCircle);

        InspectorItemRightClick inspectorCreateDirectionalLight = {
            .name = "Create Directional Light",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *lightEntity = _app.scene.CreateEntity("Directional Light");
                lightEntity->AddComponent<DirectionalLight>();
            }
        };

        RegisterInspectorItem(inspectorCreateDirectionalLight);

        InspectorItemRightClick inspectorCreatePointLight = {
            .name = "Create Point Light",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *lightEntity = _app.scene.CreateEntity("Point Light");
                Transform *transform = lightEntity->AddComponent<Transform>();
                transform->position = Vector3(2.0f, 2.5f, 2.0f);
                lightEntity->AddComponent<PointLight>();
            }
        };

        RegisterInspectorItem(inspectorCreatePointLight);

        InspectorItemRightClick inspectorCreateCube = {
            .name = "Create Cube",
            .Func = [](App& _app, Editor& _editor, Entity& _entity, std::vector<ScriptConf>& _scriptConfs) -> void {
                Canis::Entity *cube = _app.scene.CreateEntity("Cube");
                
                Transform *transform = cube->AddComponent<Transform>();
                transform->position = Vector3(0.0f);
                
                Model* model = cube->AddComponent<Model>();
                model->modelId = AssetManager::LoadModel("assets/defaults/models/cube.glb");

                Material* material = cube->AddComponent<Material>();
                material->materialId = AssetManager::LoadMaterial("assets/defaults/materials/default.material");
            }
        };

        RegisterInspectorItem(inspectorCreateCube);
    }

    float App::FPS()
    {
        return Time::FPS();
    }

    float App::DeltaTime()
    {
        return Time::DeltaTime();
    }

    void App::SetTargetFPS(float _targetFPS)
    {
        Time::SetTargetFPS(_targetFPS);
    }

    ScriptConf* App::GetScriptConf(const std::string& _name)
    {
        for(ScriptConf& sc : m_scriptRegistry)
        {
            if (sc.name == _name)
            {
                return &sc;
            }
        }

        return nullptr;
    }

    bool App::DispatchUIAction(Entity& _targetEntity, const std::string& _scriptName, const std::string& _actionName, const UIActionContext& _context)
    {
        if (_actionName.empty())
            return false;

        auto invokeAction = [&](ScriptConf& _conf) -> bool
        {
            auto actionIt = _conf.uiActions.find(_actionName);
            if (actionIt == _conf.uiActions.end() || _conf.Get == nullptr)
                return false;

            if (ScriptableEntity* script = static_cast<ScriptableEntity*>(_conf.Get(_targetEntity)))
            {
                actionIt->second(*script, _context);
                return true;
            }

            return false;
        };

        if (!_scriptName.empty())
        {
            if (ScriptConf* conf = GetScriptConf(_scriptName))
                return invokeAction(*conf);

            return false;
        }

        bool handled = false;
        for (ScriptConf& conf : m_scriptRegistry)
            handled = invokeAction(conf) || handled;

        return handled;
    }

    bool App::AddRequiredScript(Entity& _entity, const std::string& _name)
    {
        if (ScriptConf* sc = GetScriptConf(_name))
        {
            if (sc->Has && sc->Has(_entity) == false)
            {
                if (sc->Add)
                    sc->Add(_entity);
            }

            return true;
        }
        else
        {
            return false;
        }
    }

    void App::RegisterScript(ScriptConf &_conf)
    {
        for (ScriptConf &sc : m_scriptRegistry)
            if (_conf.name == sc.name)
                return;

        m_scriptRegistry.push_back(_conf);
    }

    void App::UnregisterScript(ScriptConf &_conf)
    {
        for (int i = 0; i < m_scriptRegistry.size(); i++)
        {
            if (_conf.name == m_scriptRegistry[i].name)
            {
                ScriptConf& conf = m_scriptRegistry[i];

                for (Entity* entity : scene.GetEntities())
                {
                    if (entity == nullptr)
                        continue;

                    entity->RemoveScript(conf.name);
                }

                m_scriptRegistry.erase(m_scriptRegistry.begin() + i);
                i--;
            }
        }
    }

    void App::RegisterInspectorItem(InspectorItemRightClick& _item)
    {
        for (InspectorItemRightClick &item : m_inspectorItemRegistry)
            if (item.name == _item.name)
                return;

        m_inspectorItemRegistry.push_back(_item);
    }

    void App::UnregisterInspectorItem(InspectorItemRightClick& _item)
    {
        for (int i = 0; i < m_inspectorItemRegistry.size(); i++)
        {
            if (_item.name == m_inspectorItemRegistry[i].name)
            {
                m_inspectorItemRegistry.erase(m_inspectorItemRegistry.begin() + i);
                i--;
            }
        }
    }

} // namespace Canis
