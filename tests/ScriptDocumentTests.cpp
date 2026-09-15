#include <Canis/ScriptDocument.hpp>
#include <chrono>
#include <iostream>
#include <stdexcept>

using namespace Canis::ScriptEditing;
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
int main()
{
    const auto root = std::filesystem::temp_directory_path() /
        ("canis-script-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    try
    {
        const auto path = root / "Test.cpp";
        { std::ofstream file(path); file << "first\nsecond\n"; }
        Document doc;
        Check(doc.Load(path), "load");
        Check(!doc.Dirty(), "fresh document should be clean");
        doc.text += "third\n";
        Check(doc.Dirty() && doc.Save() && !doc.Dirty(), "save edits");
        std::string disk;
        Check(Document::Read(path, disk) && disk == doc.text, "saved bytes");
        { std::ofstream file(path); file << "external edit\n"; }
        doc.text += "fourth\n";
        Check(!doc.Save(), "must reject external changes");
        Check(Document::Read(path, disk) && disk == "external edit\n", "external changes preserved");
        Check(doc.Dirty(), "unsaved buffer preserved on conflict");
        Check(doc.Load(path) && !doc.Dirty(), "explicit disk reload");
        std::filesystem::remove(path);
        doc.text += "new\n";
        Check(!doc.Save(), "must not resurrect deleted files");
        Check(!doc.Load(root / "missing.cpp"), "missing file");
        Check(LineOffset("one\ntwo\n", 2, 2) == 5, "line column navigation");
        Check(LineOffset("one\ntwo\n", 1, 90) == 3, "column clamp");
        Check(LineOffset("one\ntwo\n", 100) == 8, "line clamp");
        Diagnostic diagnostic;
        Check(ParseDiagnostic("/tmp/space dir/Foo.cpp:12:4: error: unknown name", diagnostic), "gcc error");
        Check(diagnostic.path == "/tmp/space dir/Foo.cpp" && diagnostic.line == 12 && diagnostic.column == 4, "gcc location");
        Check(ParseDiagnostic("C:\\Game\\Foo.cpp(8,2): error C2065: unknown name", diagnostic), "MSVC error");
        Check(diagnostic.line == 8 && diagnostic.column == 2, "MSVC location");
        Check(ParseDiagnostic("C:\\Game\\Foo.cpp(8): warning C4100: parameter", diagnostic), "MSVC line only");
        Check(ParseDiagnostic("/tmp/Foo.cpp:3: warning: unused value", diagnostic), "gcc line only");
        Check(!ParseDiagnostic("[100%] Built target GameCode", diagnostic), "ordinary output");
        Check(!ParseDiagnostic("foo.cpp:9999999999999999999999:1: error: overflow", diagnostic), "malformed location");
        std::filesystem::remove_all(root);
        std::cout << "Script document and diagnostic checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::filesystem::remove_all(root);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
