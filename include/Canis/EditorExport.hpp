#pragma once

namespace Canis
{
    struct ScriptConf;

    // The generated specialization is the only bridge used to bind marked
    // private members. Exporting a member does not change its C++ visibility.
    template<class Component> struct EditorExports
    {
        static void Register(ScriptConf& config);
    };
}

// Mark mutable instance fields or non-static void Signal actions. Public and
// private members follow the same opt-in rule.
#define CANIS_EDITOR_EXPORT template<class> friend struct ::Canis::EditorExports;
