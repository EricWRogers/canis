#pragma once
#include <memory>
#include <string>
#include <vector>
namespace Canis::ScriptEditing
{
    struct LanguageReply { std::string method, path, snapshot, json; };
    class ScriptLanguageClient
    {
    public:
        ScriptLanguageClient(const std::string& root, const std::string& build);
        ~ScriptLanguageClient();
        bool Ready() const;
        std::string Error() const;
        void Close(const std::string& path);
        void Sync(const std::string& path, const std::string& text);
        void Request(const std::string& method, const std::string& path, const std::string& text, int cursor);
        std::vector<LanguageReply> Poll();
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
    std::string FileUri(const std::string& path);
    std::string UriPath(const std::string& uri);
}
