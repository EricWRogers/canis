#include <Canis/Tag.hpp>
#include <stdexcept>
#include <unordered_map>

namespace Canis {
namespace {
std::unordered_map<TagId, std::string>& Names() {
    static std::unordered_map<TagId, std::string> names{{NoTag, ""}};
    return names;
}
}
TagId RegisterTag(std::string_view name) {
    const TagId id = TagIdFromName(name);
    auto [entry, inserted] = Names().try_emplace(id, name);
    if (!inserted && entry->second != name)
        throw std::runtime_error("Tag ID collision between '" + entry->second + "' and '" + std::string(name) + "'");
    return id;
}
const std::string& GetTagName(TagId id) {
    const auto entry = Names().find(id);
    if (entry == Names().end())
        throw std::runtime_error("Unknown tag ID; assign authored tags with SetTag or RegisterTag");
    return entry->second;
}
}
