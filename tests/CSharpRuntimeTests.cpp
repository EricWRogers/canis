#include <Canis/Scripting/CSharpRuntime.hpp>
#include <SDL3/SDL.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

using Canis::Scripting::CSharpRuntime;
namespace fs = std::filesystem;
void Check(bool result, const std::string& message) { if (!result) throw std::runtime_error(message); }
void Write(const fs::path& file, const std::string& text) { std::ofstream stream(file); stream << text; }
std::string Read(const fs::path& file) { std::ifstream stream(file); return {std::istreambuf_iterator<char>(stream), {}}; }
template<class Predicate> void Wait(CSharpRuntime& runtime, Predicate predicate, bool play = false, bool pause = false)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(25);
    do
    {
        runtime.Tick(play, pause, .016f);
        if (predicate()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    } while (std::chrono::steady_clock::now() < deadline);
    throw std::runtime_error(runtime.Status() + "\n" + runtime.BuildOutput());
}
int main()
{
    SDL_Init(0);
    const auto root = fs::temp_directory_path() / ("canis csharp test " + std::to_string(SDL_GetTicksNS()));
    fs::create_directories(root / "assets/Editor");
    const auto script = root / "assets/Demo.cs", trace = root / "trace.txt";
    std::string escaped = trace.generic_string();
    auto source = [&](const std::string& version)
    {
        return "using Canis; using System.IO; public sealed class Demo : GameSystem { static int count; "
            "void Emit(string s) => File.AppendAllText(@\"" + escaped + "\", s + \"\\n\"); "
            "public override void Start() { Emit(\"" + version + "-start-\" + ++count); } "
            "public override void Update(float dt) { Emit(\"" + version + "-update\"); } "
            "public override void Destroy() { Emit(\"" + version + "-stop\"); } }";
    };
    try
    {
        // Editor-only files must not contaminate the runtime compilation.
        Write(root / "assets/Editor/Tool.cs", "invalid runtime source deliberately excluded");
        Write(script, source("v1"));
        {
            CSharpRuntime runtime(root / "assets", root / "cache");
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            runtime.Tick(true, false, .016f);
            Check(Read(trace).find("v1-start-1") != std::string::npos, "initial managed system did not execute");
            const auto pausedTrace = Read(trace);
            runtime.Tick(true, true, .016f);
            Check(Read(trace) == pausedTrace, "Pause invoked Update");
            Write(script, source("v2"));
            Wait(runtime, [&] { return runtime.Status().find("pending until Stop") != std::string::npos; }, true);
            Check(Read(trace).find("v2-start") == std::string::npos, "save replaced executing gameplay before Stop");
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            runtime.Tick(true, false, .016f);
            Check(Read(trace).find("v1-stop") != std::string::npos && Read(trace).find("v2-start-1") != std::string::npos,
                "Stop did not activate the new generation");
            // Invalid save must preserve the live generation, including while playing.
            Write(script, "this is deliberately invalid C#");
            Wait(runtime, [&] { return runtime.Status().find("compile failed") != std::string::npos; }, true);
            Check(!runtime.ReadyToPlay(), "failed generation reported ready");
            const auto size = Read(trace).size(); runtime.Tick(true, false, .016f);
            Check(Read(trace).size() > size, "failed build stopped the previous live assembly");
            Check(runtime.BuildOutput().find(script.string()) != std::string::npos, "diagnostics do not point at original asset");
            Write(script, source("v3"));
            Wait(runtime, [&] { return runtime.Status().find("pending until Stop") != std::string::npos; }, true);
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            runtime.Tick(true, false, .016f);
            Check(Read(trace).find("v3-start-1") != std::string::npos, "fixed source not activated");
            Wait(runtime, [&] { return runtime.ReadyToPlay(); }); // Stop/reset
            runtime.Tick(true, false, .016f);
            Check(Read(trace).find("v3-start-2") == std::string::npos, "Play did not reset script statics");
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            Check(runtime.CollectRetiredContexts() == 0, "old gameplay contexts remained rooted");
            // Multiple saves while a build is running must activate only the newest snapshot.
            Write(script, source("obsolete"));
            runtime.RefreshSources(); runtime.Tick(false, false, 0);
            Check(!runtime.ReadyToPlay(), "immediate Play could bypass a saved change");
            Wait(runtime, [&] { return runtime.Busy(); });
            Write(script, source("newest"));
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            runtime.Tick(true, false, .016f);
            Check(Read(trace).find("newest-start-1") != std::string::npos && Read(trace).find("obsolete-start") == std::string::npos,
                "a stale build replaced the latest source");
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            runtime.SetReloadOnSave(false);
            Write(script, source("manual"));
            for (int i = 0; i < 35; ++i) { runtime.Tick(false, false, 0); SDL_Delay(25); }
            Check(!runtime.Busy() && !runtime.ReadyToPlay(), "disabled auto reload still built changed source");
            runtime.RequestBuild();
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            runtime.Tick(true, false, .016f);
            Check(Read(trace).find("manual-start-1") != std::string::npos, "manual compile did not work");
            runtime.SetReloadOnSave(true);
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            fs::remove(script);
            Wait(runtime, [&] { return runtime.Status().find("compiling") != std::string::npos; });
            Wait(runtime, [&] { return runtime.ReadyToPlay(); });
            const auto before = Read(trace); runtime.Tick(true, false, .016f);
            Check(Read(trace) == before, "deleted script still ran");
        }
        fs::remove_all(root); SDL_Quit();
        std::cout << "C# host/save/reload/lifetime checks passed\n"; return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << "\nFixtures: " << root << '\n';
        SDL_Quit(); return 1;
    }
}
