#include <Canis/ScriptSync.hpp>
#include <algorithm>
#include <cctype>
#include <set>
#include <map>
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
        std::string LeadingIndent(const std::string& text, size_t position)
        {
            const auto previousLine = position == 0 ? std::string::npos : text.rfind('\n', position - 1);
            const size_t start = previousLine == std::string::npos ? 0 : previousLine + 1;
            const auto end = text.find_first_not_of(" \t", start);
            return text.substr(start, (end == std::string::npos ? text.size() : end) - start);
        }
        std::string RegistrationIndent(const std::string& text, const Tokens& tokens, size_t body, size_t end)
        {
            const auto base = LeadingIndent(text, tokens[body].begin);
            if (body + 1 < end)
            {
                const auto first = LeadingIndent(text, tokens[body + 1].begin);
                if (first.starts_with(base) && first.size() > base.size()) return first;
            }

            // Compact function bodies have no statement indentation to copy.
            // Infer a step from the surrounding file, then fall back to four spaces.
            std::string step;
            for (size_t line = 0; line < text.size();)
            {
                const auto indent = LeadingIndent(text, line);
                const size_t content = line + indent.size();
                if (content < text.size() && text[content] != '\r' && text[content] != '\n' && !indent.empty() &&
                    (step.empty() || indent.size() < step.size())) step = indent;
                const auto next = text.find('\n', line);
                if (next == std::string::npos) break;
                line = next + 1;
            }
            return base + (step.empty() ? "    " : step);
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
        std::string Spell(const Tokens& tokens)
        {
            std::string result, previous;
            for (const auto& token : tokens)
            {
                const auto& value = token.value;
                const bool adjacent = value == "::" || value == "<" || value == ">" || value == "," || value == ")" ||
                    value == "}" || value == "]" || value == "&" || value == "*" || value == "{" ||
                    previous == "::" || previous == "<" || previous == "(" || previous == "{" || previous == "[";
                if (!result.empty() && !adjacent) result += " ";
                result += value; previous = value;
            }
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
        size_t RuntimeDefinitionsPosition(const std::string& text, const Tokens& tokens, size_t registration, const std::string& config)
        {
            if (registration == tokens.size()) return text.size();
            size_t anchor = registration > 0 ? registration - 1 : registration;
            for (size_t i = 1; i < registration; ++i)
                if (tokens[i].value == config && (tokens[i - 1].value == "ScriptConf" ||
                    tokens[i - 1].value == "ComponentConf" || tokens[i - 1].value == "SystemConf"))
                { anchor = i - 1; break; }
            // Insert a separate namespace block before the enclosing declaration,
            // so method stubs never follow (or enter) the configuration section.
            for (size_t i = 0; i < anchor; ++i)
                if (tokens[i].value == "namespace")
                {
                    size_t open = i + 1;
                    while (open < anchor && tokens[open].value != "{" && tokens[open].value != ";") ++open;
                    if (open < anchor && tokens[open].value == "{" && Match(tokens, open, "{", "}") > anchor)
                    { anchor = i; break; }
                }
            const auto line = text.rfind('\n', tokens[anchor].begin);
            return line == std::string::npos ? 0 : line + 1;
        }
        struct ExportPlacement
        {
            size_t position;
            std::string closeNamespaces, reopenNamespaces;
        };
        ExportPlacement PlaceExportsBeforeConfiguration(const std::string& text, const std::string& className, const std::string& config)
        {
            const auto tokens = Lex(text);
            size_t registration = tokens.size();
            for (size_t i = 0; i + 1 < tokens.size(); ++i)
                if ((tokens[i].value == "Register" + className + "Script" || tokens[i].value == "Register" + className + "Component") && tokens[i + 1].value == "(")
                {
                    const auto close = Match(tokens, i + 1, "(", ")");
                    if (close + 1 < tokens.size() && tokens[close + 1].value == "{") { registration = i; break; }
                }
            if (registration == tokens.size()) throw std::runtime_error("Cannot locate the export registration section.");
            size_t anchor = registration > 0 ? registration - 1 : registration;
            for (size_t i = 1; i < registration; ++i)
                if (tokens[i].value == config && (tokens[i - 1].value == "ScriptConf" || tokens[i - 1].value == "ComponentConf" || tokens[i - 1].value == "SystemConf"))
                { anchor = i - 1; break; }
            while (anchor > 0 && tokens[anchor - 1].value != ";" && tokens[anchor - 1].value != "{" &&
                   tokens[anchor - 1].value != "}" && !tokens[anchor - 1].value.starts_with("#")) --anchor;

            struct NamespaceScope { size_t start, open, close; bool anonymous; };
            std::vector<NamespaceScope> scopes;
            for (size_t i = 0; i < anchor; ++i)
            {
                if (tokens[i].value != "namespace" || (i > 0 && tokens[i - 1].value == "using")) continue;
                size_t open = i + 1;
                while (open < anchor && (Identifier(tokens[open].value) || tokens[open].value == "::")) ++open;
                if (open >= anchor || tokens[open].value != "{") continue;
                const auto close = Match(tokens, open, "{", "}");
                if (close > anchor) scopes.push_back({i > 0 && tokens[i - 1].value == "inline" ? i - 1 : i, open, close, open == i + 1});
            }
            // Keep an anonymous config namespace intact as part of the section.
            for (const auto& scope : scopes)
                if (scope.anonymous) { anchor = scope.start; break; }
            std::erase_if(scopes, [&](const auto& scope) { return scope.open >= anchor; });
            const auto lineStart = [&](size_t token)
            {
                const auto previous = text.rfind('\n', tokens[token].begin);
                const size_t line = previous == std::string::npos ? 0 : previous + 1;
                return text.find_first_not_of(" \t", line) == tokens[token].begin ? line : tokens[token].begin;
            };
            if (scopes.empty()) return {lineStart(anchor), {}, {}};

            // A namespace already containing only registration needs no split.
            size_t cursor = scopes.front().open + 1;
            bool onlyNamespaces = true;
            for (size_t i = 1; i < scopes.size(); ++i)
            {
                if (cursor != scopes[i].start) { onlyNamespaces = false; break; }
                cursor = scopes[i].open + 1;
            }
            if (onlyNamespaces && cursor == anchor) return {lineStart(scopes.front().start), {}, {}};

            ExportPlacement placement{lineStart(anchor), {}, {}};
            for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
                placement.closeNamespaces += LeadingIndent(text, tokens[it->start].begin) + "}\n";
            for (const auto& scope : scopes)
                placement.reopenNamespaces += LeadingIndent(text, tokens[scope.start].begin) +
                    text.substr(tokens[scope.start].begin, tokens[scope.open].end - tokens[scope.start].begin) + "\n";
            return placement;
        }
        struct Binding
        {
            std::string name, key, label, parameterNames;
            size_t begin = 0, end = 0;
            bool hasSignature = false;
        };
        std::vector<Binding> ReadBindings(const std::string& text, const std::string& className, const std::string& qualifiedName, bool actions)
        {
            const auto tokens = Lex(text);
            std::vector<Binding> result;
            for (size_t i = 0; i + 1 < tokens.size(); ++i)
            {
                if (tokens[i].value != (actions ? "RegisterSignalAction" : "REGISTER_PROPERTY") || tokens[i + 1].value != "(") continue;
                const auto close = Match(tokens, i + 1, "(", ")");
                const auto args = Parameters(tokens, i + 2, close);
                if (args.size() < 3) continue;
                Binding binding;
                binding.begin = tokens[i].begin;
                binding.end = tokens[close].end;
                if (close + 1 < tokens.size() && tokens[close + 1].value == ";") binding.end = tokens[close + 1].end;
                if (!actions)
                {
                    const auto owner = Compact(Join(args[1], 0, args[1].size()));
                    if (owner != className && owner != qualifiedName) continue;
                    binding.name = Compact(Join(args[2], 0, args[2].size()));
                }
                else
                {
                    const auto& pointer = args[2];
                    // Reference parameters may contain '&' before the actual
                    // member address in a static_cast. The address is last.
                    size_t amp = pointer.size();
                    while (amp > 0 && pointer[amp - 1].value != "&") --amp;
                    if (amp == 0) continue;
                    --amp;
                    size_t ownerEnd = amp;
                    for (size_t j = amp + 1; j + 1 < pointer.size(); ++j)
                        if (pointer[j].value == "::" && Identifier(pointer[j + 1].value)) { binding.name = pointer[j + 1].value; ownerEnd = j; }
                    if (binding.name.empty()) continue;
                    const auto owner = Compact(Join(pointer, amp + 1, ownerEnd));
                    if (owner != className && owner != qualifiedName) continue;
                    binding.label = Join(args[1], 0, args[1].size());
                    if (args.size() > 3) binding.parameterNames = Spell(args[3]);
                    if (Contains(pointer, "static_cast"))
                    {
                        size_t end = amp;
                        while (end > 0 && pointer[end - 1].value != ")") --end;
                        if (end > 0)
                        {
                            size_t open = end - 1;
                            int depth = 1;
                            while (open > 0 && depth)
                            { --open; if (pointer[open].value == ")") ++depth; if (pointer[open].value == "(") --depth; }
                            if (!depth) { binding.key = ParameterKey(Parameters(pointer, open + 1, end - 1)); binding.hasSignature = true; }
                        }
                    }
                }
                result.push_back(std::move(binding));
            }
            return result;
        }
        std::string CanonicalKey(std::string key, bool usingCanis)
        {
            if (usingCanis)
                for (size_t p = 0; (p = key.find("Canis::", p)) != std::string::npos;) key.erase(p, 7);
            return key;
        }
        void EraseBinding(std::string& source, size_t begin, size_t end)
        {
            const auto previous = begin == 0 ? std::string::npos : source.rfind('\n', begin - 1);
            const size_t line = previous == std::string::npos ? 0 : previous + 1;
            if (source.find_first_not_of(" \t", line) == begin) begin = line;
            const auto next = source.find_first_not_of(" \t\r", end);
            if (next != std::string::npos && source[next] == '\n') end = next + 1;
            source.erase(begin, end - begin);
        }
        struct Method
        {
            std::string name, key, definition, parameterTypes, qualifiers;
            std::vector<std::string> parameterNames;
            bool returnsValue = false, exported = false, inlineBody = false;
        };
        struct Field { std::string name, type; };
        const std::set<std::string> propertyTypes = {"bool", "int", "float", "double", "std::string",
            "char", "short", "long", "longlong", "unsigned", "unsignedint", "unsignedshort", "unsignedlong", "unsignedlonglong",
            "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64", "f32", "f64",
            "std::int32_t", "std::uint32_t", "std::int64_t", "std::uint64_t",
            "glm::vec2", "glm::vec3", "glm::vec4", "glm::quat",
            "Vector2", "Vector3", "Vector4", "Quaternion", "Color", "Mask", "Entity",
            "Canis::Vector2", "Canis::Vector3", "Canis::Vector4", "Canis::Quaternion", "Canis::Color", "Canis::Mask", "Canis::Entity",
            "AudioAssetHandle", "SceneAssetHandle", "ShaderGraphAssetHandle", "AnimationClipAssetHandle", "AnimatorControllerAssetHandle", "TerrainAssetHandle",
            "Canis::AudioAssetHandle", "Canis::SceneAssetHandle", "Canis::ShaderGraphAssetHandle", "Canis::AnimationClipAssetHandle", "Canis::AnimatorControllerAssetHandle", "Canis::TerrainAssetHandle"};
        bool IsPropertyType(const std::string& type)
        {
            if ((type.starts_with("std::vector<") || type.starts_with("std::array<")) && type.ends_with(">"))
            {
                auto element = type.substr(type.find('<') + 1);
                element = element.substr(0, element.find_first_of(",>"));
                return IsPropertyType(element) || element == "Canis::TagId" || element == "TagId";
            }
            return propertyTypes.count(type) ||
                ((type.starts_with("Canis::Signal<") || type.starts_with("Signal<")) && type.ends_with(">"));
        }
    }

    SyncResult SynchronizeHeader(const std::string& header, const std::string& original, bool generateMethodStubs)
    {
        SyncResult result; result.source = original;
        try
        {
            const Tokens h = Lex(header);
            std::string className, qualifiedName;
            size_t body = 0, bodyEnd = 0;
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
                        body = open; bodyEnd = end;
                    }
                i = end;
            }
            if (className.empty()) return result;
            if (!generateMethodStubs && !Contains(h, "CANIS_EDITOR_EXPORT") &&
                original.find("// CANIS AUTO PROPERTIES BEGIN ") == std::string::npos &&
                original.find("// CANIS EDITOR EXPORTS BEGIN ") == std::string::npos) return result;
            const auto separator = qualifiedName.rfind("::");
            if ((separator == std::string::npos ? qualifiedName : qualifiedName.substr(separator + 2)) != className)
                throw std::runtime_error("ScriptName must match the class name for header sync.");
            const std::string nameSpace = separator == std::string::npos ? "" : qualifiedName.substr(0, separator);
            for (size_t i = 0; i < h.size(); ++i)
                if ((i > body && i < bodyEnd && h[i].value.front() == '#') ||
                    h[i].value.starts_with("#if") || h[i].value.starts_with("#else") || h[i].value.starts_with("#elif")) throw std::runtime_error("Conditional/preprocessor declarations require manual sync.");

            std::vector<Field> properties;
            std::vector<Method> methods;
            for (size_t i = body + 1; i < bodyEnd;)
            {
                if (i + 1 < bodyEnd && h[i + 1].value == ":" &&
                    (h[i].value == "public" || h[i].value == "private" || h[i].value == "protected"))
                { i += 2; continue; }
                if (h[i].value == ";") { ++i; continue; }
                const bool exported = h[i].value == "CANIS_EDITOR_EXPORT";
                if (exported) ++i;
                const size_t start = i;
                size_t paren = bodyEnd;
                size_t inlineOpen = bodyEnd;
                bool initializer = false;
                int templateDepth = 0;
                bool inlineBody = false;
                while (i < bodyEnd && h[i].value != ";")
                {
                    if (h[i].value == "=") initializer = true;
                    if (!initializer && h[i].value == "<") ++templateDepth;
                    if (!initializer && h[i].value == ">" && templateDepth > 0) --templateDepth;
                    if (h[i].value == "(")
                    {
                        if (paren == bodyEnd && !initializer && templateDepth == 0) paren = i;
                        i = Match(h, i, "(", ")") + 1; continue;
                    }
                    if (h[i].value == "{")
                    {
                        const auto open = i;
                        i = Match(h, i, "{", "}") + 1;
                        if (paren != bodyEnd) { inlineBody = true; inlineOpen = open; break; }
                        continue;
                    }
                    ++i;
                }
                const size_t end = inlineBody ? inlineOpen : i;
                if (i == bodyEnd && !inlineBody) throw std::runtime_error("Missing semicolon in script class.");
                if (i < bodyEnd && h[i].value == ";") ++i;
                Tokens member(h.begin() + start, h.begin() + end);
                if (Contains(member, "CANIS_EDITOR_EXPORT")) throw std::runtime_error("Finish the previous declaration before CANIS_EDITOR_EXPORT.");
                if (Contains(member, "using") || Contains(member, "typedef") || Contains(member, "friend") ||
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
                        arguments += (arguments.empty() ? "" : ", ") + Spell(param);
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
                    {
                        if (exported) throw std::runtime_error("Unsupported CANIS_EDITOR_EXPORT method signature: " + methodName);
                        result.notes.push_back("Skipped complex method " + methodName + "."); continue;
                    }
                    Method method;
                    method.name = methodName;
                    method.key = methodName + "(" + ParameterKey(params) + ")" + Qualifiers(h, close + 1, end);
                    method.returnsValue = returns != "void";
                    method.exported = exported;
                    method.inlineBody = inlineBody;
                    method.qualifiers = suffix;
                    for (auto param : params)
                    {
                        if (param.size() == 1 && param[0].value == "void") continue;
                        std::string parameterName = "argument" + std::to_string(method.parameterNames.size() + 1);
                        if (param.size() >= 2 && Identifier(param.back().value) && param[param.size() - 2].value != "::")
                        { parameterName = param.back().value; param.pop_back(); }
                        while (!parameterName.empty() && parameterName.front() == '_') parameterName.erase(parameterName.begin());
                        method.parameterNames.push_back(parameterName);
                        method.parameterTypes += (method.parameterTypes.empty() ? "" : ", ") + Spell(param);
                    }
                    if (exported && (method.returnsValue || Contains(member, "static") || !suffix.empty()))
                        throw std::runtime_error("CANIS_EDITOR_EXPORT methods must be non-static, non-const void actions: " + methodName);
                    method.definition = method.returnsValue ? "auto " + className + "::" + methodName + "(" + arguments + ")" + suffix + " -> " + returns :
                        "void " + className + "::" + methodName + "(" + arguments + ")" + suffix;
                    methods.push_back(method);
                }
                else if (exported && !Contains(member, "static") && !Contains(member, "const") && !Contains(member, "constexpr"))
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
                            if (stop > 2 && !IsPropertyType(type) && IsPropertyType(declaration.front().value) &&
                                !Contains(declaration, "*") && !Contains(declaration, "&") && !Contains(declaration, "["))
                                throw std::runtime_error("Cannot parse public field; check its type and semicolon.");
                        }
                        const auto name = declaration[stop - 1].value;
                        if (field == 0 || stop == 1) properties.push_back({name, type});
                    }
                }
            }

            // Replace only our marked registration block. Manual registrations stay byte-for-byte intact.
            const bool usingCanis = Compact(original).find("usingnamespaceCanis;") != std::string::npos || nameSpace == "Canis";
            const auto authoredActions = ReadBindings(original, className, qualifiedName, true);
            const auto authoredProperties = ReadBindings(original, className, qualifiedName, false);
            const std::string beginMarker = "// CANIS AUTO PROPERTIES BEGIN " + qualifiedName;
            const std::string endMarker = "// CANIS AUTO PROPERTIES END " + qualifiedName;
            auto source = original;
            const auto markerPosition = [&](const std::string& marker, size_t from = 0)
            {
                const auto protectedTokens = Lex(source);
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
            const auto removeBlock = [&](const std::string& begin, const std::string& finish)
            {
                const auto block = markerPosition(begin);
                if (block == std::string::npos) return;
                const auto end = markerPosition(finish, block);
                if (markerPosition(begin, block + begin.size()) != std::string::npos)
                    throw std::runtime_error("Duplicate generated export blocks in source.");
                if (end == std::string::npos) throw std::runtime_error("Incomplete generated export block in source.");
                const auto previous = source.rfind('\n', block);
                size_t eraseStart = previous == std::string::npos ? 0 : previous;
                size_t eraseEnd = end + finish.size();
                if (begin.starts_with("// CANIS EDITOR EXPORT DECLARATION"))
                {
                    eraseStart = previous == std::string::npos ? 0 : previous + 1;
                    for (int i = 0; i < 2 && eraseEnd < source.size() && source[eraseEnd] == '\n'; ++i) ++eraseEnd;
                }
                else if (begin.starts_with("// CANIS EDITOR EXPORTS") && eraseEnd < source.size() && source[eraseEnd] == '\n') ++eraseEnd;
                source.erase(eraseStart, eraseEnd - eraseStart);
            };
            removeBlock(beginMarker, endMarker);
            removeBlock("// CANIS EDITOR EXPORT DECLARATION BEGIN " + qualifiedName, "// CANIS EDITOR EXPORT DECLARATION END " + qualifiedName);
            removeBlock("// CANIS EDITOR EXPORTS BEGIN " + qualifiedName, "// CANIS EDITOR EXPORTS END " + qualifiedName);
            // The registration call is recognizable without wrapping comments.
            // Keep legacy marker support above so older sources migrate cleanly.
            {
                const auto tokens = Lex(source);
                std::vector<Binding> calls;
                for (size_t i = 0; i + 4 < tokens.size(); ++i)
                {
                    if (tokens[i].value != "Canis" || tokens[i + 1].value != "::" ||
                        tokens[i + 2].value != "EditorExports" || tokens[i + 3].value != "<") continue;
                    const auto close = Match(tokens, i + 3, "<", ">");
                    if (Compact(Join(tokens, i + 4, close)) != qualifiedName || close + 3 >= tokens.size() ||
                        tokens[close + 1].value != "::" || tokens[close + 2].value != "Register" || tokens[close + 3].value != "(") continue;
                    const auto end = Match(tokens, close + 3, "(", ")");
                    if (end + 1 < tokens.size() && tokens[end + 1].value == ";")
                    {
                        Binding call;
                        call.begin = tokens[i].begin; call.end = tokens[end + 1].end;
                        calls.push_back(call);
                    }
                }
                for (auto it = calls.rbegin(); it != calls.rend(); ++it) EraseBinding(source, it->begin, it->end);
            }
            // Remove the obsolete commented example, without touching literals.
            {
                const auto tokens = Lex(source);
                std::vector<std::pair<size_t, size_t>> comments;
                for (size_t line = 0; line < source.size();)
                {
                    const auto newline = source.find('\n', line);
                    const size_t end = newline == std::string::npos ? source.size() : newline;
                    const auto first = source.find_first_not_of(" \t\r", line);
                    if (first != std::string::npos && first < end && source.compare(first, 21, "// REGISTER_PROPERTY(") == 0 &&
                        source.substr(first, end - first).find("exampleProperty);") != std::string::npos &&
                        std::none_of(tokens.begin(), tokens.end(), [&](const auto& token) { return first >= token.begin && first < token.end; }))
                        comments.emplace_back(line, newline == std::string::npos ? end : end + 1);
                    if (newline == std::string::npos) break;
                    line = newline + 1;
                }
                for (auto it = comments.rbegin(); it != comments.rend(); ++it) source.erase(it->first, it->second - it->first);
            }
            const auto exportedMethod = [&](const Binding& action)
            {
                return std::any_of(methods.begin(), methods.end(), [&](const auto& method) {
                    const auto args = method.key.substr(method.key.find('(') + 1);
                    const auto key = args.substr(0, args.find(')'));
                    return method.exported && method.name == action.name && (!action.hasSignature ||
                        CanonicalKey(key, usingCanis) == CanonicalKey(action.key, usingCanis));
                });
            };
            size_t migrateBegin = source.size(), migrateEnd = source.size();
            const auto migrateTokens = Lex(source);
            for (size_t i = 0; i + 1 < migrateTokens.size(); ++i)
                if ((migrateTokens[i].value == "Register" + className + "Script" || migrateTokens[i].value == "Register" + className + "Component") && migrateTokens[i + 1].value == "(")
                {
                    const auto close = Match(migrateTokens, i + 1, "(", ")");
                    if (close + 1 < migrateTokens.size() && migrateTokens[close + 1].value == "{")
                    { migrateBegin = migrateTokens[close + 1].begin; migrateEnd = migrateTokens[Match(migrateTokens, close + 1, "{", "}")].end; break; }
                }
            std::vector<Binding> migrated;
            for (const auto& binding : ReadBindings(source, className, qualifiedName, false))
                if (binding.begin >= migrateBegin && binding.end <= migrateEnd && std::any_of(properties.begin(), properties.end(), [&](const auto& field) { return field.name == binding.name; })) migrated.push_back(binding);
            for (const auto& binding : ReadBindings(source, className, qualifiedName, true))
                if (binding.begin >= migrateBegin && binding.end <= migrateEnd && exportedMethod(binding)) migrated.push_back(binding);
            std::sort(migrated.begin(), migrated.end(), [](const auto& a, const auto& b) { return a.begin > b.begin; });
            for (const auto& binding : migrated) EraseBinding(source, binding.begin, binding.end);
            Tokens s = Lex(source);
            size_t registrationBody = s.size(), registrationEnd = s.size(), registrationFunction = s.size(), registrationConfig = s.size();
            std::string config;
            for (size_t i = 0; i + 2 < s.size(); ++i)
                if ((s[i].value == "Register" + className + "Script" || s[i].value == "Register" + className + "Component") && s[i + 1].value == "(")
                {
                    const auto close = Match(s, i + 1, "(", ")");
                    if (close + 1 < s.size() && s[close + 1].value == "{")
                    { registrationFunction = i; registrationBody = close + 1; registrationEnd = Match(s, registrationBody, "{", "}"); break; }
                }
            std::set<std::string> manualProperties;
            if (registrationBody < s.size())
            {
                for (size_t i = registrationBody + 1; i + 3 < registrationEnd; ++i)
                {
                    if (s[i].value == "DEFAULT_CONFIG" || s[i].value == "DEFAULT_CONFIG_AND_REQUIRED" ||
                        s[i].value == "DEFAULT_COMPONENT_CONFIG" || s[i].value == "DEFAULT_COMPONENT_CONFIG_AND_REQUIRED")
                    {
                        config = s[i + 2].value;
                        if (registrationConfig == s.size()) registrationConfig = i;
                    }
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
            std::string bindings;
            for (const auto& field : properties)
            {
                const bool manual = std::any_of(authoredProperties.begin(), authoredProperties.end(), [&](const auto& binding) { return binding.name == field.name; });
                if (!IsPropertyType(field.type) && !manual)
                { result.notes.push_back("Export requires a supported property type or custom registration: " + field.name + "."); continue; }
                bindings += "    REGISTER_PROPERTY(" + config + ", " + qualifiedName + ", " + field.name + ");\n";
                ++result.properties;
            }
            for (const auto& method : methods)
            {
                if (!method.exported) continue;
                std::vector<Binding> actions;
                const auto args = method.key.substr(method.key.find('(') + 1);
                const auto key = args.substr(0, args.find(')'));
                for (const auto& action : authoredActions)
                    if (action.name == method.name && (!action.hasSignature || CanonicalKey(key, usingCanis) == CanonicalKey(action.key, usingCanis))) actions.push_back(action);
                if (actions.empty())
                {
                    std::string label = method.name;
                    if (std::count_if(methods.begin(), methods.end(), [&](const auto& m) { return m.name == method.name; }) > 1 && !method.parameterNames.empty())
                    {
                        label += " (";
                        for (const auto& name : method.parameterNames) label += (label.back() == '(' ? "" : ", ") + name;
                        label += ")";
                    }
                    actions.push_back({method.name, key, "\"" + label + "\"", ""});
                }
                std::set<std::string> labels;
                for (const auto& action : actions)
                {
                    if (!labels.insert(action.label).second) continue;
                    bindings += "    RegisterSignalAction(" + config + ", " + action.label + ", static_cast<void(" + qualifiedName + "::*)(" + method.parameterTypes + ")>(&" + qualifiedName + "::" + method.name + ")";
                    if (method.parameterNames.size() > 1)
                    {
                        std::string names = action.parameterNames;
                        if (names.empty())
                        {
                            names = "{";
                            for (const auto& name : method.parameterNames) names += (names.back() == '{' ? "" : ", ") + std::string("\"") + name + "\"";
                            names += "}";
                        }
                        bindings += ", " + names;
                    }
                    bindings += ");\n";
                    ++result.actions;
                }
            }
            if (!bindings.empty() && (registrationBody == s.size() || config.empty()))
                throw std::runtime_error("Cannot find the Register / DEFAULT_CONFIG function for CANIS_EDITOR_EXPORT members. No source was changed.");
            const auto definitionsPosition = RuntimeDefinitionsPosition(source, s, registrationFunction, config);
            const auto indent = registrationBody < s.size() ? RegistrationIndent(source, s, registrationBody, registrationEnd) : std::string{};
            if (!bindings.empty())
            {
                const auto previous = source.rfind('\n', s[registrationConfig].begin);
                const size_t line = previous == std::string::npos ? 0 : previous + 1;
                const bool ownLine = source.find_first_not_of(" \t", line) == s[registrationConfig].begin;
                const size_t position = ownLine ? line : s[registrationConfig].begin;
                // Register after any config reset, directly before DEFAULT_CONFIG.
                // Keep compact source on its own line as well.
                const std::string prefix = ownLine ? "" : "\n";
                source.insert(position, prefix + indent + "Canis::EditorExports<" + qualifiedName +
                    ">::Register(" + config + ");\n" + (ownLine ? "" : indent));
            }

            std::set<std::string> existing;
            for (size_t i = 2; i + 1 < s.size(); ++i)
            {
                if (s[i - 1].value != "::" || s[i - 2].value != className || s[i + 1].value != "(") continue;
                const auto close = Match(s, i + 1, "(", ")");
                size_t end = close + 1;
                while (end < s.size() && s[end].value != "{" && s[end].value != ";") ++end;
                if (end == s.size() || s[end].value != "{") continue;
                existing.insert(CanonicalKey(s[i].value + "(" + ParameterKey(Parameters(s, i + 2, close)) + ")" + Qualifiers(s, close + 1, end), usingCanis));
            }
            std::string definitions;
            bool needsException = false;
            for (const auto& method : methods)
                if (generateMethodStubs && !method.inlineBody && existing.insert(CanonicalKey(method.key, usingCanis)).second)
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
                source.insert(definitionsPosition, definitions + "\n");
            }
            if (!bindings.empty())
            {
                const auto placement = PlaceExportsBeforeConfiguration(source, className, config);
                const std::string exports = "\n// CANIS EDITOR EXPORTS BEGIN " + qualifiedName + "\n" +
                    "template<> void Canis::EditorExports<" + qualifiedName + ">::Register(Canis::ScriptConf& " + config + ")\n{\n" +
                    (nameSpace.empty() ? "" : "    using namespace " + nameSpace + ";\n") + bindings + "}\n" +
                    "// CANIS EDITOR EXPORTS END " + qualifiedName + "\n";
                source.insert(placement.position, placement.closeNamespaces + exports + placement.reopenNamespaces);
            }
            if (needsException && source.find("#include <stdexcept>") == std::string::npos)
                source = "#include <stdexcept>\n" + source;
            result.source = source;
        }
        catch (const std::exception& error) { result.error = error.what(); result.source = original; result.methods = result.properties = result.actions = 0; }
        return result;
    }
}
