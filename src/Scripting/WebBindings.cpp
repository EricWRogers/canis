#include <Canis/External/tinygltf/json.hpp>
#include <Canis/Scripting/NativeBindings.hpp>
#include <Canis/Scripting/WebBindings.hpp>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>

namespace Canis::Scripting
{
    std::string DispatchWebBinding(std::string_view request) noexcept
    {
        using Json = nlohmann::json;
        try
        {
            if (request.size() > 2 * 1024 * 1024) throw std::invalid_argument("Web binding request too large");
            auto message = Json::parse(request);
            const auto name = message.at("name").get<std::string>();
            const auto& arguments = message.at("arguments");
            if (name.empty() || name.size() > 512 || !arguments.is_array() || arguments.size() > 32)
                throw std::invalid_argument("Invalid web binding request");
            std::array<NativeValue, 32> values{};
            std::array<std::string, 32> strings;
            for (size_t i = 0; i < arguments.size(); ++i)
            {
                const auto& argument = arguments[i];
                const auto& kind = argument.at("kind");
                if (!kind.is_number_integer() || kind < 1 || kind > 9)
                    throw std::invalid_argument("Invalid web argument kind");
                auto& value = values[i];
                value.kind = static_cast<NativeKind>(kind.get<int>());
                if (value.kind == NativeKind::String)
                {
                    strings[i] = argument.at("text").get<std::string>();
                    if (strings[i].size() > 1024 * 1024) throw std::invalid_argument("Web string too large");
                    value.data = reinterpret_cast<uintptr_t>(strings[i].data());
                    value.length = static_cast<int32_t>(strings[i].size());
                }
                else if (value.kind == NativeKind::Handle)
                {
                    const auto text = argument.at("handle").get<std::string>();
                    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value.data);
                    if (error != std::errc() || end != text.data() + text.size())
                        throw std::invalid_argument("Invalid web handle");
                }
                else
                {
                    const auto& numbers = argument.at("numbers");
                    const size_t count = value.kind == NativeKind::Vector3 ? 3 :
                        (value.kind == NativeKind::Quaternion || value.kind == NativeKind::Vector4 ? 4 : 1);
                    if (!numbers.is_array() || numbers.size() != count) throw std::invalid_argument("Invalid web value length");
                    std::array<double, 4> decoded{};
                    for (size_t n = 0; n < count; ++n)
                    {
                        decoded[n] = numbers[n].get<double>();
                        if (!std::isfinite(decoded[n]) || (value.kind != NativeKind::Double && std::abs(decoded[n]) > std::numeric_limits<float>::max()))
                            throw std::invalid_argument("Invalid web numeric value");
                    }
                    value.x = decoded[0]; value.y = decoded[1]; value.z = decoded[2]; value.w = decoded[3];
                    if (value.kind == NativeKind::Bool && value.x != 0 && value.x != 1)
                        throw std::invalid_argument("Invalid web boolean");
                    if (value.kind == NativeKind::Int && (value.x < std::numeric_limits<int>::min() || value.x > std::numeric_limits<int>::max() || std::trunc(value.x) != value.x))
                        throw std::invalid_argument("Invalid web integer");
                }
            }
            const auto value = NativeBindingRegistry::Get().Invoke(name, {values.data(), arguments.size()});
            Json result = {{"kind", static_cast<int>(value.kind)}};
            if (value.kind == NativeKind::String) result["text"] = Decode<std::string>(value);
            else if (value.kind == NativeKind::Handle) result["handle"] = std::to_string(value.data);
            else result["numbers"] = {value.x, value.y, value.z, value.w};
            return Json{{"result", result}}.dump();
        }
        catch (const std::exception& error) { return Json{{"error", error.what()}}.dump(-1, ' ', false, Json::error_handler_t::replace); }
        catch (...) { return R"({"error":"Unknown native web binding failure"})"; }
    }
}
