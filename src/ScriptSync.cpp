#include <Canis/ScriptSync.hpp>
#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <utility>

namespace Canis::ScriptEditing
{
    namespace
    {
        struct Token { std::string value; size_t begin, end; };
        using Tokens = std::vector<Token>;
        bool Identifier(const std::string& value)
        {
            return !value.empty() && (std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_');
        }
        Tokens Lex(const std::string& text)
        {
            Tokens out;
            for (size_t i = 0; i < text.size();)
            {
                const size_t start = i;
                if (std::isspace(static_cast<unsigned char>(text[i]))) { ++i; continue; }
                if (text.compare(i, 2, "//") == 0)
                { i = text.find('\n', i); if (i == std::string::npos) break; continue; }
                if (text.compare(i, 2, "/*") == 0)
                {
                    i = text.find("*/", i + 2);
                    if (i == std::string::npos) throw std::runtime_error("Unclosed comment.");
                    i += 2; continue;
                }
                if (text.compare(i, 2, "R\"") == 0)
                {
                    const auto paren = text.find('(', i + 2);
                    if (paren == std::string::npos) throw std::runtime_error("Unclosed raw string.");
                    const auto end = text.find(")" + text.substr(i + 2, paren - i - 2) + "\"", paren);
                    if (end == std::string::npos) throw std::runtime_error("Unclosed raw string.");
                    i = end + (paren - start - 2) + 2;
                }
                else if (text[i] == '"' || text[i] == '\'')
                {
                    const char quote = text[i++];
                    bool closed = false;
                    while (i < text.size())
                    {
                        if (text[i] == '\\') { i += 2; continue; }
                        if (text[i++] == quote) { closed = true; break; }
                    }
                    if (!closed) throw std::runtime_error("Unclosed string literal.");
                }
                else if (text[i] == '#')
                {
                    do { i = text.find('\n', i + 1); }
                    while (i != std::string::npos && i > 0 && text[i - 1] == '\\');
                    if (i == std::string::npos) i = text.size();
                }
                else if (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')
                {
                    while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) ++i;
                }
                else
                {
                    ++i;
                    for (const char* pair : {"::", "&&", "->", "[[", "]]"})
                        if (text.compare(start, 2, pair) == 0) { ++i; break; }
                }
                out.push_back({text.substr(start, i - start), start, i});
            }
            return out;
        }
        size_t Match(const Tokens& tokens, size_t start, const std::string& open, const std::string& close)
        {
            int depth = 0;
            for (size_t i = start; i < tokens.size(); ++i)
            {
                if (tokens[i].value == open) ++depth;
                if (tokens[i].value == close && --depth == 0) return i;
                // Only decrement on a closing token (the expression above short-circuits).
            }
            throw std::runtime_error("Unbalanced " + open + close + ". Finish the declaration before saving.");
        }
        std::string Join(const Tokens& tokens, size_t first, size_t end)
        {
            std::string result;
            for (size_t i = first; i < end; ++i) result += (result.empty() ? "" : " ") + tokens[i].value;
            return result;
        }
        bool Contains(const Tokens& tokens, const std::string& value)
        {
            return std::any_of(tokens.begin(), tokens.end(), [&](const auto& t) { return t.value == value; });
        }
        std::string Compact(std::string value)
        {
            std::erase_if(value, [](unsigned char c) { return std::isspace(c); });
            return value;
        }
        std::vector<Tokens> Parameters(const Tokens& tokens, size_t first, size_t end)
        {
            std::vector<Tokens> result;
            size_t start = first;
            int angle = 0, paren = 0, brace = 0;
            for (size_t i = first; i <= end; ++i)
            {
                if (i == end || (tokens[i].value == "," && angle == 0 && paren == 0 && brace == 0))
                {
                    if (i > start) result.emplace_back(tokens.begin() + start, tokens.begin() + i);
                    start = i + 1;
                    continue;
                }
                const auto& v = tokens[i].value;
                if (v == "<") ++angle; if (v == ">") --angle;
                if (v == "(") ++paren; if (v == ")") --paren;
                if (v == "{") ++brace; if (v == "}") --brace;
            }
            for (auto& param : result)
            {
                // Defaults are only legal in the declaration; remove them in definitions.
                const auto equal = std::find_if(param.begin(), param.end(), [](const auto& t) { return t.value == "="; });
                param.erase(equal, param.end());
            }
            return result;
        }
        std::string ParameterKey(std::vector<Tokens> params)
        {
            std::string key;
            for (auto& param : params)
            {
                if (param.size() == 1 && param[0].value == "void") continue;
                static const std::set<std::string> typeWords = {"int", "float", "double", "bool", "char", "short", "long", "unsigned", "signed", "void"};
                if (param.size() >= 2 && Identifier(param.back().value) && !typeWords.count(param.back().value) &&
                    param[param.size() - 2].value != "::" &&
                    !(param.size() == 2 && (param[0].value == "const" || param[0].value == "volatile")))
                    param.pop_back();
                // Normalize const T& and T const& spellings for signature matching.
                const auto indirection = std::find_if(param.begin(), param.end(), [](const auto& t) { return t.value == "*" || t.value == "&" || t.value == "&&"; });
                const auto prefixEnd = static_cast<size_t>(indirection - param.begin());
                Tokens cv, base;
                for (size_t i = 0; i < prefixEnd; ++i)
                    (param[i].value == "const" || param[i].value == "volatile" ? cv : base).push_back(param[i]);
                cv.insert(cv.end(), base.begin(), base.end());
                cv.insert(cv.end(), param.begin() + prefixEnd, param.end());
                param = std::move(cv);
                // Top-level cv on value parameters does not distinguish overloads.
                if (!Contains(param, "*") && !Contains(param, "&") && !Contains(param, "&&"))
                    std::erase_if(param, [](const auto& t) { return t.value == "const" || t.value == "volatile"; });
                key += Compact(Join(param, 0, param.size())) + ";";
            }
            return key;
        }
        std::string Qualifiers(const Tokens& tokens, size_t first, size_t end)
        {
            std::string out;
            for (size_t i = first; i < end && tokens[i].value != "->"; ++i)
                if (tokens[i].value == "const" || tokens[i].value == "volatile" || tokens[i].value == "&" || tokens[i].value == "&&")
                    out += tokens[i].value;
            return out;
        }
        struct Method { std::string name, key, definition; bool returnsValue = false; };
        const std::set<std::string> propertyTypes = {"bool", "int", "float", "double", "std::string",
            "char", "short", "long", "longlong", "unsigned", "unsignedint", "unsignedshort", "unsignedlong", "unsignedlonglong",
            "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64",
            "std::int32_t", "std::uint32_t", "std::int64_t", "std::uint64_t",
            "glm::vec2", "glm::vec3", "glm::vec4", "glm::quat",
            "Vector2", "Vector3", "Vector4", "Quaternion", "Color", "Mask", "Entity",
            "Canis::Vector2", "Canis::Vector3", "Canis::Vector4", "Canis::Quaternion", "Canis::Color", "Canis::Mask", "Canis::Entity",
            "AudioAssetHandle", "SceneAssetHandle", "ShaderGraphAssetHandle", "AnimationClipAssetHandle", "AnimatorControllerAssetHandle", "TerrainAssetHandle",
            "Canis::AudioAssetHandle", "Canis::SceneAssetHandle", "Canis::ShaderGraphAssetHandle", "Canis::AnimationClipAssetHandle", "Canis::AnimatorControllerAssetHandle", "Canis::TerrainAssetHandle"};
    }

