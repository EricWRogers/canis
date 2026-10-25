#include <Canis/EditorRelease.hpp>
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <fstream>
#include <vector>

namespace Canis
{
    EditorRelease::~EditorRelease()
    {
        // Output goes directly to a file, so a running compiler can finish after the editor exits.
        if (m_process) SDL_DestroyProcess(m_process);
    }

    void EditorRelease::Open(const std::filesystem::path& sourceRoot, const std::string& startupScene)
    {
        m_root = sourceRoot;
        m_project = std::filesystem::current_path();
        if (m_folder.empty()) m_folder = (m_root / "Builds" / "Desktop").string();
        if (m_scene.empty()) m_scene = startupScene;
        m_open = true;
    }

    void EditorRelease::Browse(SDL_Window* window)
    {
        // The callback may outlive the editor; it owns only shared picker state, never `this`.
        auto* state = new std::shared_ptr<FolderPicker>(m_picker);
        {
            std::lock_guard lock(m_picker->mutex);
            m_picker->active = true;
        }
        SDL_ShowOpenFolderDialog([](void* userdata, const char* const* files, int)
        {
            std::unique_ptr<std::shared_ptr<FolderPicker>> owner(static_cast<std::shared_ptr<FolderPicker>*>(userdata));
            auto& picker = **owner;
            std::lock_guard lock(picker.mutex);
            picker.active = false;
            if (!files) picker.error = SDL_GetError();
            else if (files[0]) picker.selected = files[0];
        }, state, window, m_folder.c_str(), false);
    }

    void EditorRelease::Start()
    {
        namespace fs = std::filesystem;
        m_error.clear();
        m_output.clear();
        m_finished = m_succeeded = false;
        if (m_root.empty() || !fs::is_regular_file(m_root / "scripts/build-release.cmake"))
        {
            m_error = "Cannot find scripts/build-release.cmake in this project's source folder.";
            return;
        }
        if (m_folder.empty() || m_scene.empty())
        {
            m_error = "Choose a startup scene and a new or empty build folder.";
            return;
        }
        static const char* platforms[] = {"desktop", "web", "desktop-vr", "vr"};
        const auto job = m_root / "build/editor-release/logs";
        std::error_code error;
        fs::create_directories(job, error);
        if (error) { m_error = error.message(); return; }
        // SDL's nanosecond timer is monotonic system-wide; concurrent editor jobs keep separate logs.
        m_logPath = job / ("release-" + std::to_string(SDL_GetTicksNS()) + ".log");
        auto* log = SDL_IOFromFile(m_logPath.string().c_str(), "wb");
        if (!log) { m_error = SDL_GetError(); return; }
        std::vector<std::string> storage = {"cmake",
            "-DPROJECT=" + m_project.string(),
            std::string("-DPLATFORM=") + platforms[m_platform], "-DOUTPUT=" + m_folder, "-DSCENE=" + m_scene,
            "-DJOBS=" + std::to_string(std::clamp(m_jobs, 1, 64)),
            m_csharp ? "-DCSHARP=ON" : "-DCSHARP=OFF",
            m_steamAudio ? "-DSTEAM_AUDIO=ON" : "-DSTEAM_AUDIO=OFF",
            m_development ? "-DDEVELOPMENT=ON" : "-DDEVELOPMENT=OFF",
            "-P", (m_root / "scripts/build-release.cmake").string()};
        std::vector<const char*> args;
        for (const auto& argument : storage) args.push_back(argument.c_str());
        args.push_back(nullptr);
        const auto props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, args.data());
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, m_root.string().c_str());
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, log);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
        m_process = SDL_CreateProcessWithProperties(props);
        if (!m_process) m_error = std::string("Unable to start CMake: ") + SDL_GetError();
        else m_builtFolder = m_folder;
        SDL_DestroyProperties(props);
        SDL_CloseIO(log);
        m_lastPoll = 0;
    }

    void EditorRelease::Poll()
    {
        if (!m_process) return;
        int exitCode = 0;
        if (SDL_WaitProcess(m_process, false, &exitCode))
        {
            SDL_DestroyProcess(m_process);
            m_process = nullptr;
            m_finished = true;
            m_succeeded = exitCode == 0;
        }
        if (!m_finished && SDL_GetTicks() - m_lastPoll < 250) return;
        m_lastPoll = SDL_GetTicks();
        std::ifstream log(m_logPath, std::ios::binary | std::ios::ate);
        if (log)
        {
            const auto size = static_cast<std::streamoff>(log.tellg());
            constexpr std::streamoff limit = 128 * 1024;
            const auto start = std::max<std::streamoff>(0, size - limit);
            log.seekg(start);
            m_output.assign(std::istreambuf_iterator<char>(log), {});
            if (start) m_output.insert(0, "[Showing the last 128 KB; complete output is in the log file.]\n");
        }
    }

    void EditorRelease::Draw(SDL_Window* window, const std::function<bool()>& save)
    {
        namespace fs = std::filesystem;
        Poll();
        if (m_open) { ImGui::OpenPopup("Release Build"); m_open = false; }
        ImGui::SetNextWindowSize(ImVec2(760, 650), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal("Release Build", nullptr)) return;
        bool pickerActive;
        {
            std::lock_guard lock(m_picker->mutex);
            pickerActive = m_picker->active;
            if (!m_picker->selected.empty()) { m_folder = m_picker->selected; m_picker->selected.clear(); }
            if (!m_picker->error.empty()) { m_error = m_picker->error; m_picker->error.clear(); }
        }
        const bool running = m_process != nullptr;
        ImGui::BeginDisabled(running || pickerActive);
        ImGui::SetNextItemWidth(260);
        ImGui::Combo("Platform", &m_platform, "Desktop\0Web\0Desktop VR\0VR\0");
        const char* descriptions[] = {
            "Desktop player for this computer's OS and architecture.",
            "Browser player: HTML, WebAssembly, data and managed runtime. Requires Emscripten; C# also needs .NET wasm-tools.",
            "PC OpenXR player. Starts in headset mode; requires an OpenXR loader and active headset runtime.",
            "Standalone Meta Quest / Android. Unavailable: Canis needs an Android lifecycle, OpenXR GLES/Vulkan backend and ARM64 C# host."};
        ImGui::TextWrapped("%s", descriptions[m_platform]);
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-145);
        ImGui::InputText("Startup scene", &m_scene);
        if (ImGui::BeginCombo("Saved scenes", "Choose a scene..."))
        {
            std::error_code error;
            const auto& project = m_project;
            std::vector<std::string> scenes;
            for (fs::recursive_directory_iterator it(project / "assets", error), end; it != end && !error; it.increment(error))
                if (it->path().extension() == ".scene") scenes.push_back(it->path().lexically_relative(project).generic_string());
            std::sort(scenes.begin(), scenes.end());
            for (const auto& scene : scenes)
                if (ImGui::Selectable(scene.c_str(), scene == m_scene)) m_scene = scene;
            ImGui::EndCombo();
        }
        ImGui::Checkbox("Development build (native debug symbols)", &m_development);
        ImGui::Checkbox("C# scripting (precompile saved scripts)", &m_csharp);
