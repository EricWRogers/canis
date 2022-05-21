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
        _keyVec.push_back(InputData { keyID , true});
    }

    void InputManager::releasedKey(unsigned int keyID)
    {
        _keyVec.push_back(InputData { keyID , false});
    }

    void InputManager::swapMaps()
    {
        int index;
        for (int i = 0; i < _keyVec.size(); i++)
        {
            index = isInLastKnown(_keyVec[i].key);
            if (index != -1)
            {
                lastKnown[index].value = _keyVec[i].value;
            }
            else
            {
                lastKnown.push_back(_keyVec[i]);
            }
        }

        _keyVec.clear();
    }

    bool InputManager::isKeyPressed(unsigned int keyID)
    {
        return isKeyDownInVec(&_keyVec, keyID);
    }

    bool InputManager::justPressedKey(unsigned int keyID)
    {
        bool currentValue = isKeyDownInVec(&_keyVec, keyID);

        bool lastKnownValue = false;

        int index = isInLastKnown(keyID);
        if (index != -1)
        {
            lastKnownValue = lastKnown[index].value;
        }

        if (currentValue && !lastKnownValue)
            return true;
        
        return false;       
    }

    bool InputManager::justReleasedKey(unsigned int keyID)
    {
        return isKeyUpInVec(&_keyVec, keyID);
    }

    bool InputManager::isKeyUpInVec(std::vector<InputData> *arr, unsigned int value)
    {
        for (int i = 0; i < arr->size(); i++)
        {
            if ((*arr)[i].key == value)
                return !(*arr)[i].value;
        }
        
        return false;
    }

    bool InputManager::isKeyDownInVec(std::vector<InputData> *arr, unsigned int value)
    {
        for (int i = 0; i < arr->size(); i++)
        {
            if ((*arr)[i].key == value)
                return (*arr)[i].value;
        }
        
        return false;
    }

    int InputManager::isInLastKnown(unsigned int value)
    {
        for (int i = 0; i < lastKnown.size(); i++)
        {
            if (lastKnown[i].key == value)
                return i;
        }

        return -1;
    }
    
    bool InputManager::isKeyDownInLastKnowVec(unsigned int value)
    {
        for (int i = 0; i < lastKnown.size(); i++)
        {
            if (lastKnown[i].key == value)
                return lastKnown[i].value;
        }
        
        return false;
    }
} // end of Canis namespace