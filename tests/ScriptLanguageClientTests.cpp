#include <Canis/ScriptLanguageClient.hpp>
#include <Canis/External/tinygltf/json.hpp>
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Canis::ScriptEditing;
using Json = nlohmann::json;
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main()
{
    SDL_Init(0);
    auto root = std::filesystem::temp_directory_path() / ("canis-language-test-" + std::to_string(SDL_GetPerformanceCounter()));
    std::filesystem::create_directories(root);
    try
    {
        const auto path = (root / "truck test.cpp").string();
        std::ofstream(root / "compile_flags.txt") << "-std=c++20\n";
        Check(UriPath(FileUri(path)) == path, "URI round trip with spaces");
        ScriptLanguageClient client(root.string(), root.string());
        Uint64 start = SDL_GetTicks();
        while (!client.Ready() && SDL_GetTicks() - start < 10000) { client.Poll(); SDL_Delay(20); }
        Check(client.Ready(), client.Error().c_str());
        const std::string prefix = "/*" + std::string(150000, 'x') + "*/\n" + "struct Truck { int speed; void Drive(float dt); };\nvoid Truck::Drive(float dt) {}\nvoid Use(){ /* 🚚 */ Truck truck; ";
        auto request = [&](const std::string& method, const std::string& text, int cursor)
        {
            client.Request(method, path, text, cursor);
            Uint64 started = SDL_GetTicks();
            while (SDL_GetTicks() - started < 15000)
            {
                for (const auto& reply : client.Poll()) if (reply.method == method)
                {
                    auto json = Json::parse(reply.json);
                    Check(!json.contains("error"), json.dump().c_str()); return json["result"];
                }
                SDL_Delay(20);
            }
            throw std::runtime_error("Language request timed out: " + method);
        };
        std::string text = prefix + "truck. }";
        auto result = request("textDocument/completion", text, prefix.size() + 6);
        Check(result.dump().find("speed") != std::string::npos && result.dump().find("Drive") != std::string::npos, "member completion");
        text = prefix + "truck.Drive( }";
        result = request("textDocument/signatureHelp", text, prefix.size() + 12);
        Check(result.dump().find("float dt") != std::string::npos, "parameter hint");
        text = prefix + "truck.Drive(1); }";
        result = request("textDocument/definition", text, prefix.size() + 8);
        Check(!result.empty(), "definition location");
        result = request("textDocument/references", text, prefix.size() + 8);
        Check(result.size() >= 2, "references include call and declaration");
        result = request("textDocument/documentSymbol", text, 0);
        Check(result.dump().find("Drive") != std::string::npos, "document symbol list");
        client.Sync(path, "void broken() { missing_name; }");
        bool diagnostic = false; start = SDL_GetTicks();
        while (!diagnostic && SDL_GetTicks() - start < 15000)
        {
            for (const auto& reply : client.Poll())
                if (reply.method == "diagnostics" && reply.json.find("missing_name") != std::string::npos) diagnostic = true;
            SDL_Delay(20);
        }
        Check(diagnostic, "live diagnostics");
        client.Close(path);
        std::cout << "clangd completion, signatures, definition, references, symbols, diagnostics passed\n";
    }
    catch (const std::exception& error)
    { std::cerr << error.what() << '\n'; std::filesystem::remove_all(root); SDL_Quit(); return 1; }
    std::filesystem::remove_all(root); SDL_Quit();
}
