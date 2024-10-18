#pragma once
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <SDL_keyboard.h>
#include <SDL_gamecontroller.h>

namespace Canis
{
    enum class InputDevice
    {
      MOUSE,
      KEYBOARD,
      GAMEPAD
    };

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
        SDL_GameController *controller = nullptr;
        unsigned int index = 0;
        SDL_JoystickID joyId;
        GameControllerData currentData = {};
        GameControllerData oldData = {};
        float deadZone = 0.2f;
        unsigned int lastButtonsPressed = 0;
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

        bool Update(int _screenWidth, int _screenHeight, void* _window);

        bool GetKey(unsigned int _keyID);
        bool JustPressedKey(unsigned int _keyID);
        bool JustReleasedKey(unsigned int _keyID);

        bool GetButton(unsigned int _gameControllerId, unsigned int _buttonId);
        bool JustPressedButton(unsigned int _gameControllerId, unsigned int _buttonId);
        bool JustReleasedButton(unsigned int _gameControllerId, unsigned int _buttonId);
        bool LastButtonsPressed(unsigned int _gameControllerId, unsigned int _buttonId);

        glm::vec2 GetLeftStick(unsigned int _gameControllerId);
        glm::vec2 GetRightStick(unsigned int _gameControllerId);
        float GetLeftTrigger(unsigned int _gameControllerId);
        float GetRightTrigger(unsigned int _gameControllerId);

        int VerticalScroll() { return m_scrollVertical; }

        bool GetLeftClick() { return  m_leftClick; }
        bool LeftClickReleased() { return  m_leftClick == false && m_wasLeftClick == true; }
        bool JustLeftClicked() { return  m_leftClick == true && m_wasLeftClick == false; }

        bool GetRightClick() { return  m_rightClick; }
        bool RightClickReleased() { return  m_rightClick == false && m_wasRightClick == true; }
        bool JustRightClicked() { return  m_rightClick == true && m_wasRightClick == false; }
        
        InputDevice GetLastDeviceType() { return m_lastInputDeviceType; }

        bool GetButton(unsigned int _buttonId) { return GetButton(m_lastControllerID, _buttonId); }
        bool JustPressedButton(unsigned int _buttonId) { return JustPressedButton(m_lastControllerID, _buttonId); }
        bool JustReleasedButton(unsigned int _buttonId) { return JustReleasedButton(m_lastControllerID, _buttonId); }
        bool LastButtonsPressed(unsigned int _buttonId) { return LastButtonsPressed(m_lastControllerID, _buttonId); }

        glm::vec2 GetLeftStick() { return GetLeftStick(m_lastControllerID); }
        glm::vec2 GetRightStick() { return GetRightStick(m_lastControllerID); }
        float GetLeftTrigger() { return GetLeftTrigger(m_lastControllerID); }
        float GetRightTrigger() { return GetRightTrigger(m_lastControllerID); }

        glm::vec2 mouse = glm::vec2(0,0);
        glm::vec2 mouseRel = glm::vec2(0);
        
    private:
        void PressKey(unsigned int _keyID);
        void ReleasedKey(unsigned int _keyID);
        void SwapMaps();

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

        bool m_leftClick = false;
        bool m_rightClick = false;
        bool m_wasLeftClick = false;
        bool m_wasRightClick = false;

        int m_scrollVertical = 0;

        unsigned int m_lastControllerID = 0u;

        InputDevice m_lastInputDeviceType = InputDevice::MOUSE;
    };
} // end of Canis namespace