    SyncResult SynchronizeHeader(const std::string& header, const std::string& original)
    {
        SyncResult result; result.source = original;
        try
        {
            const Tokens h = Lex(header);
            std::string className, qualifiedName;
            size_t body = 0, bodyEnd = 0;
            bool isStruct = false;
            for (size_t i = 0; i + 2 < h.size(); ++i)
            {
                if ((h[i].value != "class" && h[i].value != "struct") || !Identifier(h[i + 1].value)) continue;
                size_t open = i + 2;
                while (open < h.size() && h[open].value != "{" && h[open].value != ";") ++open;
                if (open == h.size() || h[open].value != "{") continue;
                const auto end = Match(h, open, "{", "}");
                for (size_t j = open + 1; j + 2 < end; ++j)
                    if (h[j].value == "ScriptName" && h[j + 1].value == "=" && h[j + 2].value.front() == '"')
                    {
                        if (!className.empty()) throw std::runtime_error("Sync supports one ScriptName class per header.");
                        if ((i > 0 && h[i - 1].value == ">") || h[i + 2].value == "<")
                            throw std::runtime_error("Template script classes require manual sync.");
                        className = h[i + 1].value;
                        qualifiedName = h[j + 2].value.substr(1, h[j + 2].value.size() - 2);
                        body = open; bodyEnd = end; isStruct = h[i].value == "struct";
                    }
                i = end;
            }
            if (className.empty()) return result;
            const auto separator = qualifiedName.rfind("::");
            if ((separator == std::string::npos ? qualifiedName : qualifiedName.substr(separator + 2)) != className)
                throw std::runtime_error("ScriptName must match the class name for header sync.");
            const std::string nameSpace = separator == std::string::npos ? "" : qualifiedName.substr(0, separator);
            for (size_t i = 0; i < h.size(); ++i)
                if ((i > body && i < bodyEnd && h[i].value.front() == '#') ||
                    h[i].value.starts_with("#if") || h[i].value.starts_with("#else") || h[i].value.starts_with("#elif")) throw std::runtime_error("Conditional/preprocessor declarations require manual sync.");

            std::vector<std::string> properties;
            std::vector<Method> methods;
            bool publicAccess = isStruct;
            for (size_t i = body + 1; i < bodyEnd;)
            {
                if (i + 1 < bodyEnd && h[i + 1].value == ":" &&
                    (h[i].value == "public" || h[i].value == "private" || h[i].value == "protected"))
                { publicAccess = h[i].value == "public"; i += 2; continue; }
                if (h[i].value == ";") { ++i; continue; }
                const size_t start = i;
                size_t paren = bodyEnd;
                bool initializer = false;
                bool inlineBody = false;
                while (i < bodyEnd && h[i].value != ";")
                {
                    if (h[i].value == "=") initializer = true;
                    if (h[i].value == "(")
                    {
                        if (paren == bodyEnd && !initializer) paren = i;
                        i = Match(h, i, "(", ")") + 1; continue;
                    }
                    if (h[i].value == "{")
                    {
                        i = Match(h, i, "{", "}") + 1;
                        if (paren != bodyEnd) { inlineBody = true; break; }
                        continue;
                    }
                    ++i;
                }
                const size_t end = i;
                if (i == bodyEnd && !inlineBody) throw std::runtime_error("Missing semicolon in script class.");
                if (i < bodyEnd && h[i].value == ";") ++i;
                Tokens member(h.begin() + start, h.begin() + end);
                if (inlineBody || Contains(member, "using") || Contains(member, "typedef") || Contains(member, "friend") ||
                    Contains(member, "class") || Contains(member, "struct") || Contains(member, "enum")) continue;
                if (Contains(member, "template") || Contains(member, "[[") || Contains(member, "operator"))
                { result.notes.push_back("Skipped a template, attribute, or operator declaration."); continue; }
                if (paren != bodyEnd && paren > start)
                {
                    const std::string methodName = h[paren - 1].value;
                    const size_t close = Match(h, paren, "(", ")");
                    const Tokens suffixTokens(h.begin() + close + 1, h.begin() + end);
                    if (!Identifier(methodName) || methodName == className || Contains(suffixTokens, "=") ||
                        Contains(member, "constexpr") || Contains(member, "consteval")) continue;
                    auto params = Parameters(h, paren + 1, close);
                    bool complex = false;
                    std::string arguments;
                    for (const auto& param : params)
                    {
                        if (Contains(param, "(") || Contains(param, "[") || Contains(param, ".")) complex = true;
                        arguments += (arguments.empty() ? "" : ", ") + Join(param, 0, param.size());
                    }
                    Tokens returnTokens(h.begin() + start, h.begin() + paren - 1);
                    std::erase_if(returnTokens, [](const auto& t) { return t.value == "virtual" || t.value == "static" || t.value == "inline"; });
                    const std::string returns = Join(returnTokens, 0, returnTokens.size());
                    std::string suffix;
                    for (size_t j = close + 1; j < end; ++j)
                    {
                        if (h[j].value == "override" || h[j].value == "final") continue;
                        if (h[j].value != "const" && h[j].value != "volatile" && h[j].value != "&" && h[j].value != "&&" && h[j].value != "noexcept") complex = true;
                        suffix += " " + h[j].value;
                    }
                    if (complex || returns.empty() || returns == "auto" || (returns != "void" && Contains(suffixTokens, "noexcept")))
                    { result.notes.push_back("Skipped complex method " + methodName + "."); continue; }
                    Method method;
                    method.name = methodName;
                    method.key = methodName + "(" + ParameterKey(params) + ")" + Qualifiers(h, close + 1, end);
                    method.returnsValue = returns != "void";
                    method.definition = method.returnsValue ? "auto " + className + "::" + methodName + "(" + arguments + ")" + suffix + " -> " + returns :
                        "void " + className + "::" + methodName + "(" + arguments + ")" + suffix;
                    methods.push_back(method);
                }
                else if (publicAccess && !Contains(member, "static") && !Contains(member, "const") && !Contains(member, "constexpr"))
                {
                    // Support comma-separated fields and both = and brace initializers.
                    const auto fields = Parameters(member, 0, member.size());
                    std::string type;
                    for (size_t field = 0; field < fields.size(); ++field)
                    {
                        const auto& declaration = fields[field];
                        size_t stop = 0;
                        while (stop < declaration.size() && declaration[stop].value != "{") ++stop;
                        if (!stop || !Identifier(declaration[stop - 1].value)) continue;
                        if (field == 0)
                        {
                            type = Compact(Join(declaration, 0, stop - 1));
                            if (stop > 2 && !propertyTypes.count(type) && propertyTypes.count(declaration.front().value) &&
                                !Contains(declaration, "*") && !Contains(declaration, "&") && !Contains(declaration, "["))
                                throw std::runtime_error("Cannot parse public field; check its type and semicolon.");
                        }
                        const auto name = declaration[stop - 1].value;
                        if (propertyTypes.count(type) && (field == 0 || stop == 1)) properties.push_back(name);
                        else result.notes.push_back("Inspector sync skipped unsupported member " + name + ".");
                    }
                }
            }

            // Replace only our marked registration block. Manual registrations stay byte-for-byte intact.
            const std::string beginMarker = "// CANIS AUTO PROPERTIES BEGIN " + qualifiedName;
            const std::string endMarker = "// CANIS AUTO PROPERTIES END " + qualifiedName;
            auto source = original;
            const auto protectedTokens = Lex(source);
            const auto markerPosition = [&](const std::string& marker, size_t from = 0)
            {
                for (size_t line = from; line < source.size();)
                {
                    const auto end = source.find('\n', line);
                    const auto first = source.find_first_not_of(" \t\r", line);
                    if (first != std::string::npos && first < (end == std::string::npos ? source.size() : end) &&
                        source.compare(first, marker.size(), marker) == 0 &&
                        std::none_of(protectedTokens.begin(), protectedTokens.end(), [&](const auto& t) { return first >= t.begin && first < t.end; }))
                    {
                        const auto after = source.find_first_not_of(" \t\r", first + marker.size());
                        if (after == std::string::npos || after == end) return first;
                    }
                    if (end == std::string::npos) break;
                    line = end + 1;
                }
                return std::string::npos;
            };
            const auto block = markerPosition(beginMarker);
            if (block != std::string::npos)
            {
                const auto end = markerPosition(endMarker, block);
                if (markerPosition(beginMarker, block + beginMarker.size()) != std::string::npos)
                    throw std::runtime_error("Duplicate auto-property blocks in source.");
                if (end == std::string::npos) throw std::runtime_error("Incomplete auto-property block in source.");
                const auto lineStart = source.rfind('\n', block);
                const auto eraseStart = lineStart == std::string::npos ? 0 : lineStart;
                source.erase(eraseStart, end + endMarker.size() - eraseStart);
            }
            Tokens s = Lex(source);
            size_t registrationBody = s.size(), registrationEnd = s.size();
            std::string config;
            for (size_t i = 0; i + 2 < s.size(); ++i)
                if ((s[i].value == "Register" + className + "Script" || s[i].value == "Register" + className + "Component") && s[i + 1].value == "(")
                {
                    const auto close = Match(s, i + 1, "(", ")");
                    if (close + 1 < s.size() && s[close + 1].value == "{")
                    { registrationBody = close + 1; registrationEnd = Match(s, registrationBody, "{", "}"); break; }
                }
            std::set<std::string> manualProperties;
            if (registrationBody < s.size())
            {
                for (size_t i = registrationBody + 1; i + 3 < registrationEnd; ++i)
                {
                    if (s[i].value == "DEFAULT_CONFIG" || s[i].value == "DEFAULT_CONFIG_AND_REQUIRED" ||
                        s[i].value == "DEFAULT_COMPONENT_CONFIG" || s[i].value == "DEFAULT_COMPONENT_CONFIG_AND_REQUIRED")
                        config = s[i + 2].value;
                    if (s[i].value == "REGISTER_PROPERTY" && s[i + 1].value == "(")
                    {
                        const auto close = Match(s, i + 1, "(", ")");
                        const auto args = Parameters(s, i + 2, close);
                        if (args.size() == 3)
                        {
                            const auto owner = Compact(Join(args[1], 0, args[1].size()));
                            if (owner == qualifiedName || owner == className)
                                manualProperties.insert(Compact(Join(args[2], 0, args[2].size())));
                        }
                    }
                }
            }
            if (!properties.empty() && (registrationBody == s.size() || config.empty()))
                throw std::runtime_error("Cannot find the template Register" + className + "Script / DEFAULT_CONFIG function. No source was changed.");
            std::string registrations;
            for (const auto& property : properties)
                if (!manualProperties.count(property))
                { registrations += "    REGISTER_PROPERTY(" + config + ", " + qualifiedName + ", " + property + ");\n"; ++result.properties; }
            if (!registrations.empty())
                source.insert(s[registrationBody].end, "\n    " + beginMarker + "\n" + registrations + "    " + endMarker);

            std::set<std::string> existing;
            for (size_t i = 2; i + 1 < s.size(); ++i)
            {
                if (s[i - 1].value != "::" || s[i - 2].value != className || s[i + 1].value != "(") continue;
                const auto close = Match(s, i + 1, "(", ")");
                size_t end = close + 1;
                while (end < s.size() && s[end].value != "{" && s[end].value != ";") ++end;
                if (end == s.size() || s[end].value != "{") continue;
                existing.insert(s[i].value + "(" + ParameterKey(Parameters(s, i + 2, close)) + ")" + Qualifiers(s, close + 1, end));
            }
            std::string definitions;
            bool needsException = false;
            for (const auto& method : methods)
                if (existing.insert(method.key).second)
                {
                    ++result.methods;
                    definitions += "\n" + method.definition + "\n{\n";
                    if (method.returnsValue)
                    {
                        needsException = true;
                        definitions += "    // TODO: implement this method before calling it.\n    throw std::logic_error(\"Unimplemented: " + qualifiedName + "::" + method.name + "\");\n";
                    }
                    definitions += "}\n";
                }
            if (!definitions.empty())
            {
                if (!nameSpace.empty()) definitions = "\nnamespace " + nameSpace + "\n{\n" + definitions + "}\n";
                source += definitions;
            }
            if (needsException && source.find("#include <stdexcept>") == std::string::npos)
                source = "#include <stdexcept>\n" + source;
            result.source = source;
        }
        catch (const std::exception& error) { result.error = error.what(); result.source = original; result.methods = result.properties = 0; }
        return result;
    }
}
