#include "InputManager.hpp"

namespace Canis
{
    InputManager::InputManager()
    {

    }

    InputManager::~InputManager()
    {

    }

    void InputManager::pressKey(unsigned int keyID)
    {
        _keyMap[keyID] = true;
    }

    void InputManager::releasedKey(unsigned int keyID)
    {
        _keyMap[keyID] = false;
    }

    bool InputManager::isKeyPressed(unsigned int keyID)
    {
        std::unordered_map<unsigned int, bool>::iterator it = _keyMap.find(keyID);
        if(it != _keyMap.end())
        {
            return it->second;
        }
        else
        {
            return false;
        }
    }
} // end of Canis namespace