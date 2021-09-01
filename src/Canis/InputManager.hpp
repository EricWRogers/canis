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

        bool isKeyPressed(unsigned int keyID);
    private:
        std::unordered_map<unsigned int, bool> _keyMap;
    };
} // end of Canis namespace