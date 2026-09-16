#include <Canis/Scripting/NativeBindings.hpp>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>
#include <sstream>

namespace Canis::Scripting
{
namespace
{
    bool Identifier(const std::string& name)
    {
        return !name.empty() && (std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_') &&
            std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
    }
    std::string Type(NativeKind kind)
    {
        switch (kind)
        {
        case NativeKind::Void: return "void"; case NativeKind::Bool: return "bool"; case NativeKind::Int: return "int";
        case NativeKind::Float: return "float"; case NativeKind::Double: return "double"; case NativeKind::Handle: return "ulong";
        case NativeKind::Vector3: return "System.Numerics.Vector3"; case NativeKind::Quaternion: return "System.Numerics.Quaternion";
        case NativeKind::Vector4: return "System.Numerics.Vector4"; case NativeKind::String: return "string";
        }
        throw std::invalid_argument("Unknown native type");
    }
}
NativeBindingRegistry& NativeBindingRegistry::Get() { static NativeBindingRegistry registry; return registry; }
void NativeBindingRegistry::Add(Entry entry)
{
    if (!Identifier(entry.group) || !Identifier(entry.member) || entry.group == entry.member) throw std::invalid_argument("Bindings require C# identifiers");
    const auto key = entry.group + "." + entry.member + (entry.operation.empty() ? "" : "." + entry.operation);
    if (entries.contains(key)) throw std::invalid_argument("Duplicate native binding: " + key);
    for (const auto& [_, existing] : entries)
        if (existing.group == entry.group && existing.member == entry.member &&
            (existing.owner != entry.owner || existing.operation.empty() || entry.operation.empty()))
            throw std::invalid_argument("Conflicting native member: " + key);
    entries.emplace(key, std::move(entry));
}
void NativeBindingRegistry::RemoveOwner(const std::string& owner)
{ std::erase_if(entries, [&](const auto& pair) { return pair.second.owner == owner; }); }
NativeValue NativeBindingRegistry::Invoke(const std::string& name, std::span<const NativeValue> arguments) const
{
    const auto it = entries.find(name);
    if (it == entries.end()) throw std::invalid_argument("Native binding unavailable: " + name);
    return it->second.invoke(arguments);
}
std::string NativeBindingRegistry::GenerateCSharp() const
{
    std::ostringstream out;
    out << "// Generated from game/engine C++ registrations. Do not edit.\nnamespace Canis.Native {\n";
    std::set<std::string> groups;
    for (const auto& [_, entry] : entries) groups.insert(entry.group);
    for (const auto& group : groups)
    {
        out << "public static class @" << group << " {\n";
        for (const auto& [key, entry] : entries)
        {
            if (entry.group != group || entry.operation == "set") continue;
            const std::string call = entry.result == NativeKind::Void ? "Canis.NativeBridge.Call" : "Canis.NativeBridge.Call<" + Type(entry.result) + ">";
            out << "public static " << Type(entry.result) << " @" << entry.member;
            if (entry.operation == "get")
            {
                out << " { get => " << call << "(\"" << key << "\");";
                if (entries.contains(group + "." + entry.member + ".set"))
                    out << " set => Canis.NativeBridge.Call(\"" << group << "." << entry.member << ".set\", value);";
                out << " }\n";
            }
            else
            {
                out << "(";
                for (size_t i = 0; i < entry.arguments.size(); ++i) out << (i ? ", " : "") << Type(entry.arguments[i]) << " arg" << i;
                out << ") => " << call << "(\"" << key << "\"";
                for (size_t i = 0; i < entry.arguments.size(); ++i) out << ", arg" << i;
                out << ");\n";
            }
        }
        out << "}\n";
    }
    return out.str() + "}\n";
}
int CANIS_NATIVE_CALL DispatchNative(const unsigned char* name, int length, const NativeValue* args, int count,
    NativeValue* result, unsigned char* error, int capacity) noexcept
{
    try
    {
        if (!name || length <= 0 || length > 512 || count < 0 || count > 32 || (count && !args) || !result)
            throw std::invalid_argument("Invalid native binding request");
        *result = NativeBindingRegistry::Get().Invoke(std::string(reinterpret_cast<const char*>(name), length), {args, static_cast<size_t>(count)});
        return 0;
    }
    catch (const std::exception& exception)
    {
        if (error && capacity > 0)
        {
            const auto size = std::min(static_cast<size_t>(capacity - 1), std::strlen(exception.what()));
            std::memcpy(error, exception.what(), size); error[size] = 0;
        }
        return -1;
    }
    catch (...) { if (error && capacity > 0) error[0] = 0; return -1; }
}
}
