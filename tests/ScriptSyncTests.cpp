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
namespace Canis {
struct ScriptConf {};
template<class T> struct EditorExports { static void Register(ScriptConf&); };
}
#define CANIS_EDITOR_EXPORT template<class> friend struct ::Canis::EditorExports;
namespace Demo {
class Stove {
public:
    static constexpr const char* ScriptName = "Demo::Stove";
    CANIS_EDITOR_EXPORT float temperature = 100.0f;
    CANIS_EDITOR_EXPORT std::string label = "grill; { }";
    CANIS_EDITOR_EXPORT bool heating{true};
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
Canis::ScriptConf config;
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
        auto signalHeader = header;
        signalHeader.insert(signalHeader.find("    CANIS_EDITOR_EXPORT float temperature"), "    CANIS_EDITOR_EXPORT Canis::Signal<> dead; CANIS_EDITOR_EXPORT Signal<float> healed; CANIS_EDITOR_EXPORT Canis::Signal<Demo::Payload> changed;\n");
        const auto signalSync = SynchronizeHeader(signalHeader, source);
        Check(signalSync.error.empty() && signalSync.properties == 6 && signalSync.source.find("Stove, changed)") != std::string::npos,
              "Script sync omitted serializable signals or structured payloads");
        Check(sync.source.find("CANIS EDITOR EXPORT DECLARATION") == std::string::npos,
              "An unnecessary forward specialization declaration was emitted");
        Check(sync.source.find("template<> void Canis::EditorExports<Demo::Stove>::Register") <
              sync.source.find("Canis::EditorExports<Demo::Stove>::Register(config);"),
              "Export definition must precede its first registration call");
        auto repeat = SynchronizeHeader(header, sync.source);
        Check(repeat.error.empty() && repeat.source == sync.source && repeat.methods == 0, "repeat save must be byte-idempotent");
        const auto checkIndent = [&](const std::string& registration, const std::string& indent)
        {
            const auto result = SynchronizeHeader(header, registration);
            Check(result.error.empty(), result.error.c_str());
            const std::string block = "\n" + indent + "Canis::EditorExports<Demo::Stove>::Register(config);\n";
            Check(result.source.find(block) != std::string::npos, "Generated block did not follow body indentation");
            Check(result.source.find(indent + "REGISTER_PROPERTY(config, Demo::Stove, manual);") != std::string::npos,
                  "Indentation sync changed a manual registration");
            const auto repeated = SynchronizeHeader(header, result.source);
            Check(repeated.error.empty() && repeated.source == result.source, "Indented generation was not idempotent");
        };
        checkIndent(R"cpp(namespace Outer
{
    namespace Inner
    {
        void RegisterStoveScript()
        {
            REGISTER_PROPERTY(config, Demo::Stove, manual);
            DEFAULT_CONFIG(config, Demo::Stove);
        }
    }
}
)cpp", "            ");
        checkIndent("namespace Demo {\n  void RegisterStoveScript() {\n    REGISTER_PROPERTY(config, Demo::Stove, manual);\n    DEFAULT_CONFIG(config, Demo::Stove);\n  }\n}\n", "    ");
        checkIndent("namespace Demo {\n\tvoid RegisterStoveScript() {\n\t\tREGISTER_PROPERTY(config, Demo::Stove, manual);\n\t\tDEFAULT_CONFIG(config, Demo::Stove);\n\t}\n}\n", "\t\t");
        const auto repaired = SynchronizeHeader(header, R"cpp(namespace Demo
{
    void RegisterStoveScript()
    {
    // CANIS AUTO PROPERTIES BEGIN Demo::Stove
    REGISTER_PROPERTY(config, Demo::Stove, temperature);
    // CANIS AUTO PROPERTIES END Demo::Stove
        REGISTER_PROPERTY(config, Demo::Stove, manual);
        DEFAULT_CONFIG(config, Demo::Stove);
    }
}
)cpp");
        Check(repaired.error.empty() && repaired.source.find("\n        Canis::EditorExports<Demo::Stove>::Register(config);") != std::string::npos &&
              repaired.source.find("// CANIS AUTO PROPERTIES BEGIN") == std::string::npos,
              "Existing incorrectly indented block was not repaired");
        const auto bottomConfig = SynchronizeHeader(header, R"cpp(
namespace Demo
{
    void Stove::Existing(float value) { /* Preserve runtime code */ }
    namespace { Canis::ScriptConf config; }
    void RegisterStoveScript()
    {
        REGISTER_PROPERTY(config, Demo::Stove, manual);
        DEFAULT_CONFIG(config, Demo::Stove);
    }
}
)cpp");
        Check(bottomConfig.error.empty() && bottomConfig.methods == 5, "Failed to sync a bottom registration section");
        Check(bottomConfig.source.find("Stove::Cool") < bottomConfig.source.find("Canis::ScriptConf config;") &&
              bottomConfig.source.find("Stove::Read") < bottomConfig.source.find("void RegisterStoveScript"),
              "Generated methods were placed after the configuration section");
        Check(bottomConfig.source.find("Stove::Existing") < bottomConfig.source.find("// CANIS EDITOR EXPORTS BEGIN") &&
              bottomConfig.source.find("// CANIS EDITOR EXPORTS END") < bottomConfig.source.find("Canis::ScriptConf config;"),
              "Export definition was not placed between runtime code and configuration");
        auto resetSource = source;
        resetSource.insert(resetSource.find("    DEFAULT_CONFIG"), "    config = {};\n");
        const auto resetSync = SynchronizeHeader(header, resetSource);
        Check(resetSync.error.empty() && resetSync.source.find("config = {};") < resetSync.source.find("Canis::EditorExports<Demo::Stove>::Register(config);"),
              "Generated bindings were wiped by configuration initialization");
        const auto repeatBottomConfig = SynchronizeHeader(header, bottomConfig.source);
        Check(repeatBottomConfig.error.empty() && repeatBottomConfig.source == bottomConfig.source,
              "Bottom registration synchronization was not idempotent");
        const std::string globalHeader = R"cpp(struct Stove {
    static constexpr const char* ScriptName = "Stove";
    CANIS_EDITOR_EXPORT float temperature;
    void Existing();
    void Added();
};)cpp";
        const auto globalConfig = SynchronizeHeader(globalHeader, R"cpp(void Stove::Existing() {}
namespace { Canis::ComponentConf config; }
void RegisterStoveComponent() {
    DEFAULT_COMPONENT_CONFIG(config, Stove);
}
)cpp");
        Check(globalConfig.error.empty() && globalConfig.methods == 1 &&
              globalConfig.source.find("Stove::Added") < globalConfig.source.find("namespace { Canis::ComponentConf config; }"),
              "Global component method was placed after its anonymous configuration namespace");
        Check(SynchronizeHeader(globalHeader, globalConfig.source).source == globalConfig.source,
              "Global component synchronization was not idempotent");
        auto explicitHeader = header;
        explicitHeader.insert(explicitHeader.find("private:"), "CANIS_EDITOR_EXPORT void Fire();\nCANIS_EDITOR_EXPORT void Fire(float amount);\nprivate:\nCANIS_EDITOR_EXPORT float secretExport = 3;\nCANIS_EDITOR_EXPORT void Pause();\nCANIS_EDITOR_EXPORT void Notify(const std::string& text);\n");
        auto explicitSource = source;
        explicitSource.insert(explicitSource.find("    DEFAULT_CONFIG"), "    RegisterSignalAction(config, \"Legacy Fire\", static_cast<void(Stove::*)(float)>(&Stove::Fire));\n    RegisterSignalAction(config, \"Text delivered\", &Stove::Notify);\n");
        const auto explicitSync = SynchronizeHeader(explicitHeader, explicitSource);
        Check(explicitSync.error.empty() && explicitSync.properties == 4 && explicitSync.actions == 4,
              "Explicit public/private exports or overloads were not collected");
        Check(explicitSync.source.find("REGISTER_PROPERTY(config, Demo::Stove, secretExport)") != std::string::npos &&
              explicitSync.source.find("REGISTER_PROPERTY(config, Demo::Stove, secret)") == std::string::npos &&
              explicitSync.source.find("RegisterSignalAction(config, \"Legacy Fire\"") != std::string::npos,
              "Unmarked private field leaked or authored callback name changed");
        Check(SynchronizeHeader(explicitHeader, explicitSync.source).source == explicitSync.source,
              "Explicit action exports did not remain byte-idempotent");
        explicitHeader.erase(explicitHeader.find("CANIS_EDITOR_EXPORT void Fire(float amount);"), std::string("CANIS_EDITOR_EXPORT ").size());
        explicitHeader.erase(explicitHeader.find("CANIS_EDITOR_EXPORT float secretExport"), std::string("CANIS_EDITOR_EXPORT ").size());
        const auto unexport = SynchronizeHeader(explicitHeader, explicitSync.source);
        Check(unexport.error.empty() && unexport.properties == 3 && unexport.actions == 3 &&
              unexport.source.find("REGISTER_PROPERTY(config, Demo::Stove, secretExport)") == std::string::npos &&
              unexport.source.find("RegisterSignalAction(config, \"Legacy Fire\"") == std::string::npos,
              "Removing a marker did not remove its generated field/action binding");
        auto obsoleteExample = source;
        obsoleteExample.insert(obsoleteExample.find("    DEFAULT_CONFIG"), "    // REGISTER_PROPERTY(config, Demo::Stove, exampleProperty);\n");
        const auto cleanedExample = SynchronizeHeader(header, obsoleteExample);
        Check(cleanedExample.error.empty() && cleanedExample.source.find("exampleProperty);") == std::string::npos &&
              cleanedExample.source.find("// CANIS AUTO PROPERTIES") == std::string::npos,
              "Obsolete scaffolding comments were generated or retained");
        auto changed = header;
        changed.replace(changed.find("CANIS_EDITOR_EXPORT float temperature = 100.0f;"), std::string("CANIS_EDITOR_EXPORT float temperature = 100.0f;").size(), "private: CANIS_EDITOR_EXPORT float temperature = 100.0f; public:");
        auto hidden = SynchronizeHeader(changed, sync.source);
        Check(hidden.error.empty(), "private field sync");
        Check(hidden.source.find("    REGISTER_PROPERTY(config, Demo::Stove, temperature);") != std::string::npos, "private marked field was not exported");
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
        multiple.insert(multiple.find("    CANIS_EDITOR_EXPORT float temperature"), "float first = 1, second{2};\n");
        auto multiSync = SynchronizeHeader(multiple, source);
        Check(multiSync.error.empty() && multiSync.properties == 3, "unmarked public fields must not be exported");
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
