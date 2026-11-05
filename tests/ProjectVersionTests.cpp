#include <Canis/Canis.hpp>
#include <Canis/Yaml.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Canis;
namespace fs = std::filesystem;
static void Check(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
int main()
{
    const auto cwd = fs::current_path();
    const auto temp = fs::temp_directory_path() / ("canis-versions-" + std::to_string(SDL_GetTicksNS()));
    try
    {
        fs::create_directories(temp / "project/project_settings");
        fs::create_directories(temp / "project_settings");
        std::ofstream(temp / "CMakeLists.txt") << "# isolated workspace\n";
        fs::current_path(temp / "project");
        GetProjectConfig() = ProjectConfig{};
        SetEditorRuntimeEnabled(true);
        GetProjectConfig().gameVersion = "1.2.3-beta.4";
        GetProjectConfig().assetVersion = "2026.11.05";
        Check(SaveProjectConfig(), "Version settings failed to save");
        for (const char *path : {"project_settings/project.canis", "../project_settings/project.canis"})
        {
            const auto saved = YAML::LoadFile(path);
            Check(saved["gameVersion"].as<std::string>() == "1.2.3-beta.4", "Game version was not saved");
            Check(saved["assetVersion"].as<std::string>() == "2026.11.05", "Asset version was not saved");
        }
        GetProjectConfig() = ProjectConfig{};
        Init();
        Check(GetProjectConfig().gameVersion == "1.2.3-beta.4" &&
                  GetProjectConfig().assetVersion == "2026.11.05", "Versions did not survive restart");
        std::ofstream("project_settings/project.canis") << "gameName: Legacy project\n";
        Init();
        Check(GetProjectConfig().gameVersion == "0.0.0" && GetProjectConfig().assetVersion == "0.0.0",
              "Legacy project did not get default versions");
        fs::current_path(cwd);
        fs::remove_all(temp);
        std::cout << "Project versions: runtime/source persistence and legacy defaults passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        fs::current_path(cwd);
        fs::remove_all(temp);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
