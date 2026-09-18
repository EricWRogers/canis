#include <Canis/Scripting/CSharpRuntime.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/Scripting/WebBindings.hpp>
#include <emscripten.h>
#include <stdexcept>

extern "C" EMSCRIPTEN_KEEPALIVE const char* canis_web_dispatch(const char* request)
{
    static std::string response;
    response = Canis::Scripting::DispatchWebBinding(request ? request : "");
    return response.c_str();
}

EM_JS(int, CanisManagedCommand, (int operation, double delta), {
    try {
        if (!Module.canisManaged) throw new Error('C# browser host was not loaded');
        const error = Module.canisManaged.Command(operation, delta);
        if (error) throw new Error(error);
        return 0;
    } catch (error) {
        console.error(error);
        if (Module.setStatus) Module.setStatus('C# runtime failed: ' + error.message);
        return -1;
    }
});

EM_JS(int, CanisManagedBindingsMatch, (const char* bindings), {
    if (Module.canisManagedBindings === UTF8ToString(bindings)) return 1;
    const message = 'Packaged C# bindings do not match this native web build.';
    console.error(message);
    if (Module.setStatus) Module.setStatus(message);
    return 0;
});

namespace Canis::Scripting
{
    struct CSharpRuntime::Impl
    {
        bool initialized = false, failed = false, playing = false;
        std::string status = "C#: browser host awaiting initialization", output;
        bool Call(int operation, double delta = 0)
        {
            if (CanisManagedCommand(operation, delta) == 0) return true;
            failed = true;
            output = status = "C#: browser runtime failed; see browser console";
            return false;
        }
    };
    CSharpRuntime::CSharpRuntime(const std::filesystem::path&, const std::filesystem::path&) : impl(std::make_unique<Impl>()) {}
    CSharpRuntime::~CSharpRuntime() { StopSession(); }
    void CSharpRuntime::Tick(bool playSession, bool paused, float dt, bool executeCallbacks)
    {
        auto& s = *impl;
        if (!s.initialized && !s.failed)
        {
            if (!CanisManagedBindingsMatch(NativeBindingRegistry::Get().GenerateCSharp().c_str()))
            {
                s.failed = true;
                s.output = s.status = "Packaged C# bindings do not match this native web build.";
                return;
            }
            s.initialized = s.Call(0);
            if (s.initialized) s.status = "C#: packaged browser player ready";
        }
        if (s.playing && !playSession) StopSession();
        if (executeCallbacks && playSession) RunGameplay(paused, dt);
    }
    void CSharpRuntime::RunGameplay(bool paused, float dt)
    {
        auto& s = *impl;
        if (!ReadyToPlay()) return;
        if (!s.playing) s.playing = s.Call(1);
        if (s.playing && !s.failed && !paused) s.Call(2, dt);
    }
    void CSharpRuntime::StopSession()
    {
        if (impl->playing) { impl->Call(3); impl->playing = false; }
    }
    bool CSharpRuntime::ReadyToPlay() const { return impl->initialized && !impl->failed; }
    bool CSharpRuntime::HasBuildError() const { return impl->failed; }
    bool CSharpRuntime::Busy() const { return false; }
    const std::string& CSharpRuntime::Status() const { return impl->status; }
    const std::string& CSharpRuntime::BuildOutput() const { return impl->output; }
    std::string CSharpRuntime::ProjectPath() const { return {}; }
    void CSharpRuntime::RequestLanguage(const std::string&, const std::string&, const std::string&, int, const std::map<std::string,std::string>&, const std::string&) {}
    std::vector<ScriptEditing::LanguageReply> CSharpRuntime::PollLanguage() { return {}; }
    void CSharpRuntime::ExportPlayer(const std::filesystem::path&) const { throw std::runtime_error("Export C# browser scripts on the desktop before deployment."); }
    void CSharpRuntime::SetLiveReload(bool) {}
    bool CSharpRuntime::LiveReload() const { return false; }
    void CSharpRuntime::RequestBuild() { impl->output = "Browser scripts are precompiled; rebuild the web export to change scripts."; }
    void CSharpRuntime::RefreshSources() {}
    bool CSharpRuntime::ReloadOnSave() const { return false; }
    void CSharpRuntime::SetReloadOnSave(bool) {}
    int CSharpRuntime::CollectRetiredContexts() { return 0; }
}
