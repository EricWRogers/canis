#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <Canis/ScriptLanguageClient.hpp>
#include <map>

namespace Canis::Scripting
{
    // Initial desktop editor host. All public methods are main-thread only.
    class CSharpRuntime
    {
    public:
        CSharpRuntime(const std::filesystem::path& assets, const std::filesystem::path& cache);
        ~CSharpRuntime();
        CSharpRuntime(const CSharpRuntime&) = delete;
        CSharpRuntime& operator=(const CSharpRuntime&) = delete;
        void Tick(bool playSession, bool paused, float dt, bool executeCallbacks = true);
        void RunGameplay(bool paused, float dt);
        void StopSession();
        std::string ProjectPath() const;
        void RequestLanguage(const std::string& method, const std::string& path, const std::string& text, int cursor, const std::map<std::string,std::string>& overlays = {});
        std::vector<ScriptEditing::LanguageReply> PollLanguage();
        void ExportPlayer(const std::filesystem::path& destination) const;
        bool HasBuildError() const;
        void SetLiveReload(bool enabled);
        bool LiveReload() const;
        void RequestBuild();
        void RefreshSources();
        bool ReloadOnSave() const;
        void SetReloadOnSave(bool value);
        bool ReadyToPlay() const;
        bool Busy() const;
        const std::string& Status() const;
        const std::string& BuildOutput() const;
        int CollectRetiredContexts();
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
