#pragma once
#include <Canis/Scene.hpp>
#include <Canis/ConfigData.hpp>
#include <Canis/Network.hpp>

#include <memory>

namespace Canis
{
class Editor;

class App
{
public:
    Scene scene;
    ~App();
    int Run(int _argc = 0, char **_argv = nullptr);

    // Time
    float FPS();
    float DeltaTime();
    void SetTargetFPS(float _targetFPS);
    float UpdateTimeMs() const { return m_updateTimeMs; }
    float SceneUpdateTimeMs() const { return m_sceneUpdateTimeMs; }
    float GameCodeUpdateTimeMs() const { return m_gameCodeUpdateTimeMs; }
    float RenderTimeMs() const { return m_renderTimeMs; }

    void RegisterComponent(ComponentConf& _conf);
    void UnregisterComponent(ComponentConf& _conf);
    std::vector<ComponentConf>& GetComponentRegistry() { return m_scriptRegistry; }
    ComponentConf* GetComponentConf(const std::string& _name);

    void RegisterScript(ScriptConf& _conf);
    void UnregisterScript(ScriptConf& _conf);
    std::vector<ScriptConf>& GetScriptRegistry() { return m_scriptRegistry; }
    ScriptConf* GetScriptConf(const std::string& _name);
    void BeginGameCodeRegistration() { m_registeringGameCodeScripts = true; }
    void EndGameCodeRegistration() { m_registeringGameCodeScripts = false; }
    void RegisterSystem(SystemConf& _conf);
    void UnregisterSystem(SystemConf& _conf);
    std::vector<SystemConf>& GetSystemRegistry() { return m_systemRegistry; }
    SystemConf* GetSystemConf(const std::string& _name);

    void LoadScene(const std::string& _path);
    void LoadScene(const SceneAssetHandle& _sceneAssetHandle);
    const std::string& GetPendingScenePath() const { return m_pendingScenePath; }
    NetworkSession& GetNetwork();
    const NetworkSession& GetNetwork() const;

    bool AddRequiredComponent(Entity& _entity, const std::string& _name);
    bool AddRequiredScript(Entity& _entity, const std::string& _name);
    bool DispatchUIAction(Entity& _targetEntity, const std::string& _scriptName, const std::string& _actionName, const UIActionContext& _context);
    bool DispatchAnimationEvent(Entity& _targetEntity, const std::string& _scriptName, const std::string& _eventName, const AnimationEventContext& _context);

    Editor& GetEditor() { return *m_editor; }

    void RegisterInspectorItem(InspectorItemRightClick& _item);
    void UnregisterInspectorItem(InspectorItemRightClick& _item);
    std::vector<InspectorItemRightClick>& GetInspectorItemRegistry() { return m_inspectorItemRegistry; }
private:
    struct RuntimeContext;

    std::vector<ScriptConf> m_scriptRegistry = {};
    std::vector<SystemConf> m_systemRegistry = {};
    std::vector<InspectorItemRightClick> m_inspectorItemRegistry = {};

    void InitializeRuntime();
    bool RunFrame();
    void ShutdownRuntime();
#if defined(__EMSCRIPTEN__)
    static void WebMainLoop(void *_appPtr);
#endif
    void RegisterDefaults(Editor& _editor);
    void ProcessPendingSceneLoad();
    Editor* m_editor;
    mutable std::unique_ptr<NetworkSession> m_network = nullptr;
    RuntimeContext* m_runtime = nullptr;
    float m_updateTimeMs = 0.0f;
    float m_sceneUpdateTimeMs = 0.0f;
    float m_gameCodeUpdateTimeMs = 0.0f;
    float m_renderTimeMs = 0.0f;
    bool m_registeringGameCodeScripts = false;
    std::string m_pendingScenePath = "";
    std::vector<std::string> m_commandLineArguments = {};
    std::string m_invocationWorkingDirectory = "";
    int m_exitCode = 0;
};
}
