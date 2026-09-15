#include <Canis/ReloadLibraryBackup.hpp>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static std::string Read(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

int main()
{
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / ("canis-reload-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    try
    {
        const auto live = root / "game.so";
        const auto backup = root / "game.so.reload.bak";
        { std::ofstream file(live); file << "original library"; }
        const auto timestamp = fs::file_time_type::clock::now() - std::chrono::hours(1);
        fs::last_write_time(live, timestamp);
        std::string error;
        Check(Canis::ReloadBuild::CopyLibrary(live, backup, error), "backup failed");
        Check(Read(live) == "original library", "backup removed or modified live output");
        Check(fs::last_write_time(live) == timestamp, "backup changed build output timestamp");
        Check(fs::last_write_time(backup) == timestamp, "backup must preserve timestamp");

        // A failed link can leave either a partial file or no output at all.
        { std::ofstream file(live); file << "partial link"; }
        Check(Canis::ReloadBuild::CopyLibrary(backup, live, error), "partial output restore failed");
        Check(Read(live) == "original library", "restore lost original bytes");
        Check(fs::last_write_time(live) == timestamp, "restore must not hide changed dependencies");
        fs::remove(live);
        Check(Canis::ReloadBuild::CopyLibrary(backup, live, error), "missing output restore failed");
        Check(Read(live) == "original library", "missing output restore lost bytes");

        Check(!Canis::ReloadBuild::CopyLibrary(live, root / "missing" / "backup", error),
              "backup failure must be reported");
        Check(Read(live) == "original library", "failed backup damaged live output");
        Check(!Canis::ReloadBuild::CopyLibrary(root / "absent", backup, error),
              "missing source must be reported");
        Check(Read(backup) == "original library", "failed copy damaged recovery backup");
        fs::remove_all(root);
        std::cout << "Reload library backup tests passed\n";
    }
    catch (const std::exception& error)
    {
        fs::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
