# Serializable signals

`Canis::Signal<>` emits no value; `Canis::Signal<T>` emits one typed value. Use a struct
for multiple values. Persistent listeners are editable in the Inspector and saved
through normal registered properties in scenes and prefabs.

```cpp
#include <Canis/Signal.hpp>

Canis::Signal<> outOfHealth;
Canis::Signal<float> healed;

// In the script's registration function:
REGISTER_PROPERTY(conf, Health, outOfHealth);
REGISTER_PROPERTY(conf, Health, healed);
RegisterSignalAction(conf, "Damage", &Health::Damage); // void Damage(float)
RegisterSignalAction(conf, "Kill", &Health::Kill);     // void Kill()

// In gameplay:
healed.Emit(12.5f);
outOfHealth.Emit();
```

Register action methods on scripts or native ECS components explicitly with `RegisterSignalAction`. Actions return void
and take either no argument, a value, or a const reference. Overloaded methods need
an explicit member-function pointer cast or a small named wrapper. A signal may
call a no-argument action regardless of its emitted value. Typed actions require an
exact matching C++ type; there are no implicit float/int conversions.

Expand the signal in the Inspector and choose **Add listener**, drag a scene entity
into **Target**, then pick **Method**. Compatible typed methods have an **event value**
option. Float, int, bool, and string actions also have **constant** options with a
value field. Structured payloads use the event value. Every listener has an enabled
checkbox and a remove button. Missing bindings remain authored, show a warning,
and are skipped at runtime. `None` clears the selected method.

Bindings save the entity UUID, registered script/component name, method name, enabled state, argument
mode, and any constant. They resolve the current script or component at emission time. Prefab
copies remap internal entity targets using the scene reference resolver. Unknown
UUIDs are preserved for diagnosis. Function addresses, live script pointers, RTTI
identifiers, and native callbacks are not saved. Changing a registered method name
requires updating its authored bindings.

C++ can also connect temporary callbacks:

```cpp
auto connection = healed.Connect([](float amount) { /* update UI */ });
healed.Disconnect(connection);

// A receiver removed from the scene will be skipped safely:
outOfHealth.Connect(Canis::ScriptHandle<MyScript>(receiver), &MyScript::OnDeath);
```

Persistent callbacks run first in authored order, followed by native callbacks in
connection order. Emission snapshots listeners and copies the typed payload;
connections added or removed during a callback affect the next emission. Lambdas
that capture raw objects retain normal C++ lifetime responsibilities. ScriptHandle
connections and persistent actions defer script deletion and entity destruction until
safe cleanup. Native component removal itself is immediate, so a method that removes
its own component must avoid accessing its members afterwards.
Do not retain signals or entity references beyond their scene's lifetime. Emission
and editing run on the scene/main thread.

This is a native C++ action API; UI actions and animation events remain separate.
The script editor's property sync recognizes Signal fields and keeps their property
registrations when saving a header. Scene undo/redo, prefab serialization, and game
code reload use the same property codecs as other fields.

Native component registration uses the same Signal fields and RegisterSignalAction
calls, with DEFAULT_COMPONENT_CONFIG, DEFAULT_DRAW_COMPONENT_INSPECTOR, and
App::RegisterComponent. The saved listener field named `script` remains unchanged
for compatibility; it stores the registered type name for either kind of target.
