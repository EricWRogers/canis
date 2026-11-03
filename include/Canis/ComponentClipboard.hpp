#pragma once

#include <cstdint>
#include <string>
#include <yaml-cpp/yaml.h>

namespace Canis
{
    class Entity;
    struct ScriptConf;

    // Serialized values survive entity deletion, scene changes and code reloads.
    // No script pointers, native callbacks or attachment tokens are retained.
    class ComponentClipboard
    {
    public:
        static bool CanCopy(const ScriptConf& conf, Entity& source);
        bool Copy(const ScriptConf& conf, Entity& source, std::string& error);
        bool CanPaste(const ScriptConf& conf, Entity& destination) const;
        bool Paste(const ScriptConf& conf, Entity& destination, std::string& error) const;

        bool CopyManaged(Entity& source, uint64_t token, std::string& error);
        bool CanPasteManaged(Entity& destination, uint64_t token) const;
        bool PasteManaged(Entity& destination, uint64_t token, std::string& error) const;
        const std::string& TypeName() const { return m_type; }

    private:
        std::string m_type;
        YAML::Node m_values;
        bool m_managed = false;
    };
}
