#pragma once

#include <Canis/CppSyntax.hpp>
#include <Canis/ScriptEditingTools.hpp>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <vector>

namespace Canis::ScriptEditing
{
    struct Document
    {
        std::filesystem::path path;
        std::string text;
        std::string savedText;
        std::string error;
        CppHighlightCache highlight;
        float editorHeight = 0;
        int cursor = 0;
        int selectionStart = 0, selectionFinish = 0;
        std::optional<TextEdit> pendingEdit;
        std::vector<int> foldedLines;
        int jump = -1;
        int selectionEnd = -1;
        bool Dirty() const { return text != savedText; }

        static bool Read(const std::filesystem::path& path, std::string& result)
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) return false;
            result.assign(std::istreambuf_iterator<char>(stream), {});
            return !stream.bad();
        }

        bool Load(const std::filesystem::path& file)
        {
            std::string contents;
            if (!Read(file, contents)) { error = "Cannot read " + file.string(); return false; }
            path = std::filesystem::absolute(file).lexically_normal();
            text = savedText = contents;
            error.clear();
            cursor = 0;
            return true;
        }

        bool Save()
        {
            if (!Dirty()) return true;
            std::string disk;
            if (!Read(path, disk) || disk != savedText)
            {
                error = "File changed or was deleted outside the editor. Copy your edits before reopening the disk version.";
                return false;
            }
            // Write alongside the original, then replace it only after a complete write.
            const auto temporary = std::filesystem::path(path.string() + ".canis-saving");
            std::error_code ec;
            if (std::filesystem::exists(temporary, ec))
            {
                error = "A recovery file already exists: " + temporary.string();
                return false;
            }
            std::ofstream stream(temporary, std::ios::binary);
            stream.write(text.data(), static_cast<std::streamsize>(text.size()));
            stream.close();
            if (!stream)
            {
                std::filesystem::remove(temporary, ec);
                error = "Unable to write script. Original file retained.";
                return false;
            }
            std::filesystem::rename(temporary, path, ec);
            if (ec)
            {
                error = "Unable to replace script: " + ec.message() + ". Edits retained in " + temporary.string();
                return false;
            }
            savedText = text;
            error.clear();
            return true;
        }
    };

    inline int LineOffset(const std::string& text, int line, int column = 1)
    {
        size_t offset = 0;
        for (int i = 1; i < line; ++i)
        {
            const auto newline = text.find('\n', offset);
            if (newline == std::string::npos) return static_cast<int>(text.size());
            offset = newline + 1;
        }
        const auto end = text.find('\n', offset);
        return static_cast<int>(std::min(offset + static_cast<size_t>(std::max(1, column) - 1),
            end == std::string::npos ? text.size() : end));
    }

    struct Diagnostic { std::string path; int line = 1; int column = 1; std::string message; int severity = 1; };
    inline bool ParseDiagnostic(const std::string& text, Diagnostic& out)
    {
        static const std::regex gcc(R"(^(.+):([0-9]+):([0-9]+):\s*(.*(?:error|warning|note):.*)$)");
        static const std::regex gccLine(R"(^(.+):([0-9]+):\s*(.*(?:error|warning|note):.*)$)");
        static const std::regex msvc(R"(^\s*(.+)\(([0-9]+)(?:,([0-9]+))?\)\s*:\s*(.*(?:error|warning).*)$)");
        std::smatch match;
        try
        {
            if (std::regex_match(text, match, gcc) || std::regex_match(text, match, msvc))
            {
                out = {match[1], std::stoi(match[2]), match[3].matched ? std::stoi(match[3]) : 1, match[4]};
                return true;
            }
            if (std::regex_match(text, match, gccLine))
            {
                out = {match[1], std::stoi(match[2]), 1, match[3]};
                return true;
            }
        }
        catch (const std::exception&) { return false; }
        return false;
    }
}
