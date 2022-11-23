#pragma once
#include <Canis/ScriptableEntity.hpp>

namespace Canis
{
    struct ScriptComponent
    {
        ScriptableEntity* Instance = nullptr;
        
        ScriptableEntity*(*InstantiateScript)();
        void (*DestroyScript)(ScriptComponent*);

        template<typename T>
        void Bind()
        {
            InstantiateScript = []() { return static_cast<ScriptableEntity*>(new T()); };
            DestroyScript = [](ScriptComponent* scriptComponent) { delete scriptComponent->Instance; scriptComponent->Instance = nullptr; };
        }
    };
} // end of Canis namespace