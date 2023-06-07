#pragma once

#include <string>
#include <functional>

#include <Canis/ScriptableEntity.hpp>

namespace Canis
{
    class State
    {
    protected:
        bool m_hasBeenEntered = false;

    public:
        std::string name = "";
        std::function<void(std::string _name)> ChangeState = nullptr;
        Canis::ScriptableEntity *scriptableEntity;


        State(Canis::ScriptableEntity *_scriptableEntity, std::function<void(std::string _name)> _changeState, std::string _name)
        {
            scriptableEntity = _scriptableEntity;
            ChangeState = _changeState;
            name = _name;
        }

        virtual void Enter()
        {
            m_hasBeenEntered = true;
        }

        virtual void Update(float _deltaTime)
        {
        }

        virtual void Exit(std::string _nextStateName)
        {
        }
    };
} // End of Canis namespace