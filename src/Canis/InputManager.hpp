#pragma once
#include <unordered_map>

namespace Canis
{
    class InputManager
    {
    public:
        InputManager();
        ~InputManager();

        void pressKey(unsigned int keyID);
        void releasedKey(unsigned int keyID);
        void swapMaps();

        bool isKeyPressed(unsigned int keyID);
        bool justPressedKey(unsigned int keyID);
        bool justReleasedKey(unsigned int keyID);
    private:
        std::unordered_map<unsigned int, bool> *_keyMap;
        std::unordered_map<unsigned int, bool> *_prevKeyMap;

        std::unordered_map<unsigned int, bool> _keyMapOne;
        std::unordered_map<unsigned int, bool> _keyMapTwo;

        bool keyMapIsOne = true;
    };
} // end of Canis namespace