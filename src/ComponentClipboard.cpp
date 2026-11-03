#include <Canis/ComponentClipboard.hpp>
#include <Canis/Components.hpp>
#include <Canis/ConfigData.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>
#include <exception>
#include <stdexcept>

using namespace Canis;

namespace
{
    YAML::Node Values(const ScriptConf& conf, Entity& entity)
    {
        YAML::Node encoded;
        conf.Encode(encoded, entity);
        const auto node = encoded[conf.name];
        if (!node.IsDefined()) throw std::runtime_error("Component has no serialized values.");
        return YAML::Clone(node);
    }
}

namespace Canis
{
    bool ComponentClipboard::CanCopy(const ScriptConf& conf, Entity& source)
    {
        return source && conf.Has && conf.Has(source) && conf.Encode && conf.Decode &&
            conf.name != PrefabInstance::ScriptName;
    }

    bool ComponentClipboard::Copy(const ScriptConf& conf, Entity& source, std::string& error)
    {
        error.clear();
        if (!CanCopy(conf, source)) { error = "This component cannot copy values."; return false; }
        try
        {
            auto values = Values(conf, source);
            if (conf.name == Transform::ScriptName)
            {
                values.remove("parent");
                values.remove("children");
            }
            m_values.reset(values);
            m_type = conf.name;
            m_managed = false;
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    bool ComponentClipboard::CanPaste(const ScriptConf& conf, Entity& destination) const
    {
        return !m_managed && !m_type.empty() && m_type == conf.name && CanCopy(conf, destination);
    }

    bool ComponentClipboard::Paste(const ScriptConf& conf, Entity& destination, std::string& error) const
    {
        error.clear();
        if (!CanPaste(conf, destination)) { error = "Paste requires an existing component of the copied type."; return false; }
        YAML::Node before;
        // Copy callbacks so a custom decoder cannot invalidate references by changing registrations.
        const auto decode = conf.Decode;
        const auto name = conf.name;
        try
        {
            before[name] = Values(conf, destination);
            YAML::Node patch;
            patch[name] = YAML::Clone(m_values);
            decode(patch, destination, false);
            return true;
        }
        catch (const std::exception& exception)
        {
            error = exception.what();
            if (before.IsMap() && destination)
                try { decode(before, destination, false); }
                catch (const std::exception& rollback) { error += "; restoring values failed: " + std::string(rollback.what()); }
            return false;
        }
    }

    bool ComponentClipboard::CopyManaged(Entity& source, uint64_t token, std::string& error)
    {
        error.clear();
        try
        {
            const auto values = Scripting::GetManagedComponentValues(source, token);
            if (!values.IsDefined()) { error = "C# component is no longer attached."; return false; }
            m_type = values["type"].as<std::string>();
            m_values.reset(YAML::Clone(values));
            m_managed = true;
            return true;
        }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }

    bool ComponentClipboard::CanPasteManaged(Entity& destination, uint64_t token) const
    {
        if (!m_managed || m_type.empty()) return false;
        const auto values = Scripting::GetManagedComponentValues(destination, token);
        return values.IsDefined() && values["type"].as<std::string>() == m_type;
    }

    bool ComponentClipboard::PasteManaged(Entity& destination, uint64_t token, std::string& error) const
    {
        error.clear();
        if (!CanPasteManaged(destination, token)) { error = "Paste requires an existing C# component of the copied type."; return false; }
        try { return Scripting::SetManagedComponentValues(destination, token, YAML::Clone(m_values)); }
        catch (const std::exception& exception) { error = exception.what(); return false; }
    }
}
