#pragma once
#include <Canis/CppSyntax.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace Canis::ScriptEditing
{
    struct TextEdit { int begin = 0, end = 0; std::string text; int cursor = 0; };
    inline TextEdit IndentLines(const std::string& text, int start, int end, bool unindent, bool comment)
    {
        start = std::clamp(start, 0, (int)text.size());
        end = std::clamp(end, start, (int)text.size());
        const auto previous = start ? text.rfind('\n', start - 1) : std::string::npos;
        int begin = previous == std::string::npos ? 0 : (int)previous + 1;
        if (end > start && text[end - 1] == '\n') --end;
        auto finish = text.find('\n', end);
        if (finish == std::string::npos) finish = text.size();
        std::string result;
        bool allCommented = true;
        for (size_t p = begin; p <= finish;)
        {
            auto n = std::min(text.find('\n', p), finish);
            auto q = text.find_first_not_of(" \t", p);
            if (q < n && text.compare(q, 2, "//") != 0) allCommented = false;
            if (n == finish) break; p = n + 1;
        }
        for (size_t p = begin; p <= finish;)
        {
            auto n = std::min(text.find('\n', p), finish);
            std::string line = text.substr(p, n - p);
            auto q = line.find_first_not_of(" \t");
            if (comment)
            {
                if (q != std::string::npos)
                {
                    if (allCommented) line.erase(q, line.compare(q, 3, "// ") == 0 ? 3 : 2);
                    else line.insert(q, "// ");
                }
            }
            else if (unindent)
            {
                int count = 0; while (count < (int)line.size() && count < 4 && line[count] == ' ') ++count;
                if (!line.empty() && line[0] == '\t') count = 1;
                line.erase(0, count);
            }
            else line.insert(0, "    ");
            result += line;
            if (n == finish) break;
            result += '\n'; p = n + 1;
        }
        return {begin, (int)finish, result, begin + (int)result.size()};
    }
    inline bool InLiteral(const std::string& text, int offset)
    {
        for (const auto& span : HighlightCpp(text))
            if (span.begin <= (size_t)offset && (size_t)offset < span.end)
                return span.color == CppColor::String || span.color == CppColor::Comment;
        return false;
    }
    inline int MatchingBracket(const std::string& text, int offset)
    {
        if (offset < 0 || offset >= (int)text.size() || InLiteral(text, offset)) return -1;
        const std::string opens = "({[", closes = ")}]";
        auto kind = opens.find(text[offset]); int direction = 1;
        if (kind == std::string::npos) { kind = closes.find(text[offset]); direction = -1; }
        if (kind == std::string::npos) return -1;
        auto spans = HighlightCpp(text); std::vector<bool> ignored(text.size(), false);
        for (const auto& span : spans)
            if (span.color == CppColor::String || span.color == CppColor::Comment)
                std::fill(ignored.begin() + span.begin, ignored.begin() + span.end, true);
        int depth = 0;
        for (int p = offset; p >= 0 && p < (int)text.size(); p += direction)
        {
            if (ignored[p]) continue;
            if (text[p] == opens[kind]) depth += direction;
            if (text[p] == closes[kind]) depth -= direction;
            if (!depth) return p;
        }
        return -1;
    }
    inline TextEdit ReplaceMatches(const std::string& text, const std::string& needle,
                                  const std::string& replacement, int begin, int end, bool all)
    {
        begin = std::clamp(begin, 0, (int)text.size()); end = std::clamp(end, begin, (int)text.size());
        std::string range = text.substr(begin, end - begin);
        if (!needle.empty()) for (size_t p = 0; (p = range.find(needle, p)) != std::string::npos;)
        { range.replace(p, needle.size(), replacement); p += replacement.size(); if (!all) break; }
        return {begin, end, range, begin + (int)range.size()};
    }
}