#ifdef __APPLE__
        ImGui::BeginDisabled(true);
#else
        ImGui::BeginDisabled(m_platform == 1);
#endif
        ImGui::Checkbox("Steam Audio spatial sound (desktop only)", &m_steamAudio);
        ImGui::EndDisabled();
#ifdef __APPLE__
        ImGui::TextWrapped("macOS 10.15 releases use stereo spatial panning. The Steam Audio SDK requires C++ runtime symbols unavailable on Catalina.");
#endif
        ImGui::SetNextItemWidth(110);
        ImGui::InputInt("Parallel compile jobs", &m_jobs);
        m_jobs = std::clamp(m_jobs, 1, 64);
        ImGui::TextUnformatted("Build folder");
        ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Browse...").x - 2 * ImGui::GetStyle().FramePadding.x - ImGui::GetStyle().ItemSpacing.x);
        ImGui::InputText("##ReleaseFolder", &m_folder);
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) Browse(window);
        ImGui::TextWrapped("Choose a new or empty build folder. Build saves the active scene and open scripts, then copies all saved runtime assets. Existing files are never overwritten.");
        ImGui::EndDisabled();
        if (!m_error.empty()) ImGui::TextWrapped("%s", m_error.c_str());
        ImGui::Separator();
        ImGui::TextUnformatted(running ? "Building... You can close this dialog; the build will continue." :
            (m_finished ? (m_succeeded ? "Build complete." : "Build failed. See the log below.") : "Ready to build."));
        if (m_succeeded) ImGui::TextWrapped("Output: %s", m_builtFolder.c_str());
        if (!m_logPath.empty()) ImGui::TextWrapped("Log: %s", m_logPath.string().c_str());
        ImGui::BeginChild("ReleaseLog", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 8), true, ImGuiWindowFlags_HorizontalScrollbar);
        const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY();
        ImGui::TextUnformatted(m_output.c_str());
        if (atBottom) ImGui::SetScrollHereY(1);
        ImGui::EndChild();
        ImGui::BeginDisabled(running || pickerActive || m_platform == 3);
        if (ImGui::Button("Build"))
        {
            if (save()) Start();
            else m_error = "Could not save the active scene or scripts. Resolve the save error before building.";
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Copy log")) ImGui::SetClipboardText(m_output.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}
