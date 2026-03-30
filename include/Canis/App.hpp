#pragma once
#include <Canis/Scene.hpp>
#include <Canis/ConfigData.hpp>

namespace Canis
{
class Editor;

class App
{
public:
    Scene scene;
    ~App();
    void Run();

    // Time
    float FPS();
    float DeltaTime();
    void SetTargetFPS(float _targetFPS);
    float UpdateTimeMs() const { return m_updateTimeMs; }
    float SceneUpdateTimeMs() const { return m_sceneUpdateTimeMs; }
    float GameCodeUpdateTimeMs() const { return m_gameCodeUpdateTimeMs; }
    float RenderTimeMs() const { return m_renderTimeMs; }

    void RegisterScript(ScriptConf& _conf);
    void UnregisterScript(ScriptConf& _conf);
    std::vector<ScriptConf>& GetScriptRegistry() { return m_scriptRegistry; }

    ScriptConf* GetScriptConf(const std::string& _name);
    bool AddRequiredScript(Entity& _entity, const std::string& _name);
    bool DispatchUIAction(Entity& _targetEntity, const std::string& _scriptName, const std::string& _actionName, const UIActionContext& _context);

    Editor& GetEditor() { return *m_editor; }

    void RegisterInspectorItem(InspectorItemRightClick& _item);
    void UnregisterInspectorItem(InspectorItemRightClick& _item);
    std::vector<InspectorItemRightClick>& GetInspectorItemRegistry() { return m_inspectorItemRegistry; }
private:
    struct RuntimeContext;

    std::vector<ScriptConf> m_scriptRegistry = {};
    std::vector<InspectorItemRightClick> m_inspectorItemRegistry = {};

    void InitializeRuntime();
    bool RunFrame();
    void ShutdownRuntime();
#if defined(__EMSCRIPTEN__)
    static void WebMainLoop(void *_appPtr);
#endif
    void RegisterDefaults(Editor& _editor);
    Editor* m_editor;
    RuntimeContext* m_runtime = nullptr;
    float m_updateTimeMs = 0.0f;
    float m_sceneUpdateTimeMs = 0.0f;
    float m_gameCodeUpdateTimeMs = 0.0f;
    float m_renderTimeMs = 0.0f;
};
}
