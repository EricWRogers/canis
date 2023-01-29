#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace Canis
{
    struct InputData
    {
        unsigned int key;
        bool value;
    };
    class InputManager
    {
    public:
        InputManager();
        ~InputManager();

        void PressKey(unsigned int _keyID);
        void ReleasedKey(unsigned int _keyID);
        void SwapMaps();

        bool IsKeyPressed(unsigned int _keyID);
        bool JustPressedKey(unsigned int _keyID);
        bool JustReleasedKey(unsigned int _keyID);

        glm::vec2 mouse = glm::vec2(0,0);
        bool leftClick = false;
        bool rightClick = false;
    private:
        bool IsKeyUpInVec(std::vector<InputData> *_arr, unsigned int _key);
        bool IsKeyDownInVec(std::vector<InputData> *_arr, unsigned int _value);
        int IsInLastKnown(unsigned int _value);
        bool IsKeyDownInLastKnowVec(unsigned int _value);

        std::vector<InputData> m_keyVec;

        std::vector<InputData> m_lastKnown;

        bool m_keyVecIsOne = true;
    };
} // end of Canis namespace