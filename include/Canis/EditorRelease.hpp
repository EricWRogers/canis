#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

struct SDL_Process;
struct SDL_Window;

namespace Canis
{
    // A release job runs outside the editor. Its log survives closing the dialog/editor.
    class EditorRelease
    {
    public:
        ~EditorRelease();
        void Open(const std::filesystem::path& sourceRoot, const std::string& startupScene);
        void Draw(SDL_Window* window, const std::function<bool()>& save);

    private:
        struct FolderPicker
        {
            std::mutex mutex;
            bool active = false;
            std::string selected;
            std::string error;
        };
        void Start();
        void Poll();
        void Browse(SDL_Window* window);
        std::filesystem::path m_root;
        std::filesystem::path m_project;
        std::filesystem::path m_logPath;
        std::shared_ptr<FolderPicker> m_picker = std::make_shared<FolderPicker>();
        SDL_Process* m_process = nullptr;
        std::string m_folder;
        std::string m_scene;
        std::string m_output;
        std::string m_error;
        std::string m_builtFolder;
        int m_platform = 0;
        int m_jobs = 8;
        bool m_development = false;
        bool m_csharp = true;
#ifdef __APPLE__
        bool m_steamAudio = false;
#else
        bool m_steamAudio = true;
#endif
        bool m_open = false;
        bool m_finished = false;
        bool m_succeeded = false;
        unsigned long long m_lastPoll = 0;
    };
}
