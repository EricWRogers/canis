#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>

namespace Canis
{
    enum ControllerButton
    {
        A               = 1u,
        B               = 2u,
        X               = 4u,
        Y               = 8u,
        BACK            = 16u,
        GUIDE           = 32u,
        START           = 64u,
        LEFTSTICK       = 128u,
        RIGHTSTICK      = 256u,
        LEFTSHOULDER    = 512u,
        RIGHTSHOULDER   = 1024u,
        DPAD_UP         = 2048u,
        DPAD_DOWN       = 4096u,
        DPAD_LEFT       = 8192u,
        DPAD_RIGHT      = 16384u
    };

    struct GameControllerData
    {
        glm::vec2 leftStick = glm::vec2(0.0f);
        glm::vec2 rightStick = glm::vec2(0.0f);
        float leftTrigger = 0.0f;
        float rightTrigger = 0.0f;
        unsigned int buttons = 0u;
    };
    struct GameController
    {
        void *controller = nullptr; // SDL_GameController
        unsigned int index = 0;
        GameControllerData currentData = {};
        GameControllerData oldData = {};
        float deadZone = 0.15f;
    };
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

        bool Update(int _screenWidth, int _screenHeight);

        void PressKey(unsigned int _keyID);
        void ReleasedKey(unsigned int _keyID);
        void SwapMaps();

        bool GetKey(unsigned int _keyID);
        bool GetButton(unsigned int _gameControllerId, unsigned int _buttonId);
        bool JustPressedKey(unsigned int _keyID);
        bool JustReleasedKey(unsigned int _keyID);

        glm::vec2 GetLeftStick(unsigned int _gameControllerId);
        glm::vec2 GetRightStick(unsigned int _gameControllerId);
        float GetLeftTrigger(unsigned int _gameControllerId);
        float GetRightTrigger(unsigned int _gameControllerId);

        glm::vec2 mouse = glm::vec2(0,0);
        bool leftClick = false;
        bool rightClick = false;
    private:
        bool IsKeyUpInVec(std::vector<InputData> *_arr, unsigned int _key);
        bool IsKeyDownInVec(std::vector<InputData> *_arr, unsigned int _value);
        int  IsInLastKnown(unsigned int _value);
        bool IsKeyDownInLastKnowVec(unsigned int _value);

        void OnGameControllerConnected(void *_device);
        void OnGameControllerDisconnect(void *_device);

        std::vector<InputData> m_keyVec;
        std::vector<InputData> m_lastKnown;
        std::vector<GameController> m_gameControllers = {};

        bool m_keyVecIsOne = true;
    };
} // end of Canis namespace