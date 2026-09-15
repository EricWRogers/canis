#include <Canis/ScriptSync.hpp>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Canis::ScriptEditing;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv)
{
    try
    {
        const std::string header = R"cpp(
#include <string>
namespace Demo {
class Stove {
public:
    static constexpr const char* ScriptName = "Demo::Stove";
    float temperature = 100.0f;
    std::string label = "grill; { }";
    bool heating{true};
    int manual = 0;
    static int ignored;
    const int constant = 2;
    void Existing(float changedName);
    void Start(int count = 2);
    void Start(float amount);
    int Read() const;
    int Read();
    void Inline() { /* body with ; */ }
    virtual void Pure() = 0;
    void Deleted() = delete;
private:
    float secret = 0;
    void Cool(float dt);
};
}
)cpp";
        const std::string source = R"cpp(
#include "Fixture.hpp"
#define REGISTER_PROPERTY(c,t,p) static_assert(requires(t& v) { v.p; });
#define DEFAULT_CONFIG(c,t)
namespace Demo {
void RegisterStoveScript() {
    // REGISTER_PROPERTY(config, Demo::Stove, temperature);
    REGISTER_PROPERTY(config, Demo::Stove, manual);
    DEFAULT_CONFIG(config, Demo::Stove);
}
void Stove::Existing(float oldName) { (void)oldName; /* KEEP THIS BODY */ }
}
)cpp";
        auto sync = SynchronizeHeader(header, source);
        Check(sync.error.empty(), sync.error.c_str());
        Check(sync.properties == 3, "register public supported fields only, exclude manual/static/const/private");
        Check(sync.methods == 5, "default args, overloads, const and private methods");
        Check(sync.source.find("KEEP THIS BODY") != std::string::npos, "preserve implementation");
        Check(sync.source.find("Stove::Start(int count)") != std::string::npos, "strip default argument");
        Check(sync.source.find("throw std::logic_error") != std::string::npos, "safe nonvoid placeholder");
        auto repeat = SynchronizeHeader(header, sync.source);
        Check(repeat.error.empty() && repeat.source == sync.source && repeat.methods == 0, "repeat save must be byte-idempotent");
        auto changed = header;
        changed.replace(changed.find("float temperature = 100.0f;"), std::string("float temperature = 100.0f;").size(), "private: float temperature = 100.0f; public:");
        auto hidden = SynchronizeHeader(changed, sync.source);
        Check(hidden.error.empty(), "private field sync");
        Check(hidden.source.find("    REGISTER_PROPERTY(config, Demo::Stove, temperature);") == std::string::npos, "remove stale generated registration");
        Check(hidden.source.find("REGISTER_PROPERTY(config, Demo::Stove, manual)") != std::string::npos, "preserve manual registration");
        Check(hidden.source.find("Stove::Start") != std::string::npos, "preserve generated method bodies");
        auto bad = SynchronizeHeader(header.substr(0, header.find("private:")), source);
        Check(!bad.error.empty() && bad.source == source, "incomplete header is never applied");
        changed = header;
        changed.insert(changed.find("float temperature"), "\n#if FEATURE\n#endif\n");
        bad = SynchronizeHeader(changed, source);
        Check(!bad.error.empty() && bad.source == source, "conditional class requires manual handling");
        auto qualified = header;
        qualified.insert(qualified.find("void Existing"), "void StringArg(const std::string& text);\n");
        auto qualifiedSource = source + "\nvoid Demo::Stove::StringArg(std::string const& renamed) {}\n";
        auto qualifiedSync = SynchronizeHeader(qualified, qualifiedSource);
        Check(qualifiedSync.error.empty() && qualifiedSync.methods == 5, "const parameter spelling must not duplicate an implementation");
        auto markerLiteral = source + "\nconst char* markerText = \"// CANIS AUTO PROPERTIES BEGIN Demo::Stove\";\n";
        auto literalSync = SynchronizeHeader(header, markerLiteral);
        Check(literalSync.error.empty() && literalSync.source.find("const char* markerText") != std::string::npos, "marker text inside string literals is preserved");
        const auto rawSource = source + R"test(
const char* raw = R"payload(
// CANIS AUTO PROPERTIES BEGIN Demo::Stove
not a generated block
// CANIS AUTO PROPERTIES END Demo::Stove
)payload";
)test";
        auto rawSync = SynchronizeHeader(header, rawSource);
        Check(rawSync.error.empty() && rawSync.source.find("not a generated block") != std::string::npos, "raw string marker contents preserved");
        auto templateHeader = header;
        templateHeader.insert(templateHeader.find("class Stove"), "template<typename T>\n");
        bad = SynchronizeHeader(templateHeader, source);
        Check(!bad.error.empty() && bad.source == source, "template classes require manual sync");
        auto malformedField = header;
        malformedField.insert(malformedField.find("float temperature"), "bool incomplete\n");
        bad = SynchronizeHeader(malformedField, sync.source);
        Check(!bad.error.empty() && bad.source == sync.source, "missing field semicolon must not delete generated registrations");
        auto multiple = header;
        multiple.insert(multiple.find("float temperature"), "float first = 1, second{2};\n");
        auto multiSync = SynchronizeHeader(multiple, source);
        Check(multiSync.error.empty() && multiSync.properties == 5, "comma-separated public fields");
        Check(SynchronizeHeader("#pragma once\nclass Helper {};", source).source == source, "ordinary headers unchanged");
        if (argc > 1)
        {
            std::ofstream(std::string(argv[1]) + "/Fixture.hpp") << header;
            std::ofstream(std::string(argv[1]) + "/Fixture.cpp") << sync.source;
        }
        std::cout << "Script sync checks passed\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
