#include "InputManager.hpp"

namespace Canis
{
    InputManager::InputManager()
    {
        _keyMap = &_keyMapOne;
        _prevKeyMap = &_keyMapTwo;
        keyMapIsOne = true;
    }

    InputManager::~InputManager()
    {

    }

    void InputManager::pressKey(unsigned int keyID)
    {
        (*_keyMap)[keyID] = true;
    }

    void InputManager::releasedKey(unsigned int keyID)
    {
        (*_keyMap)[keyID] = false;
    }

    void InputManager::swapMaps()
    {
        keyMapIsOne = !keyMapIsOne;

        _prevKeyMap->clear();

        if (keyMapIsOne)
        {
            _keyMap = &_keyMapTwo;
            _prevKeyMap = &_keyMapOne;
        }
        else
        {
            _keyMap = &_keyMapOne;
            _prevKeyMap = &_keyMapTwo;
        }
    }

    bool InputManager::isKeyPressed(unsigned int keyID)
    {
        std::unordered_map<unsigned int, bool>::iterator it = _keyMap->find(keyID);
        if(it != _keyMap->end())
        {
            return it->second;
        }
        else
        {
            return false;
        }
    }

    bool InputManager::justPressedKey(unsigned int keyID)
    {
        bool currentFrame = false;
        bool lastFrame = false;
        std::unordered_map<unsigned int, bool>::iterator it = _keyMap->find(keyID);
        if(it != _keyMap->end())
        {
            currentFrame = it->second;
        }
        else
        {
            currentFrame = false;
        }

        std::unordered_map<unsigned int, bool>::iterator itt = _prevKeyMap->find(keyID);
        if(itt != _prevKeyMap->end())
        {
            lastFrame = itt->second;
        }
        else
        {
            lastFrame = false;
        }

        if (currentFrame == true && lastFrame == false)
            return true;

        return false;
    }

    bool InputManager::justReleasedKey(unsigned int keyID)
    {
        bool currentFrame = false;
        bool lastFrame = false;
        std::unordered_map<unsigned int, bool>::iterator it = _keyMap->find(keyID);
        if(it != _keyMap->end())
        {
            currentFrame = it->second;
        }
        else
        {
            currentFrame = false;
        }

        std::unordered_map<unsigned int, bool>::iterator itt = _prevKeyMap->find(keyID);
        if(itt != _prevKeyMap->end())
        {
            lastFrame = itt->second;
        }
        else
        {
            lastFrame = false;
        }

        if (currentFrame == false && lastFrame == true)
            return true;

        return false;
    }
} // end of Canis namespace