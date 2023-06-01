#pragma once
#include <Canis/ScriptableEntity.hpp>

#include "State.hpp"

class StateMachine : public Canis::ScriptableEntity
{
protected:
    std::vector<State*> m_states = {};
    State* m_state = nullptr;
public:
    void OnCreate() { }

    void OnReady() { }
    
    void OnDestroy()
    {
        for(State* s : m_states)
            delete s;
    }

    void OnUpdate(float _dt)
    {        
        if (m_state != nullptr)
            m_state->Update(*this, _dt);
    }

    void SetState(State *_state)
    {
        if (_state == nullptr)
            return;
        
        if (m_state != nullptr)
            m_state->Exit(*this);
        
        m_state = _state;
        m_state->Enter(*this);
    }

    void ChangeState(std::string _name)
    {
        for(State* s : m_states)
        {
            if(s->name == _name)
            {
                SetState(s);
                return;
            }
        }

        Canis::Warning("State Not Found: " + _name);
    }
};