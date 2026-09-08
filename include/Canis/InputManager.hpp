#pragma once
#include <Canis/Math.hpp>
#include <Canis/InputActions.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
        NINTENDO,
        STEAM,
        UNKNOWN
    };

    struct GameControllerData
    {
        Vector2 leftStick = Vector2(0.0f);
        Vector2 rightStick = Vector2(0.0f);
        float leftTrigger = 0.0f;
        float rightTrigger = 0.0f;
        unsigned int buttons = 0u;
    };
    struct GameController
    {
        void *controller = nullptr;
        GameControllerType gameControllerType = GameControllerType::UNKNOWN;
        bool originalSteamController = false;
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

        // Action snapshots are published once, after SDL and synthetic collection.
        InputActionSystem& Actions() { return m_actions; }
        const InputActionSystem& Actions() const { return m_actions; }
        ActionSnapshot Action(ActionId id) const { return m_actions.Action(id); }
        template<class T> requires InputActionEnum<T>::value
        ActionSnapshot Action(T id) const { return Action(ActionId{static_cast<uint32_t>(id)}); }
        // Same zero-based index as the raw gamepad API; indices can shift on disconnect.
        ActionSnapshot Action(ActionId id, unsigned int controllerIndex) const;
        template<class T> requires InputActionEnum<T>::value
        ActionSnapshot Action(T id, unsigned int controllerIndex) const { return Action(ActionId{static_cast<uint32_t>(id)}, controllerIndex); }
        void EnableMap(InputMapId id) { m_actions.EnableMap(id); }
        void DisableMap(InputMapId id) { m_actions.DisableMap(id); }
        template<class T> requires InputMapEnum<T>::value
        void EnableMap(T id) { EnableMap(InputMapId{static_cast<uint32_t>(id)}); }
        template<class T> requires InputMapEnum<T>::value
        void DisableMap(T id) { DisableMap(InputMapId{static_cast<uint32_t>(id)}); }
        std::vector<InputBinding> Bindings(ActionId id) const { return m_actions.Bindings(id); }
        InputPrompt Prompt(ActionId id, InputScheme scheme) const { return m_actions.Prompt(id, scheme); }
        template<class T> requires InputActionEnum<T>::value
        std::vector<InputBinding> Bindings(T id) const { return m_actions.Bindings({static_cast<uint32_t>(id)}); }
        template<class T> requires InputActionEnum<T>::value
        InputPrompt Prompt(T id, InputScheme scheme) const { return m_actions.Prompt({static_cast<uint32_t>(id)}, scheme); }
        void EvaluateActions(bool gameplayActive = true);
        GameControllerType GetControllerType() const;
        bool Update(void* _window);
        void BeginSyntheticInputFrame();
        void SetSyntheticKey(unsigned int _keyID, bool _down);
        void SetSyntheticMouseButton(unsigned int _button, bool _down);
        void AddSyntheticMouseDelta(Vector2 _delta);
        void SetSyntheticMousePosition(Vector2 _position);
        void AddSyntheticMouseWheel(int _amount);
        void SetSyntheticGamepadButton(unsigned int _button, bool _down);
        void SetSyntheticGamepadLeftStick(Vector2 _value);
        void SetSyntheticGamepadRightStick(Vector2 _value);
        void SetSyntheticGamepadTriggers(float _left, float _right);
        bool ConsumeResumeFrameResetRequest()
        {
            const bool requested = m_resumeFrameResetRequested;
            m_resumeFrameResetRequested = false;
            return requested;
        }
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

        int VerticalScroll() { return (active) ? m_scrollVertical + m_syntheticScrollVertical : 0; }

        bool GetLeftClick();
        bool LeftClickReleased();
        bool JustLeftClicked();

        bool GetRightClick();
        bool RightClickReleased();
        bool JustRightClicked();
        bool GetUnfilteredRightClick() const { return m_unfilteredRightClick; }
        Vector2 GetUnfilteredMouseDelta() const { return m_unfilteredMouseRel; }
        
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
        const std::string& GetTextInput() const { return m_textInput; }

        bool active = true;
        
    private:
        InputActionSystem m_actions;
        std::unordered_set<uint64_t> m_steamOwnedSources;
        bool m_actionEditorCaptured = false;
        bool m_actionsWereAvailable = true;
        void ClearSyntheticState(InputCancellation reason);
        void PressKey(unsigned int _keyID);
        void ReleasedKey(unsigned int _keyID);
        void SwapMaps();

        bool IsKeyUpInVec(std::vector<InputData> *_arr, unsigned int _key);
        bool IsKeyDownInVec(std::vector<InputData> *_arr, unsigned int _value);
        int  IsInLastKnown(unsigned int _value);
        bool IsKeyDownInLastKnowVec(unsigned int _value);

        void OnGameControllerConnected(void *_device);
        void OnGameControllerDisconnect(void *_device);
        void ResetState();
        bool GameViewportContainsPoint(float _x, float _y) const;
        bool AcceptsGameMouseEvent(unsigned int _eventWindowID, float _x, float _y, unsigned int _gameWindowID, bool _mouseLocked) const;
        void UpdateMousePosition(unsigned int _eventWindowID, float _x, float _y, int _screenHeight, unsigned int _gameWindowID);

        std::vector<InputData> m_keyVec;
        std::vector<InputData> m_lastKnown;
        std::vector<GameController> m_gameControllers = {};

        bool m_keyVecIsOne = true;

        bool m_leftClick = false;
        bool m_rightClick = false;
        bool m_wasLeftClick = false;
        bool m_wasRightClick = false;
        bool m_unfilteredRightClick = false;
        Vector2 m_unfilteredMouseRel = Vector2(0.0f);

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
        std::string m_textInput = "";
        bool m_windowWasBackgrounded = false;
        bool m_resumeFrameResetRequested = false;

        std::unordered_map<unsigned int, bool> m_syntheticKeys = {};
        std::unordered_map<unsigned int, bool> m_previousSyntheticKeys = {};
        bool m_syntheticLeftClick = false;
        bool m_previousSyntheticLeftClick = false;
        bool m_syntheticRightClick = false;
        bool m_previousSyntheticRightClick = false;
        int m_syntheticScrollVertical = 0;
        GameControllerData m_syntheticGamepad = {};
        GameControllerData m_previousSyntheticGamepad = {};
        bool m_syntheticGamepadEnabled = false;
    };
} // end of Canis namespace
