#pragma once
#include <string>
#include <vector>

namespace Canis
{
    class App;
    class Entity;
    struct ComponentPickerEntry
    {
        std::string componentName;
        std::string displayName;
        bool managed = false;
    };
    std::vector<ComponentPickerEntry> BuildComponentPickerEntries(App& app, Entity& entity);
    bool AddPickedComponent(App& app, Entity& entity, const ComponentPickerEntry& entry);
}
