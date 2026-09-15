#include <Canis/ScriptEditingTools.hpp>
#include <iostream>
#include <stdexcept>
using namespace Canis::ScriptEditing;
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main()
{
    try
    {
        auto edit = IndentLines("one\n  two\nthree", 0, 10, false, false);
        Check(edit.text == "    one\n      two", "selection ending at newline must not indent next line");
        edit = IndentLines("    one\n\ttwo", 0, 12, true, false);
        Check(edit.text == "one\ntwo", "unindent spaces and tabs");
        edit = IndentLines("  one\n  two", 0, 11, false, true);
        Check(edit.text == "  // one\n  // two", "comment selected lines");
        edit = IndentLines(edit.text, 0, edit.text.size(), false, true);
        Check(edit.text == "  one\n  two", "uncomment round trip");
        edit = IndentLines("// one\n\n// two", 0, 14, false, true);
        Check(edit.text == "one\n\ntwo", "uncomment selection containing blank lines");
        edit = ReplaceMatches("aaa aaa tail", "aaa", "x", 0, 7, true);
        Check(edit.text == "x x" && edit.end == 7, "replace bounded selection");
        edit = ReplaceMatches("aa", "a", "aa", 0, 2, true);
        Check(edit.text == "aaaa", "replacement containing query must terminate");
        Check(MatchingBracket("{ \"}\" /* } */ { } }", 0) == 18, "ignore braces in literals and comments");
        Check(MatchingBracket("{ unmatched", 0) == -1, "unmatched bracket");
        std::cout << "Script editing tools tests passed\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
