#pragma once

#include <string>
#include <functional>

class State
{
protected:
    bool m_hasBeenEntered = false;
public:
    std::string name = "";
    std::function<void(std::string _name)> ChangeState = nullptr;

    State(std::function<void(std::string _name)> _changeState, std::string _name) {
        ChangeState = _changeState;
        name = _name;
    }

    virtual void Enter(Canis::ScriptableEntity &_scriptableEntity) {
        m_hasBeenEntered = true;
    }
    
    virtual void Update(Canis::ScriptableEntity &_scriptableEntity, float _deltaTime) {

    }
    
    virtual void Exit(Canis::ScriptableEntity &_scriptableEntity) {

    }
};