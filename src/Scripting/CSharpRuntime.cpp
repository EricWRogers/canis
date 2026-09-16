#include <Canis/External/tinygltf/json.hpp>
#include <cstdio>
#include <deque>
#include <Canis/Scripting/CSharpRuntime.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <SDL3/SDL.h>
#include <hostfxr.h>
#include <coreclr_delegates.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <future>
#include <map>
#include <sstream>
#include <stdexcept>

namespace Canis::Scripting
{
namespace
{
    using Clock = std::chrono::steady_clock;
    using Sources = std::map<std::string, std::string>;
    struct Command { int operation = 0, length = 0; const void* payload = nullptr; double delta = 0; };
#ifdef _WIN32
#define CANIS_MANAGED_CALL __cdecl
#else
#define CANIS_MANAGED_CALL
#endif
    using Dispatch = int(CANIS_MANAGED_CALL*)(Command*, int);
    void CANIS_MANAGED_CALL Log(const unsigned char* data, int length)
    {
        try { Debug::Log("%s", std::string(reinterpret_cast<const char*>(data), length).c_str()); }
        catch (...) {} // Logging must never unwind into the CLR.
    }
    std::string Read(const std::filesystem::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) throw std::runtime_error("Cannot read " + path.string());
        std::string result{std::istreambuf_iterator<char>(stream), {}};
        if (stream.bad()) throw std::runtime_error("Cannot read " + path.string());
        return result;
    }
    void Write(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream stream(path, std::ios::binary);
        stream << text; stream.close();
        if (!stream) throw std::runtime_error("Cannot write " + path.string());
    }
    std::string Xml(std::string text)
    {
        std::string result;
        for (char c : text) switch (c)
        {
        case '&': result += "&amp;"; break; case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break; case '"': result += "&quot;"; break;
        default: result += c;
        }
        return result;
    }
    Sources Scan(const std::filesystem::path& assets, const std::filesystem::path& cache)
    {
        Sources sources;
        if (!std::filesystem::exists(assets)) return sources;
        for (auto it = std::filesystem::recursive_directory_iterator(assets); it != std::filesystem::recursive_directory_iterator(); ++it)
        {
            if (it->is_symlink()) { if (it->is_directory()) it.disable_recursion_pending(); continue; }
            if (it->is_directory() && (it->path().filename() == "Editor" || it->path().filename() == "bin" || it->path().filename() == "obj"))
            { it.disable_recursion_pending(); continue; }
            if (it->is_regular_file() && it->path().extension() == ".cs") sources.emplace(it->path().string(), Read(it->path()));
        }
        if (!sources.empty())
        {
            const auto generated = cache / "Canis.Native.generated.cs";
            const auto content = NativeBindingRegistry::Get().GenerateCSharp();
            std::filesystem::create_directories(cache);
            if (!std::filesystem::exists(generated) || Read(generated) != content) Write(generated, content);
            sources[generated.string()] = content;
        }
        if(!sources.empty()) {
            const auto ide=cache.parent_path()/"IDE";std::filesystem::create_directories(ide);
            std::string project="<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><TargetFramework>net10.0</TargetFramework><EnableDefaultCompileItems>false</EnableDefaultCompileItems><ImplicitUsings>enable</ImplicitUsings><Nullable>enable</Nullable></PropertyGroup><ItemGroup>";
            for(auto& [path,_]:sources)project+="<Compile Include=\""+Xml(path)+"\" />";
            project+="<Reference Include=\"Canis.Core\"><HintPath>"+Xml(CANIS_MANAGED_HOST_DIR "/Canis.Core.dll")+"</HintPath></Reference></ItemGroup></Project>";
            if(!std::filesystem::exists(ide/"Game.Runtime.csproj") || Read(ide/"Game.Runtime.csproj")!=project)Write(ide/"Game.Runtime.csproj",project);
            Write(ide/"Canis.Game.slnx","<Solution><Project Path=\"Game.Runtime.csproj\" /></Solution>");
        }
        return sources;
    }
    struct BuildResult { bool success = false; std::string output; std::filesystem::path assembly; Sources sources; };
    BuildResult Build(Sources sources, const std::filesystem::path& directory)
    {
        BuildResult result; result.sources = std::move(sources);
        try
        {
            std::filesystem::create_directories(directory);
            std::string project = "<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><TargetFramework>net10.0</TargetFramework>"
                "<AssemblyName>Game.Runtime</AssemblyName><EnableDefaultCompileItems>false</EnableDefaultCompileItems>"
                "<ImplicitUsings>enable</ImplicitUsings><Nullable>enable</Nullable><DebugType>portable</DebugType>"
                "</PropertyGroup><ItemGroup><Reference Include=\"Canis.Core\"><HintPath>" + Xml(CANIS_MANAGED_HOST_DIR "/Canis.Core.dll") +
                "</HintPath><Private>false</Private></Reference>";
            int index = 0;
            for (const auto& [path, source] : result.sources)
            {
                // Compile immutable snapshots, but diagnostics/PDBs point at the real asset.
                std::string diagnosticPath=path;
                if(const char* root=SDL_getenv("CANIS_MANAGED_SOURCE_ROOT")) {
                    auto relative=std::filesystem::path(path).lexically_relative(std::filesystem::current_path()/"assets");
                    if(!relative.empty() && *relative.begin()!="..")diagnosticPath=(std::filesystem::path(root)/relative).string();
                }
                std::string escaped;
                for (char c : diagnosticPath) { if (c == '\\' || c == '"') escaped += '\\'; escaped += c; }
                const auto file = "Script" + std::to_string(index++) + ".cs";
                Write(directory / file, "#line 1 \"" + escaped + "\"\n" + source);
                project += "<Compile Include=\"" + file + "\" />";
            }
            project += "</ItemGroup></Project>";
            Write(directory / "Game.Runtime.csproj", project);
            // Local config prevents ambient user feeds from affecting this no-package bootstrap project.
            Write(directory / "NuGet.Config", "<configuration><packageSources><clear /></packageSources></configuration>");
            const auto csproj = (directory / "Game.Runtime.csproj").string();
            const auto output = (directory / "out").string();
            const char* args[] = {CANIS_DOTNET_EXECUTABLE, "build", csproj.c_str(), "--output", output.c_str(), "--nologo", "--verbosity", "minimal", nullptr};
            SDL_Process* process = SDL_CreateProcess(args, true);
            if (!process) throw std::runtime_error(SDL_GetError());
            size_t size = 0; int exitCode = -1;
            void* bytes = SDL_ReadProcess(process, &size, &exitCode);
            if (bytes) { result.output.assign(static_cast<const char*>(bytes), size); SDL_free(bytes); }
            SDL_DestroyProcess(process);
            result.assembly = directory / "out/Game.Runtime.dll";
            result.success = exitCode == 0 && std::filesystem::exists(result.assembly);
            if (!result.success && result.output.empty()) result.output = "dotnet build failed without output.";
        }
        catch (const std::exception& error) { result.output = error.what(); }
        return result;
    }
    std::basic_string<char_t> HostString(const std::filesystem::path& path)
    {
#ifdef _WIN32
        return path.wstring();
#else
        return path.string();
#endif
    }
    std::filesystem::path HostDirectory() {
        if(std::filesystem::exists("managed/Canis.ManagedHost.dll"))return std::filesystem::absolute("managed");
        return CANIS_MANAGED_HOST_DIR;
    }
    std::filesystem::path FindHostfxr() {
        std::vector<std::filesystem::path> roots;
        if(const char* configured=SDL_getenv("DOTNET_ROOT"))roots.emplace_back(configured);
#ifdef _WIN32
        if(const char* program=SDL_getenv("ProgramFiles"))roots.emplace_back(std::filesystem::path(program)/"dotnet");
        const auto name="hostfxr.dll";
#else
        roots.emplace_back("/usr/share/dotnet");roots.emplace_back("/usr/lib/dotnet");
        if(const char* home=SDL_getenv("HOME"))roots.emplace_back(std::filesystem::path(home)/".dotnet");
        const auto name="libhostfxr.so";
#endif
        for(auto& root:roots) {
            auto fxr=root/"host/fxr";if(!std::filesystem::exists(fxr))continue;
            std::filesystem::path best;std::pair<int,int> bestVersion{-1,-1};
            for(auto& version:std::filesystem::directory_iterator(fxr)) {
                int major=0,minor=0,patch=0;if(std::sscanf(version.path().filename().string().c_str(),"%d.%d.%d",&major,&minor,&patch)!=3 || major!=10)continue;
                if(std::pair{minor,patch}>bestVersion && std::filesystem::exists(version.path()/name)){best=version.path()/name;bestVersion={minor,patch};}
            }
            if(!best.empty())return best;
        }
        if(!std::filesystem::exists("managed/player.json") && std::filesystem::exists(CANIS_HOSTFXR_PATH))return CANIS_HOSTFXR_PATH;
        throw std::runtime_error(".NET 10 runtime missing. Install the matching 64-bit .NET 10 runtime or set DOTNET_ROOT; the player does not require an SDK.");
    }
    Dispatch LoadHost()
    {
        // Runtime and hostfxr remain loaded for process lifetime. Gameplay contexts are collectible.
        static SDL_SharedObject* library = SDL_LoadObject(FindHostfxr().string().c_str());
        if (!library) throw std::runtime_error(std::string("Cannot load hostfxr: ") + SDL_GetError());
        const auto initialize = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(SDL_LoadFunction(library, "hostfxr_initialize_for_runtime_config"));
        const auto getDelegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(SDL_LoadFunction(library, "hostfxr_get_runtime_delegate"));
        const auto close = reinterpret_cast<hostfxr_close_fn>(SDL_LoadFunction(library, "hostfxr_close"));
        if (!initialize || !getDelegate || !close) throw std::runtime_error("hostfxr exports unavailable.");
        hostfxr_handle context = nullptr;
        const auto config = HostString(HostDirectory()/"Canis.ManagedHost.runtimeconfig.json");
        const int rc = initialize(config.c_str(), nullptr, &context);
        if (rc < 0 || !context) throw std::runtime_error("Cannot initialize .NET 10 runtime (" + std::to_string(rc) + ").");
        load_assembly_and_get_function_pointer_fn load = nullptr;
        const int delegateResult = getDelegate(context, hdt_load_assembly_and_get_function_pointer, reinterpret_cast<void**>(&load));
        close(context);
        if (delegateResult != 0 || !load) throw std::runtime_error("Cannot get managed assembly loader.");
        const auto assembly = HostString(HostDirectory()/"Canis.ManagedHost.dll");
        const auto type = HostString("Canis.ManagedHost.EntryPoint, Canis.ManagedHost");
        const auto method = HostString("Dispatch");
        Dispatch dispatch = nullptr;
        if (load(assembly.c_str(), type.c_str(), method.c_str(), UNMANAGEDCALLERSONLY_METHOD, nullptr, reinterpret_cast<void**>(&dispatch)) != 0 || !dispatch)
            throw std::runtime_error("Cannot load Canis.ManagedHost.Dispatch.");
        return dispatch;
    }
}
struct CSharpRuntime::Impl
{
    std::filesystem::path assets, cache;
    Sources observed, requested;
    std::future<ScriptEditing::LanguageReply> languageJob;
    std::deque<nlohmann::json> languageQueue;
    unsigned languageSequence=0;
    std::future<BuildResult> build;
    Dispatch dispatch = nullptr;
    Clock::time_point scanAt{}, changedAt{};
    std::filesystem::path activeAssembly, stagedAssembly;
    bool player = false;
    bool liveReload = false;
    bool automatic = true, dirty = true, initialized = false, loaded = false, pending = false, playing = false, force = false, failed = false;
    unsigned generation = 0;
    std::string status = "C#: waiting for asset scan", output;
    int Call(int op, const void* data = nullptr, int length = 0, float dt = 0)
    { Command command{op, length, data, dt}; return dispatch ? dispatch(&command, sizeof(command)) : -1; }
    ~Impl() { if (build.valid()) build.wait(); if (dispatch) Call(6); }
};
CSharpRuntime::CSharpRuntime(const std::filesystem::path& assets, const std::filesystem::path& cache) : impl(std::make_unique<Impl>())
{
    impl->assets = std::filesystem::absolute(assets);
    // Separate sessions cannot overwrite assemblies still mapped by another editor.
    impl->cache = std::filesystem::absolute(cache) / ("session-" + std::to_string(SDL_GetTicksNS()));
}
CSharpRuntime::~CSharpRuntime() = default;
std::string CSharpRuntime::ProjectPath() const {return (impl->cache.parent_path()/"IDE/Game.Runtime.csproj").string();}
void CSharpRuntime::RequestLanguage(const std::string& method,const std::string& path,const std::string& text,int cursor,const std::map<std::string,std::string>& overlays) {
    auto& s=*impl;
    std::erase_if(s.languageQueue,[&](const auto& request){return request["path"]==path && request["method"]==method;});
    s.languageQueue.push_back({{"method",method},{"path",path},{"text",text},{"cursor",cursor},{"project",ProjectPath()},{"overlays",overlays}});
}
std::vector<ScriptEditing::LanguageReply> CSharpRuntime::PollLanguage() {
    auto& s=*impl;std::vector<ScriptEditing::LanguageReply> replies;
    if(s.languageJob.valid() && s.languageJob.wait_for(std::chrono::seconds(0))==std::future_status::ready)replies.push_back(s.languageJob.get());
    if(!s.languageJob.valid() && !s.languageQueue.empty() && std::filesystem::exists(ProjectPath())) {
        auto request=s.languageQueue.front();s.languageQueue.pop_front();auto file=s.cache/("language-"+std::to_string(++s.languageSequence)+".json");
        s.languageJob=std::async(std::launch::async,[request,file]() {
            ScriptEditing::LanguageReply reply{request["method"],request["path"],request["text"],"null"};
            try {
                Write(file,request.dump());const auto input=file.string();
                const char* args[]={CANIS_DOTNET_EXECUTABLE,CANIS_LANGUAGE_TOOLS_PATH,input.c_str(),nullptr};
                auto* process=SDL_CreateProcess(args,true);if(!process)throw std::runtime_error(SDL_GetError());
                size_t size=0;int code=0;void* bytes=SDL_ReadProcess(process,&size,&code);
                if(bytes){reply.json.assign(static_cast<const char*>(bytes),size);SDL_free(bytes);}SDL_DestroyProcess(process);
                if(code)throw std::runtime_error(reply.json);
                if(reply.method!="diagnostics")reply.json=nlohmann::json{{"result",nlohmann::json::parse(reply.json)}}.dump();
            }catch(const std::exception& error){reply.json=nlohmann::json{{"error",error.what()}}.dump();}
            std::error_code ignored;std::filesystem::remove(file,ignored);return reply;
        });
    }
    return replies;
}
void CSharpRuntime::ExportPlayer(const std::filesystem::path& destination) const {
    const auto& s=*impl;if(!ReadyToPlay() || s.activeAssembly.empty())throw std::runtime_error("Compile saved C# sources successfully before exporting.");
    std::filesystem::create_directories(destination);
    for(auto& file:std::filesystem::directory_iterator(HostDirectory()))
        if(file.is_regular_file() && file.path().filename().string().starts_with("Canis."))std::filesystem::copy_file(file.path(),destination/file.path().filename(),std::filesystem::copy_options::overwrite_existing);
    for(auto& file:std::filesystem::directory_iterator(s.activeAssembly.parent_path()))
        if(file.is_regular_file() && file.path().filename().string().starts_with("Game.Runtime"))std::filesystem::copy_file(file.path(),destination/file.path().filename(),std::filesystem::copy_options::overwrite_existing);
    Write(destination/"Canis.Native.generated.cs",NativeBindingRegistry::Get().GenerateCSharp());
    Write(destination/"player.json","{\"runtime\":\"net10.0\",\"schema\":1}\n");
}
void CSharpRuntime::RefreshSources() { impl->scanAt = {}; }
void CSharpRuntime::RequestBuild() { impl->force = true; impl->dirty = true; impl->scanAt = {}; }
bool CSharpRuntime::ReloadOnSave() const { return impl->automatic; }
void CSharpRuntime::SetReloadOnSave(bool value) { impl->automatic = value; }
bool CSharpRuntime::ReadyToPlay() const { return !impl->dirty && !impl->failed && !impl->build.valid() && !impl->pending; }
bool CSharpRuntime::Busy() const { return impl->build.valid(); }
const std::string& CSharpRuntime::Status() const { return impl->status; }
const std::string& CSharpRuntime::BuildOutput() const { return impl->output; }
int CSharpRuntime::CollectRetiredContexts() { return impl->Call(7); }
void CSharpRuntime::SetLiveReload(bool enabled) { impl->liveReload=enabled; }
bool CSharpRuntime::LiveReload() const { return impl->liveReload; }
bool CSharpRuntime::HasBuildError() const { return impl->failed; }
void CSharpRuntime::StopSession()
{
    auto& s = *impl;
    if (!s.dispatch || !s.playing) return;
    s.Call(8); s.playing = false;
    if (s.loaded) s.pending = true; // Fresh context resets statics, without recompiling unchanged sources.
}
void CSharpRuntime::RunGameplay(bool paused, float dt)
{
    auto& s = *impl;
    if (!s.playing && s.loaded && ReadyToPlay()) { s.Call(3); s.playing = true; }
    if (s.playing && !paused) s.Call(4, nullptr, 0, dt);
}
void CSharpRuntime::Tick(bool playSession, bool paused, float dt, bool executeCallbacks)
{
    auto& s = *impl;
    try
    {
        if(!s.initialized && std::filesystem::exists("managed/player.json")) {
            s.player=true;s.initialized=true;
            if(Read("managed/Canis.Native.generated.cs")!=NativeBindingRegistry::Get().GenerateCSharp())throw std::runtime_error("Packaged C# bindings do not match this native game build.");
            s.dispatch=LoadHost();s.Call(0,reinterpret_cast<const void*>(&Log));s.Call(9,reinterpret_cast<const void*>(&DispatchNative));
            const auto path=std::filesystem::absolute("managed/Game.Runtime.dll").string();
            if(s.Call(1,path.data(),static_cast<int>(path.size()))!=0 || s.Call(2)!=0)throw std::runtime_error("Packaged gameplay assembly failed validation.");
            s.activeAssembly=path;s.loaded=true;s.dirty=false;s.status="C#: packaged player ready";Debug::Log("%s",s.status.c_str());
        }
        if(s.player) {
            if(s.playing && !playSession)StopSession();
            if(s.pending && !s.playing){if(s.Call(2)!=0)throw std::runtime_error("Packaged restart failed");s.pending=false;}
            if(executeCallbacks && playSession)RunGameplay(paused,dt);
            return;
        }
        const auto now = Clock::now();
        if (now - s.scanAt > std::chrono::milliseconds(250))
        {
            auto sources = Scan(s.assets, s.cache); s.scanAt = now;
            if (!s.initialized || sources != s.observed)
            {
                s.initialized = true; s.observed = std::move(sources); s.changedAt = now; s.dirty = true;
                s.status = "C#: saved changes waiting to compile";
                if (s.observed.empty() && !s.loaded && !s.build.valid()) { s.dirty = false; s.failed = false; s.status = "C#: no runtime scripts"; }
            }
        }
        if (s.playing && !playSession)
        {
            StopSession();
        }
        if (s.build.valid() && s.build.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto result = s.build.get(); s.output = result.output;
            // A newer disk snapshot always wins, including file additions/deletions during compilation.
            auto latest = Scan(s.assets, s.cache);
            if (latest != result.sources)
            {
                s.observed = std::move(latest); s.changedAt = now; s.dirty = true;
                s.status = "C#: discarded outdated build; newer save queued";
            }
            else if (!result.success)
            { s.failed = true; s.status = "C#: compile failed; previous assembly retained"; Debug::Warning("%s", s.output.c_str()); }
            else
            {
                if (!s.dispatch) { s.dispatch = LoadHost(); s.Call(0, reinterpret_cast<const void*>(&Log)); s.Call(9, reinterpret_cast<const void*>(&DispatchNative)); }
                const auto path = result.assembly.string();
                if (s.Call(1, path.data(), static_cast<int>(path.size())) != 0) throw std::runtime_error("C# assembly validation failed; previous assembly retained.");
                s.stagedAssembly=result.assembly;
                s.pending = true; s.failed = false;
                s.status = playSession ? "C#: compiled; reload pending until Stop" : "C#: compiled";
            }
        }
        if(s.pending && s.playing && s.liveReload && !s.dirty && !s.failed && !s.build.valid()) {
            if(s.Call(10)==0){s.activeAssembly=s.stagedAssembly;s.pending=false;s.status="C#: stateful reload applied";}
            else {s.liveReload=false;s.status="C#: live reload rejected; current generation retained, replacement waits for Stop";}
        }
        if (s.pending && !s.playing && !s.dirty && !s.failed && !s.build.valid())
        {
            if (s.Call(2) != 0) throw std::runtime_error("C# activation failed.");
            if(!s.stagedAssembly.empty())s.activeAssembly=s.stagedAssembly;
            s.pending = false; s.loaded = true; s.status = "C#: ready (reload on save)";
        }
        if (s.dirty && !s.build.valid() && (s.automatic || s.force) && now - s.changedAt > std::chrono::milliseconds(400))
        {
            s.dirty = false; s.force = false; s.failed = false; s.requested = s.observed;
            s.status = "C#: compiling saved assets...";
            const auto directory = s.cache / std::to_string(++s.generation);
            s.build = std::async(std::launch::async, Build, s.requested, directory);
        }
        if (executeCallbacks && playSession) RunGameplay(paused, dt);
    }
    catch (const std::exception& error)
    { s.failed = true; s.status = error.what(); Debug::Warning("%s", s.status.c_str()); }
}
}
