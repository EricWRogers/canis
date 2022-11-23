#pragma once

#include <Canis/Entity.hpp>

namespace Canis
{
class ScriptableEntity
{    
public:
    virtual ~ScriptableEntity() {}
    Entity m_Entity;

    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(float _dt) {}

    template<typename T>
    T& GetComponent()
    {
        return m_Entity.GetComponent<T>();
    }
};
} // end of Canis namespace