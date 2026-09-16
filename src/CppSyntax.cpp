#include <Canis/CppSyntax.hpp>
#include <cctype>
#include <unordered_set>

namespace Canis::ScriptEditing
{
    namespace
    {
        bool IdentifierStart(unsigned char c) { return std::isalpha(c) || c == '_' || c >= 128; }
        bool IdentifierPart(unsigned char c) { return IdentifierStart(c) || std::isdigit(c); }
        const std::unordered_set<std::string> keywords = {
            "alignas", "alignof", "and", "and_eq", "asm", "auto", "bitand", "bitor", "break", "case", "catch",
            "class", "compl", "concept", "const", "consteval", "constexpr", "constinit", "const_cast", "continue",
            "co_await", "co_return", "co_yield", "decltype", "default", "delete", "do", "dynamic_cast", "else", "enum",
            "explicit", "export", "extern", "false", "final", "for", "friend", "goto", "if", "inline", "mutable",
            "namespace", "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq", "override",
            "private", "protected", "public", "register", "reinterpret_cast", "requires", "return", "sizeof",
            "static", "static_assert", "static_cast", "struct", "switch", "template", "this", "thread_local",
            "throw", "true", "try", "typedef", "typeid", "typename", "union", "using", "virtual", "volatile",
            "while", "xor", "xor_eq"};
        const std::unordered_set<std::string> types = {
            "bool", "char", "char8_t", "char16_t", "char32_t", "double", "float", "int", "long", "short", "signed",
            "unsigned", "void", "wchar_t", "string", "string_view", "vector", "array", "map", "unordered_map",
            "unique_ptr", "shared_ptr", "optional", "size_t", "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64"};
    }

