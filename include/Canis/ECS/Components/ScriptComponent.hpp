#pragma once

namespace Canis
{
    class ScriptableEntity;

    struct ScriptComponent
    {
        ScriptableEntity* Instance = nullptr;
        
        ScriptableEntity*(*InstantiateScript)();

        template<typename T>
        void Bind()
        {
            InstantiateScript = []() {
                T *t = new T();
                return static_cast<ScriptableEntity*>(t);
            };
        }
    };
} // end of Canis namespace