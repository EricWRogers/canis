#pragma once
#include "State.hpp"

namespace Canis
{
    class StateMachine : public Canis::ScriptableEntity
    {
    protected:
        std::vector<State *> m_states = {};
        State *m_state = nullptr;

    public:
        void OnCreate() {}

        void OnReady() {}

        void OnDestroy()
        {
            for (State *s : m_states)
                delete s;
        }

        void OnUpdate(float _dt)
        {
            if (m_state != nullptr)
                m_state->Update(_dt);
        }

        void SetState(State *_state, std::unordered_map<std::string, std::string> &_message)
        {
            if (_state == nullptr)
                return;

            if (m_state != nullptr)
                m_state->Exit(_state->name);

            m_state = _state;
            m_state->Enter(_message);
        }

        void ChangeState(std::string _name, std::unordered_map<std::string, std::string> &_message)
        {
            for (State *s : m_states)
            {
                if (s->name == _name)
                {
                    SetState(s, _message);
                    return;
                }
            }

            Canis::Warning("State Not Found: " + _name);
        }

        void ChangeState(std::string _name)
        {
            std::unordered_map<std::string, std::string> message = {};

            ChangeState(_name, message);
        }
    };
} // End of Canis namespace