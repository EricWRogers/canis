#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace Canis {
using TagId = std::uint64_t;
inline constexpr TagId NoTag = 0;

// Stable FNV-1a identity shared by authored names and generated enum values.
constexpr TagId TagIdFromName(std::string_view name) {
    if (name.empty()) return NoTag;
    TagId result = 14695981039346656037ull;
    for (unsigned char value : name) {
        result ^= value;
        result *= 1099511628211ull;
    }
    return result;
}
// Name registration belongs at authoring/load boundaries, never in tag comparisons.
TagId RegisterTag(std::string_view name);
const std::string& GetTagName(TagId id);
}
