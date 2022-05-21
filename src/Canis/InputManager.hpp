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

        void pressKey(unsigned int keyID);
        void releasedKey(unsigned int keyID);
        void swapMaps();

        bool isKeyPressed(unsigned int keyID);
        bool justPressedKey(unsigned int keyID);
        bool justReleasedKey(unsigned int keyID);

        glm::vec2 mouse = glm::vec2(0,0);
    private:
        bool isKeyUpInVec(std::vector<InputData> *arr, unsigned int key);
        bool isKeyDownInVec(std::vector<InputData> *arr, unsigned int value);
        int isInLastKnown(unsigned int value);
        bool isKeyDownInLastKnowVec(unsigned int value);

        std::vector<InputData> _keyVec;

        std::vector<InputData> lastKnown;

        bool keyVecIsOne = true;
    };
} // end of Canis namespace