#include <Canis/InputManager.hpp>

namespace Canis
{
    InputManager::InputManager()
    {
        
    }

    InputManager::~InputManager()
    {

    }

    void InputManager::PressKey(unsigned int _keyID)
    {
        m_keyVec.push_back(InputData { _keyID , true});
    }

    void InputManager::ReleasedKey(unsigned int _keyID)
    {
        m_keyVec.push_back(InputData { _keyID , false});
    }

    void InputManager::SwapMaps()
    {
        leftClick = false;
        rightClick = false;

        int index;
        for (int i = 0; i < m_keyVec.size(); i++)
        {
            index = IsInLastKnown(m_keyVec[i].key);
            if (index != -1)
            {
                m_lastKnown[index].value = m_keyVec[i].value;
            }
            else
            {
                m_lastKnown.push_back(m_keyVec[i]);
            }
        }

        m_keyVec.clear();
    }

    bool InputManager::IsKeyPressed(unsigned int _keyID)
    {
        return IsKeyDownInVec(&m_keyVec, _keyID);
    }

    bool InputManager::JustPressedKey(unsigned int _keyID)
    {
        bool currentValue = IsKeyDownInVec(&m_keyVec, _keyID);

        bool lastKnownValue = false;

        int index = IsInLastKnown(_keyID);
        if (index != -1)
        {
            lastKnownValue = m_lastKnown[index].value;
        }

        if (currentValue && !lastKnownValue)
            return true;
        
        return false;       
    }

    bool InputManager::JustReleasedKey(unsigned int _keyID)
    {
        return IsKeyUpInVec(&m_keyVec, _keyID);
    }

    bool InputManager::IsKeyUpInVec(std::vector<InputData> *_arr, unsigned int _value)
    {
        for (int i = 0; i < _arr->size(); i++)
        {
            if ((*_arr)[i].key == _value)
                return !(*_arr)[i].value;
        }
        
        return false;
    }

    bool InputManager::IsKeyDownInVec(std::vector<InputData> *_arr, unsigned int _value)
    {
        for (int i = 0; i < _arr->size(); i++)
        {
            if ((*_arr)[i].key == _value)
                return (*_arr)[i].value;
        }
        
        return false;
    }

    int InputManager::IsInLastKnown(unsigned int _value)
    {
        for (int i = 0; i < m_lastKnown.size(); i++)
        {
            if (m_lastKnown[i].key == _value)
                return i;
        }

        return -1;
    }
    
    bool InputManager::IsKeyDownInLastKnowVec(unsigned int _value)
    {
        for (int i = 0; i < m_lastKnown.size(); i++)
        {
            if (m_lastKnown[i].key == _value)
                return m_lastKnown[i].value;
        }
        
        return false;
    }
} // end of Canis namespace