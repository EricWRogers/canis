#include <Canis/ScriptLanguageClient.hpp>
#include <Canis/External/tinygltf/json.hpp>
#include <SDL3/SDL.h>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <thread>
#include <map>
#include <filesystem>

namespace Canis::ScriptEditing
{
using Json = nlohmann::json;
std::string FileUri(const std::string& path)
{
    std::string uri = "file://";
    auto absolute = std::filesystem::absolute(path).generic_string();
    if (absolute.empty() || absolute[0] != '/') uri += '/';
    const char* hex = "0123456789ABCDEF";
    for (unsigned char c : absolute)
        if (std::isalnum(c) || c == '/' || c == ':' || c == '-' || c == '_' || c == '.' || c == '~') uri += c;
        else { uri += '%'; uri += hex[c >> 4]; uri += hex[c & 15]; }
    return uri;
}
std::string UriPath(const std::string& uri)
{
    if (uri.rfind("file://", 0) != 0) return {};
    std::string path;
    for (size_t i = 7; i < uri.size(); ++i)
    {
        if (uri[i] == '%' && i + 2 < uri.size())
        { try { path += static_cast<char>(std::stoi(uri.substr(i + 1, 2), nullptr, 16)); i += 2; } catch (...) { return {}; } }
        else path += uri[i];
    }
#ifdef _WIN32
    if (path.size() > 2 && path[0] == '/' && path[2] == ':') path.erase(0, 1);
#endif
    return path;
}
struct ScriptLanguageClient::Impl
{
    SDL_Process* process = nullptr;
    std::thread reader, writer;
    std::condition_variable outgoingReady;
    std::deque<std::string> outgoing;
    std::atomic<bool> stopped{false}, ready{false};
    std::mutex mutex;
    std::string error;
    std::vector<LanguageReply> replies;
    std::map<int, LanguageReply> pending;
    std::map<std::string, std::pair<std::string, int>> documents;
    struct Queued { std::string method, path, text; int cursor; };
    std::vector<Queued> queued;
    int nextId = 1;
    void Send(const Json& json)
    {
        if (!process || stopped) return;
        const std::string body = json.dump();
        const std::string message = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        { std::scoped_lock lock(mutex); outgoing.push_back(message); }
        outgoingReady.notify_one();
    }
    void Write()
    {
        while (!stopped)
        {
            std::string message;
            {
                std::unique_lock lock(mutex);
                outgoingReady.wait(lock, [&] { return stopped || !outgoing.empty(); });
                if (stopped) break;
                message = std::move(outgoing.front()); outgoing.pop_front();
            }
            auto* input = SDL_GetProcessInput(process);
            size_t offset = 0;
            while (!stopped && offset < message.size())
            {
                const auto written = SDL_WriteIO(input, message.data() + offset, message.size() - offset);
                offset += written;
                if (!written)
                {
                    if (SDL_GetIOStatus(input) == SDL_IO_STATUS_NOT_READY) { SDL_Delay(5); continue; }
                    std::scoped_lock lock(mutex);
                    error = "clangd connection closed"; ready = false; stopped = true;
                }
            }
            if (!stopped) SDL_FlushIO(input);
        }
    }
    void Read()
    {
        std::string buffer; char chunk[8192];
        while (!stopped)
        {
            auto* output = SDL_GetProcessOutput(process);
            size_t count = SDL_ReadIO(output, chunk, sizeof(chunk));
            if (!count)
            {
                if (SDL_GetIOStatus(output) == SDL_IO_STATUS_NOT_READY) { SDL_Delay(10); continue; }
                break;
            }
            buffer.append(chunk, count);
            while (true)
            {
                const auto end = buffer.find("\r\n\r\n"); if (end == std::string::npos) break;
                const auto length = buffer.find("Content-Length: "); if (length == std::string::npos) { stopped = true; break; }
                size_t size = 0;
                try { size = std::stoull(buffer.substr(length + 16)); } catch (...) { stopped = true; break; }
                if (size > 32 * 1024 * 1024) { stopped = true; break; }
                if (buffer.size() < end + 4 + size) break;
                auto json = Json::parse(buffer.substr(end + 4, size), nullptr, false);
                buffer.erase(0, end + 4 + size);
                if (json.is_discarded()) continue;
                std::scoped_lock lock(mutex);
                if (json.contains("id") && json["id"].is_number_integer())
                {
                    auto item = pending.find(json["id"].get<int>());
                    if (item != pending.end())
                    {
                        auto reply = item->second; reply.json = json.dump();
                        replies.push_back(std::move(reply)); pending.erase(item);
                    }
                }
                else if (json.value("method", "") == "textDocument/publishDiagnostics")
                    replies.push_back({"diagnostics", UriPath(json["params"].value("uri", "")), "", json["params"].dump()});
            }
        }
        if (!stopped) { std::scoped_lock lock(mutex); error = "clangd stopped; reopen the editor to reconnect"; ready = false; }
    }
};
ScriptLanguageClient::ScriptLanguageClient(const std::string& root, const std::string& build) : impl(std::make_unique<Impl>())
{
#ifndef __EMSCRIPTEN__
    const std::string directory = "--compile-commands-dir=" + build;
    const char* args[] = {"clangd", directory.c_str(), "--background-index", "--header-insertion=never", "--log=error", "-j=2", nullptr};
    impl->process = SDL_CreateProcess(args, true);
    if (!impl->process) { impl->error = "Install clangd on PATH for C++ language features: " + std::string(SDL_GetError()); return; }
    impl->pending[0] = {"initialize", "", "", ""};
    impl->reader = std::thread([this] { impl->Read(); });
    impl->writer = std::thread([this] { impl->Write(); });
    Json capabilities;
    capabilities["general"]["positionEncodings"] = Json::array({"utf-8"});
    capabilities["textDocument"]["completion"]["completionItem"]["snippetSupport"] = false;
    Json params = {{"processId", nullptr}, {"rootUri", FileUri(root)}, {"capabilities", capabilities}};
    impl->Send({{"jsonrpc", "2.0"}, {"id", 0}, {"method", "initialize"}, {"params", params}});
#endif
}
ScriptLanguageClient::~ScriptLanguageClient()
{
    impl->stopped = true;
    impl->outgoingReady.notify_all();
    if (impl->process) SDL_KillProcess(impl->process, true);
    if (impl->reader.joinable()) impl->reader.join();
    if (impl->writer.joinable()) impl->writer.join();
    if (impl->process) SDL_DestroyProcess(impl->process);
}
bool ScriptLanguageClient::Ready() const { return impl->ready; }
std::string ScriptLanguageClient::Error() const { std::scoped_lock lock(impl->mutex); return impl->error; }
std::vector<LanguageReply> ScriptLanguageClient::Poll()
{
    std::vector<LanguageReply> replies;
    { std::scoped_lock lock(impl->mutex); replies.swap(impl->replies); }
    for (auto& reply : replies) if (reply.method == "initialize")
    {
        auto json = Json::parse(reply.json);
        if (json.contains("error")) { std::scoped_lock lock(impl->mutex); impl->error = json["error"].dump(); }
        else { impl->Send({{"jsonrpc", "2.0"}, {"method", "initialized"}, {"params", Json::object()}}); impl->ready = true; }
    }
    if (Ready())
    {
        auto queued = std::move(impl->queued); impl->queued.clear();
        for (const auto& request : queued) Request(request.method, request.path, request.text, request.cursor);
    }
    return replies;
}
void ScriptLanguageClient::Close(const std::string& path)
{
    if (!impl->documents.erase(path)) return;
    impl->Send({{"jsonrpc", "2.0"}, {"method", "textDocument/didClose"},
        {"params", {{"textDocument", {{"uri", FileUri(path)}}}}}});
}
void ScriptLanguageClient::Sync(const std::string& path, const std::string& text)
{
    if (!Ready()) return;
    auto it = impl->documents.find(path);
    if (it == impl->documents.end())
    {
        impl->documents[path] = {text, 1};
        impl->Send({{"jsonrpc", "2.0"}, {"method", "textDocument/didOpen"}, {"params", {{"textDocument", {
            {"uri", FileUri(path)}, {"languageId", "cpp"}, {"version", 1}, {"text", text}}}}}});
    }
    else if (it->second.first != text)
    {
        it->second.first = text; ++it->second.second;
        impl->Send({{"jsonrpc", "2.0"}, {"method", "textDocument/didChange"}, {"params", {
            {"textDocument", {{"uri", FileUri(path)}, {"version", it->second.second}}},
            {"contentChanges", Json::array({{{"text", text}}})}}}});
    }
}
void ScriptLanguageClient::Request(const std::string& method, const std::string& path, const std::string& text, int cursor)
{
    if (!Ready())
    {
        if (impl->queued.size() < 20) impl->queued.push_back({method, path, text, cursor});
        return;
    }
    Sync(path, text);
    cursor = std::clamp(cursor, 0, (int)text.size());
    const auto previous = cursor ? text.rfind('\n', cursor - 1) : std::string::npos;
    Json params = {{"textDocument", {{"uri", FileUri(path)}}}};
    if (method != "textDocument/documentSymbol") params["position"] = {
        {"line", std::count(text.begin(), text.begin() + cursor, '\n')},
        {"character", cursor - (previous == std::string::npos ? 0 : (int)previous + 1)}};
    if (method == "textDocument/references") params["context"] = {{"includeDeclaration", true}};
    const int id = impl->nextId++;
    { std::scoped_lock lock(impl->mutex); impl->pending[id] = {method, path, text, ""}; }
    impl->Send({{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}});
}
}
