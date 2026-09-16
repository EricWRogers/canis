#include <Canis/CppSyntax.hpp>
#include <iostream>
#include <stdexcept>
using namespace Canis::ScriptEditing;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
CppColor At(const std::string& text, const std::string& needle)
{
    const auto offset = text.find(needle);
    for (const auto& span : HighlightCpp(text)) if (span.begin <= offset && span.end > offset) return span.color;
    throw std::runtime_error("missing token");
}
int main()
{
    try
    {
        const std::string text = "#include <Canis/Components.hpp>\npublic:\nfloat value = 1.25e-3f;\nvoid Tick() { /* return 42; */ Log(\"// string\"); }\n// comment\nREGISTER_PROPERTY(config, Foo, value);";
        Check(At(text,"#include") == CppColor::Preprocessor, "directive");
        Check(At(text,"<Canis") == CppColor::String, "include path");
        Check(At(text,"public") == CppColor::Keyword, "keyword");
        Check(At(text,"float") == CppColor::Type, "type");
        Check(At(text,"Tick") == CppColor::Function, "function");
        Check(At(text,"1.25") == CppColor::Number, "number");
        Check(At(text,"return 42") == CppColor::Comment, "comment contents");
        Check(At(text,"// string") == CppColor::String, "string contents");
        Check(At(text,"REGISTER_PROPERTY") == CppColor::Preprocessor, "macro");
        const std::string raw = "auto text = u8R\"tag(first\n// still a string\n)tag\"; int after;";
        Check(At(raw,"still a string") == CppColor::String, "multiline raw string");
        Check(At(raw,"int after") == CppColor::Type, "raw string ends");
        for (const auto& sample : {text, raw, std::string("/* unfinished"), std::string("\"unfinished"), std::string("'\\\'' 0xff 1'000 .5f"), std::string("// UTF-8 café\r\n\tint a;"), std::string()})
        {
            size_t end = 0;
            for (const auto& span : HighlightCpp(sample))
            { Check(span.begin == end && span.end > span.begin && span.end <= sample.size(), "complete non-overlapping token coverage"); end = span.end; }
            Check(end == sample.size(), "all bytes covered");
        }
        CppHighlightCache cache;
        cache.Update("/* one\ntwo */\n");
        Check(cache.lines == std::vector<size_t>({0,7,14}), "line offsets");
        cache.Update("int x;");
        Check(cache.lines.size() == 1 && cache.spans.front().color == CppColor::Type, "refresh after edit");
        const std::string cs = "public sealed partial class Demo { string path = @\"a\\b\"\"c\nsecond\"; var raw = \"\"\"text\n// still string\"\"\"; await Work(); }";
        const auto csSpans = HighlightCSharp(cs);
        auto csAt = [&](const std::string& word) {
            const auto offset = cs.find(word);
            for (const auto& span : csSpans) if (span.begin <= offset && offset < span.end) return span.color;
            return CppColor::Text;
        };
        Check(csAt("sealed") == CppColor::Keyword && csAt("partial") == CppColor::Keyword, "C# modifiers");
        Check(csAt("await") == CppColor::Keyword && csAt("var") == CppColor::Type, "C# await/var");
        Check(csAt("second") == CppColor::String, "C# multiline verbatim string");
        Check(csAt("still string") == CppColor::String, "C# raw string");
        cache.Update(cs, true);
        Check(cache.csharp, "C# cache mode");
        cache.Update(cs, false);
        Check(!cache.csharp, "switching language invalidates highlight cache");
        std::cout << "C++ syntax checks passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
