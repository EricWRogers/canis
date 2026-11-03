#include <Canis/ComponentPicker.hpp>
#include <Canis/App.hpp>
#include <Canis/Components.hpp>
#include <Canis/Scripting/ManagedComponents.hpp>

namespace Canis
{
    std::vector<ComponentPickerEntry> BuildComponentPickerEntries(App& app, Entity& entity)
    {
        std::vector<ComponentPickerEntry> entries;
        if (!entity.IsValid()) return entries;
        for (const auto& conf : app.GetComponentRegistry())
        {
            if (conf.name == PrefabInstance::ScriptName || !conf.Has || !conf.Add || conf.Has(entity)) continue;
            std::string display = conf.name;
            if (conf.name == DirectionalLight::ScriptName) display = "3D/Directional Light";
            else if (conf.name == PointLight::ScriptName) display = "3D/Point Light";
            else if (conf.name == SpotLight::ScriptName) display = "3D/Spot Light";
            entries.push_back({conf.name, display, false});
        }
        for (const auto& script : Scripting::GetAvailableManagedComponents(entity))
            entries.push_back({script.type, script.name + " (C#)", true});
        return entries;
    }
    bool AddPickedComponent(App& app, Entity& entity, const ComponentPickerEntry& entry)
    {
        if (!entity.IsValid()) return false;
        if (entry.managed) return Scripting::AddManagedComponent(entity, entry.componentName);
        const auto* conf = app.GetComponentConf(entry.componentName);
        if (!conf || !conf->Add || !conf->Has || conf->Has(entity)) return false;
        // Copy before user code: adding a component may update registrations.
        const auto add = conf->Add;
        add(entity);
        return true;
    }
}
