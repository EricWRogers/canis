#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace Canis::ScriptEditing
{
    enum class CppColor { Text, Keyword, Type, Function, Number, String, Comment, Preprocessor };
    struct CppSpan { size_t begin, end; CppColor color; };
    std::vector<CppSpan> HighlightCpp(const std::string& text);
    struct CppHighlightCache
    {
        std::string text;
        std::vector<CppSpan> spans;
        std::vector<size_t> lines;
        std::vector<int> brackets;
        void Update(const std::string& source);
    };
}
