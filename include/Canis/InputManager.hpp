#pragma once
#include <Canis/Math.hpp>
#include <vector>
#include <Canis/Data/Key.hpp>


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

    enum GameControllerType
    {
        XBOX,
        PLAYSTATION,
        NINTENDO
    };

    struct GameControllerData
    {
        Vector2 leftStick;
        Vector2 rightStick;
        float leftTrigger = 0.0f;
        float rightTrigger = 0.0f;
        unsigned int buttons = 0u;
    };
    struct GameController
    {
        void *controller = nullptr;
        GameControllerType gameControllerType = GameControllerType::XBOX;
        unsigned int index = 0;
        unsigned int joyId;
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

        bool Update(void* _window);
        void SetGameInputWindowID(unsigned int _windowID) { m_gameInputWindowID = _windowID; }
        void SetGameMouseViewport(float _x, float _y, float _drawWidth, float _drawHeight, float _logicalWidth, float _logicalHeight);
        void ClearGameMouseViewport();

        bool GetKey(unsigned int _keyID);
        bool JustPressedKey(unsigned int _keyID);
        bool JustReleasedKey(unsigned int _keyID);

        bool GetButton(unsigned int _gameControllerId, unsigned int _buttonId);
        bool JustPressedButton(unsigned int _gameControllerId, unsigned int _buttonId);
        bool JustReleasedButton(unsigned int _gameControllerId, unsigned int _buttonId);
        bool LastButtonsPressed(unsigned int _gameControllerId, unsigned int _buttonId);

        Vector2 GetLeftStick(unsigned int _gameControllerId);
        Vector2 GetRightStick(unsigned int _gameControllerId);
        float GetLeftTrigger(unsigned int _gameControllerId);
        float GetRightTrigger(unsigned int _gameControllerId);

        int VerticalScroll() { return (active) ? m_scrollVertical : 0; }

        bool GetLeftClick() { return  m_leftClick && active; }
        bool LeftClickReleased() { return  m_leftClick == false && m_wasLeftClick == true && active; }
        bool JustLeftClicked() { return  m_leftClick == true && m_wasLeftClick == false && active; }

        bool GetRightClick() { return  m_rightClick && active; }
        bool RightClickReleased() { return  m_rightClick == false && m_wasRightClick == true && active; }
        bool JustRightClicked() { return  m_rightClick == true && m_wasRightClick == false && active; }
        
        InputDevice GetLastDeviceType() { return m_lastInputDeviceType; }

        bool GetButton(unsigned int _buttonId) { return GetButton(m_lastControllerID, _buttonId); }
        bool JustPressedButton(unsigned int _buttonId) { return JustPressedButton(m_lastControllerID, _buttonId); }
        bool JustReleasedButton(unsigned int _buttonId) { return JustReleasedButton(m_lastControllerID, _buttonId); }
        bool LastButtonsPressed(unsigned int _buttonId) { return LastButtonsPressed(m_lastControllerID, _buttonId); }

        Vector2 GetLeftStick() { return GetLeftStick(m_lastControllerID); }
        Vector2 GetRightStick() { return GetRightStick(m_lastControllerID); }
        float GetLeftTrigger() { return GetLeftTrigger(m_lastControllerID); }
        float GetRightTrigger() { return GetRightTrigger(m_lastControllerID); }

        Vector2 mouse;
        Vector2 mouseRel;

        bool active = true;
        
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
        unsigned int m_gameInputWindowID = 0u;
        bool m_gameMouseViewportEnabled = false;
        float m_gameMouseViewportX = 0.0f;
        float m_gameMouseViewportY = 0.0f;
        float m_gameMouseViewportDrawWidth = 0.0f;
        float m_gameMouseViewportDrawHeight = 0.0f;
        float m_gameMouseViewportLogicalWidth = 0.0f;
        float m_gameMouseViewportLogicalHeight = 0.0f;
    };
} // end of Canis namespace
