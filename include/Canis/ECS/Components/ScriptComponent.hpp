#pragma once

namespace Canis
{
    class ScriptableEntity;

    struct ScriptComponent
    {
        ScriptableEntity* Instance = nullptr;
        
        ScriptableEntity*(*InstantiateScript)();
        void (*DestroyScript)(ScriptComponent*);

        template<typename T>
        void Bind()
        {
            InstantiateScript = []() {
                T *t = new T();
                return static_cast<ScriptableEntity*>(t);
            };
            DestroyScript = [](ScriptComponent* scriptComponent) { delete (void*)scriptComponent->Instance; scriptComponent->Instance = nullptr; };
        }
    };
} // end of Canis namespace