    static std::vector<CppSpan> HighlightCode(const std::string& text, bool csharp)
    {
        static const std::unordered_set<std::string> csKeywords = {
            "abstract", "as", "async", "await", "base", "break", "case", "catch", "checked", "class", "const", "continue",
            "default", "delegate", "do", "else", "enum", "event", "explicit", "extern", "false", "finally", "fixed", "for",
            "foreach", "get", "global", "if", "implicit", "in", "init", "interface", "internal", "is", "lock", "namespace",
            "new", "null", "operator", "out", "override", "params", "partial", "private", "protected", "public", "readonly",
            "record", "ref", "required", "return", "scoped", "sealed", "set", "sizeof", "stackalloc", "static", "struct",
            "switch", "this", "throw", "true", "try", "typeof", "unchecked", "unsafe", "using", "virtual", "volatile",
            "when", "where", "while", "with", "yield"};
        static const std::unordered_set<std::string> csTypes = {
            "bool", "byte", "char", "decimal", "double", "dynamic", "float", "int", "long", "nint", "nuint", "object",
            "sbyte", "short", "string", "uint", "ulong", "ushort", "var", "void"};
        std::vector<CppSpan> spans;
        bool lineStart = true;
        bool includePath = false;
        for (size_t i = 0; i < text.size();)
        {
            const size_t start = i;
            CppColor color = CppColor::Text;
            if (text[i] == '\n') { ++i; lineStart = true; includePath = false; }
            else if (std::isspace(static_cast<unsigned char>(text[i]))) ++i;
            else if (text.compare(i, 2, "//") == 0)
            {
                i = text.find('\n', i);
                if (i == std::string::npos) i = text.size();
                color = CppColor::Comment;
            }
            else if (text.compare(i, 2, "/*") == 0)
            {
                const auto end = text.find("*/", i + 2);
                i = end == std::string::npos ? text.size() : end + 2;
                if (text.find('\n', start) < i) { lineStart = true; includePath = false; }
                color = CppColor::Comment;
            }
            else if (lineStart && text[i] == '#')
            {
                ++i;
                while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
                const auto directive = i;
                while (i < text.size() && IdentifierPart(text[i])) ++i;
                const auto word = text.substr(directive, i - directive);
                includePath = word == "include" || word == "include_next" || word == "import";
                color = CppColor::Preprocessor;
                lineStart = false;
            }
            else if (includePath && text[i] == '<')
            {
                ++i;
                while (i < text.size() && text[i] != '>' && text[i] != '\n') ++i;
                if (i < text.size() && text[i] == '>') ++i;
                color = CppColor::String;
                includePath = false;
            }
            else if (csharp && (text.compare(i, 2, "@\"") == 0 || text.compare(i, 3, "$@\"") == 0 || text.compare(i, 3, "@$\"") == 0))
            {
                i = text.find('"', i) + 1;
                while (i < text.size())
                {
                    if (text[i++] == '"')
                    {
                        if (i < text.size() && text[i] == '"') { ++i; continue; }
                        break;
                    }
                }
                color = CppColor::String; lineStart = false;
            }
            else if (csharp && text.compare(i, 3, "\"\"\"") == 0)
            {
                size_t count = 3;
                while (i + count < text.size() && text[i + count] == '"') ++count;
                const auto end = text.find(std::string(count, '"'), i + count);
                i = end == std::string::npos ? text.size() : end + count;
                color = CppColor::String; lineStart = false;
            }
            else
            {
                lineStart = false;
                size_t quote = i;
                bool raw = false;
                for (const char* prefix : {"u8R\"", "uR\"", "UR\"", "LR\"", "R\"", "u8\"", "u\"", "U\"", "L\"", "u8'", "u'", "U'", "L'"})
                {
                    const std::string p(prefix);
                    if (text.compare(i, p.size(), p) == 0)
                    { quote = i + p.size() - 1; raw = p.find('R') != std::string::npos; break; }
                }
                if (text[quote] == '"' || text[quote] == '\'')
                {
                    if (raw)
                    {
                        const auto paren = text.find('(', quote + 1);
                        if (paren == std::string::npos || paren - quote > 17) i = text.size();
                        else
                        {
                            const auto ending = ")" + text.substr(quote + 1, paren - quote - 1) + "\"";
                            const auto end = text.find(ending, paren + 1);
                            i = end == std::string::npos ? text.size() : end + ending.size();
                        }
                    }
                    else
                    {
                        i = quote + 1;
                        while (i < text.size())
                        {
                            if (text[i] == '\\') { i += (i + 1 < text.size() ? 2 : 1); continue; }
                            if (text[i] == '\n') break;
                            if (text[i++] == text[quote]) break;
                        }
                    }
                    color = CppColor::String;
                    includePath = false;
                }
                else if (std::isdigit(static_cast<unsigned char>(text[i])) ||
                    (text[i] == '.' && i + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[i + 1]))))
                {
                    ++i;
                    while (i < text.size())
                    {
                        const char c = text[i];
                        if (IdentifierPart(c) || c == '.' || c == '\'') { ++i; continue; }
                        if ((c == '+' || c == '-') && (text[i - 1] == 'e' || text[i - 1] == 'E' || text[i - 1] == 'p' || text[i - 1] == 'P')) { ++i; continue; }
                        break;
                    }
                    color = CppColor::Number;
                }
                else if (IdentifierStart(text[i]))
                {
                    ++i;
                    while (i < text.size() && IdentifierPart(text[i])) ++i;
                    const auto word = text.substr(start, i - start);
                    size_t next = i;
                    while (next < text.size() && std::isspace(static_cast<unsigned char>(text[next]))) ++next;
                    bool uppercase = word.size() > 1;
                    for (unsigned char c : word) if (std::islower(c)) uppercase = false;
                    if ((csharp ? csKeywords : keywords).count(word)) color = CppColor::Keyword;
                    else if ((csharp ? csTypes : types).count(word)) color = CppColor::Type;
                    else if (uppercase) color = CppColor::Preprocessor;
                    else if (next < text.size() && text[next] == '(') color = CppColor::Function;
                    else if (std::isupper(static_cast<unsigned char>(word[0]))) color = CppColor::Type;
                }
                else ++i;
            }
            if (!spans.empty() && spans.back().color == color) spans.back().end = i;
            else spans.push_back({start, i, color});
        }
        return spans;
    }

    std::vector<CppSpan> HighlightCpp(const std::string& text) { return HighlightCode(text, false); }
    std::vector<CppSpan> HighlightCSharp(const std::string& text) { return HighlightCode(text, true); }

    void CppHighlightCache::Update(const std::string& source, bool isCSharp)
    {
        if (text == source && csharp == isCSharp && !lines.empty()) return;
        text = source;
        csharp = isCSharp;
        spans = csharp ? HighlightCSharp(text) : HighlightCpp(text);
        brackets.assign(text.size(), -1);
        std::vector<size_t> stack;
        for (const auto& span : spans)
        {
            if (span.color == CppColor::Comment || span.color == CppColor::String) continue;
            for (size_t i = span.begin; i < span.end; ++i)
            {
                if (text[i] == '(' || text[i] == '{' || text[i] == '[') stack.push_back(i);
                else if (text[i] == ')' || text[i] == '}' || text[i] == ']')
                {
                    if (stack.empty()) continue;
                    const auto open = stack.back();
                    if ((text[open] == '(' && text[i] == ')') || (text[open] == '{' && text[i] == '}') || (text[open] == '[' && text[i] == ']'))
                    { brackets[open] = (int)i; brackets[i] = (int)open; stack.pop_back(); }
                }
            }
        }
        lines = {0};
        for (size_t i = 0; i < text.size(); ++i) if (text[i] == '\n') lines.push_back(i + 1);
    }
}
