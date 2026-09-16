#pragma once
namespace Canis { class App; }
namespace Canis::Scripting
{
    void RegisterSceneBindings(App& app);
    void UnregisterSceneBindings();
}
