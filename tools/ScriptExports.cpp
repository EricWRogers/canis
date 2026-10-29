#include <Canis/ScriptSync.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <regex>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;
namespace
{
    std::string Read(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Cannot read " + path.string());
        return {std::istreambuf_iterator<char>(file), {}};
    }
}
int main(int argc, char** argv)
{
    try
    {
        if (argc != 3) throw std::runtime_error("Usage: CanisScriptExports header.hpp source.cpp");
        const fs::path headerPath = argv[1], sourcePath = argv[2];
        const auto header = Read(headerPath), source = Read(sourcePath);
        std::smatch match;
        if (!std::regex_search(header, match, std::regex("ScriptName\\s*=\\s*\"([^\"]+)\""))) return 0;
        auto name = match[1].str();
        if (const auto separator = name.rfind("::"); separator != std::string::npos) name.erase(0, separator + 2);
        // Some legacy types have a separate, custom registration translation unit.
        if (source.find("Register" + name + "Script") == std::string::npos &&
            source.find("Register" + name + "Component") == std::string::npos) return 0;
        const auto result = Canis::ScriptEditing::SynchronizeHeader(header, source, false);
        if (!result.error.empty()) throw std::runtime_error(sourcePath.string() + ": " + result.error);
        if (result.source == source) return 0;
        if (Read(headerPath) != header || Read(sourcePath) != source)
            throw std::runtime_error("File changed during export generation; rebuild to retry.");
        auto temporary = sourcePath;
        temporary += ".canis-exports.tmp";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file << result.source;
            file.close();
            if (!file) throw std::runtime_error("Cannot write " + temporary.string());
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), sourcePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace " + sourcePath.string());
#else
        fs::rename(temporary, sourcePath);
#endif
        std::cout << "Editor exports: " << name << " (" << result.properties << " fields, " << result.actions << " actions)\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
