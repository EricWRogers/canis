#include <Canis/InputManager.hpp>
#include <bit>
#include <Canis/SteamInput.hpp>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_events.h>
#include <Canis/Debug.hpp>
#include <Canis/Canis.hpp>
#if CANIS_EDITOR
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#endif
#include <Canis/Window.hpp>

namespace Canis
{
    namespace
    {
        Uint32 GetEventWindowID(const SDL_Event& _event)
        {
            if (_event.type >= SDL_EVENT_WINDOW_FIRST && _event.type <= SDL_EVENT_WINDOW_LAST)
                return _event.window.windowID;

            switch (_event.type)
            {
                case SDL_EVENT_MOUSE_MOTION:
                    return _event.motion.windowID;
                case SDL_EVENT_MOUSE_WHEEL:
                    return _event.wheel.windowID;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                case SDL_EVENT_MOUSE_BUTTON_UP:
                    return _event.button.windowID;
                case SDL_EVENT_KEY_DOWN:
                case SDL_EVENT_KEY_UP:
                    return _event.key.windowID;
                case SDL_EVENT_TEXT_INPUT:
                    return _event.text.windowID;
                default:
                    return 0u;
            }
        }

        bool IsMouseInputEvent(const SDL_Event& _event)
        {
            return _event.type == SDL_EVENT_MOUSE_MOTION ||
                   _event.type == SDL_EVENT_MOUSE_WHEEL ||
                   _event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                   _event.type == SDL_EVENT_MOUSE_BUTTON_UP;
        }
    } // namespace

    InputManager::InputManager()
        {
            
        }

    void InputManager::ResetState()
    {
        m_actions.Cancel(InputCancellation::FocusLost);
        ClearSyntheticState(InputCancellation::FocusLost);
        // Physical samples remain known until SDL reports neutral after focus returns.
        m_keyVec.clear();
        m_lastKnown.clear();
        mouseRel = Vector2(0.0f);
        m_scrollVertical = 0;
        m_textInput.clear();
        m_leftClick = false;
        m_rightClick = false;
        m_wasLeftClick = false;
        m_wasRightClick = false;
        m_unfilteredRightClick = false;
        m_unfilteredMouseRel = Vector2(0.0f);

        for (GameController &controller : m_gameControllers)
        {
            controller.currentData = {};
            controller.oldData = {};
            controller.lastButtonsPressed = 0u;
        }
    }

    void InputManager::ClearSyntheticState(InputCancellation reason)
    {
        m_actions.RemoveSource(1, reason);
        m_syntheticKeys.clear(); m_previousSyntheticKeys.clear();
        m_syntheticLeftClick = m_previousSyntheticLeftClick = false;
        m_syntheticRightClick = m_previousSyntheticRightClick = false;
        m_syntheticGamepad = {}; m_previousSyntheticGamepad = {};
        m_syntheticGamepadEnabled = false;
        m_syntheticScrollVertical = 0;
    }

    InputManager::~InputManager()
    {
        while(m_gameControllers.size())
        {
            SDL_CloseGamepad((SDL_Gamepad*)(m_gameControllers.begin()->controller));
            m_gameControllers.erase( m_gameControllers.begin() );
        }
    }

    void InputManager::BeginSyntheticInputFrame()
    {
        m_previousSyntheticKeys = m_syntheticKeys;
        m_previousSyntheticLeftClick = m_syntheticLeftClick;
        m_previousSyntheticRightClick = m_syntheticRightClick;
        m_previousSyntheticGamepad = m_syntheticGamepad;
        m_syntheticScrollVertical = 0;
    }

    void InputManager::SetSyntheticKey(unsigned int _keyID, bool _down)
    {
        for (const auto& c : InputControls()) if (c.key == _keyID && c.key != 0)
            m_actions.Record(c.path, Vector2(_down ? 1.0f : 0.0f, 0), 1);
        m_syntheticKeys[_keyID] = _down;
        m_lastInputDeviceType = InputDevice::KEYBOARD;
    }

    void InputManager::SetSyntheticMouseButton(unsigned int _button, bool _down)
    {
        const char* names[] = {"", "Mouse/Left", "Mouse/Middle", "Mouse/Right", "Mouse/X1", "Mouse/X2"};
        if (_button > 0 && _button < 6) m_actions.Record(names[_button], Vector2(_down ? 1.0f : 0.0f, 0), 1);
        if (_button == 1u)
            m_syntheticLeftClick = _down;
        else if (_button == 3u)
            m_syntheticRightClick = _down;
        m_lastInputDeviceType = InputDevice::MOUSE;
    }

    void InputManager::AddSyntheticMouseDelta(Vector2 _delta)
    {
        mouseRel += _delta;
        m_lastInputDeviceType = InputDevice::MOUSE;
    }

    void InputManager::SetSyntheticMousePosition(Vector2 _position)
    {
        mouse = _position;
        m_lastInputDeviceType = InputDevice::MOUSE;
    }

    void InputManager::AddSyntheticMouseWheel(int _amount)
    {
        m_syntheticScrollVertical += _amount;
        m_lastInputDeviceType = InputDevice::MOUSE;
    }

    void InputManager::SetSyntheticGamepadButton(unsigned int _button, bool _down)
    {
        m_syntheticGamepadEnabled = true;
        for (const auto& c : InputControls()) if (c.button && (_button & c.button))
            m_actions.Record(c.path, Vector2(_down ? 1.0f : 0.0f, 0), 1);
        if (_down)
            m_syntheticGamepad.buttons |= _button;
        else
            m_syntheticGamepad.buttons &= ~_button;
        m_lastInputDeviceType = InputDevice::GAMEPAD;
    }

    void InputManager::SetSyntheticGamepadLeftStick(Vector2 _value)
    {
        m_syntheticGamepadEnabled = true;
        m_syntheticGamepad.leftStick = glm::clamp(_value, Vector2(-1.0f), Vector2(1.0f));
        m_actions.Record("Gamepad/LeftStick", m_syntheticGamepad.leftStick, 1);
        m_lastInputDeviceType = InputDevice::GAMEPAD;
    }

    void InputManager::SetSyntheticGamepadRightStick(Vector2 _value)
    {
        m_syntheticGamepadEnabled = true;
        m_syntheticGamepad.rightStick = glm::clamp(_value, Vector2(-1.0f), Vector2(1.0f));
        m_actions.Record("Gamepad/RightStick", m_syntheticGamepad.rightStick, 1);
        m_lastInputDeviceType = InputDevice::GAMEPAD;
    }

    void InputManager::SetSyntheticGamepadTriggers(float _left, float _right)
    {
        m_syntheticGamepadEnabled = true;
        m_syntheticGamepad.leftTrigger = glm::clamp(_left, 0.0f, 1.0f);
        m_syntheticGamepad.rightTrigger = glm::clamp(_right, 0.0f, 1.0f);
        m_actions.Record("Gamepad/LeftTrigger", Vector2(m_syntheticGamepad.leftTrigger, 0), 1);
        m_actions.Record("Gamepad/RightTrigger", Vector2(m_syntheticGamepad.rightTrigger, 0), 1);
        m_lastInputDeviceType = InputDevice::GAMEPAD;
    }

    bool InputManager::GameViewportContainsPoint(float _x, float _y) const
    {
        if (!m_gameMouseViewportEnabled)
            return true;

        return _x >= m_gameMouseViewportX &&
               _y >= m_gameMouseViewportY &&
               _x < m_gameMouseViewportX + m_gameMouseViewportDrawWidth &&
               _y < m_gameMouseViewportY + m_gameMouseViewportDrawHeight;
    }

    bool InputManager::AcceptsGameMouseEvent(unsigned int _eventWindowID, float _x, float _y, unsigned int _gameWindowID, bool _mouseLocked) const
    {
        if (!Canis::IsEditorRuntimeEnabled())
            return true;

        if (_eventWindowID != _gameWindowID)
            return false;

        if (_mouseLocked)
            return true;

        return GameViewportContainsPoint(_x, _y);
    }

    void InputManager::UpdateMousePosition(unsigned int _eventWindowID, float _x, float _y, int _screenHeight, unsigned int _gameWindowID)
    {
        if (_eventWindowID == _gameWindowID &&
            m_gameMouseViewportEnabled &&
            m_gameMouseViewportDrawWidth > 0.0f &&
            m_gameMouseViewportDrawHeight > 0.0f &&
            m_gameMouseViewportLogicalWidth > 0.0f &&
            m_gameMouseViewportLogicalHeight > 0.0f)
        {
            const float localX = _x - m_gameMouseViewportX;
            const float localY = _y - m_gameMouseViewportY;
            const float scaleX = m_gameMouseViewportLogicalWidth / m_gameMouseViewportDrawWidth;
            const float scaleY = m_gameMouseViewportLogicalHeight / m_gameMouseViewportDrawHeight;

            mouse.x = localX * scaleX;
            mouse.y = m_gameMouseViewportLogicalHeight - (localY * scaleY);
        }
        else
        {
            mouse.x = _x;
            mouse.y = _screenHeight - _y;
        }
    }

    bool InputManager::Update(void* _window)
    {
        SwapMaps();
        mouseRel = Vector2(0.0f);
        m_unfilteredMouseRel = Vector2(0.0f);
        m_scrollVertical = 0;
        m_textInput.clear();

        Window* window = (Window*)_window;
#if CANIS_EDITOR
        m_actionEditorCaptured = Canis::IsEditorRuntimeEnabled() && ImGui::GetCurrentContext() &&
            (ImGui::GetIO().WantTextInput || (ImGui::GetIO().WantCaptureKeyboard && !window->IsMouseLocked()));
#endif
        if (window->IsMouseLocked())
            window->RefreshMouseLock();
        int screenWidth = window->GetWindowWidth();
        int screenHeight = window->GetWindowHeight();
        window->SetResized(false);
        const Uint32 mainWindowID = SDL_GetWindowID((SDL_Window*)window->GetSDLWindow());
        const Uint32 gameWindowID = (m_gameInputWindowID == 0u) ? mainWindowID : m_gameInputWindowID;

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            const Uint32 eventWindowID = GetEventWindowID(event);

            #if CANIS_EDITOR
            bool imguiWantsMouse = false;
            bool imguiWantsKeyboard = false;
            if (Canis::IsEditorRuntimeEnabled())
            {
                // Relative mouse capture belongs exclusively to the running
                // game. Do not let its invisible pointer move, scroll, or
                // press editor widgets behind the Game panel. Button-up is
                // still forwarded so the backend cannot retain a stale press
                // that began before capture was enabled.
                const bool gameplayOwnsMouse = window->IsMouseLocked();
                const bool releaseEvent =
                    event.type == SDL_EVENT_MOUSE_BUTTON_UP;
                if (!gameplayOwnsMouse || !IsMouseInputEvent(event) || releaseEvent)
                    ImGui_ImplSDL3_ProcessEvent(&event);
                ImGuiIO& io = ImGui::GetIO();
                imguiWantsMouse = io.WantCaptureMouse;
                imguiWantsKeyboard = io.WantCaptureKeyboard;
            }
            #endif

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && eventWindowID == mainWindowID)
                return false;

            switch (event.type)
            {
            case SDL_EVENT_QUIT:
                return false;
                break;
            case SDL_EVENT_WINDOW_HIDDEN:
            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_WINDOW_OCCLUDED:
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                m_unfilteredRightClick = false;
                if (eventWindowID == mainWindowID)
                {
                    ResetState();
                    m_windowWasBackgrounded = true;
                }
                break;
            case SDL_EVENT_WINDOW_SHOWN:
            case SDL_EVENT_WINDOW_EXPOSED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
                if (eventWindowID == mainWindowID)
                {
                    if (m_windowWasBackgrounded)
                        m_resumeFrameResetRequested = true;
                    m_windowWasBackgrounded = false;
                }
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                if(eventWindowID == mainWindowID) {
                    screenWidth = event.window.data1;
                    screenHeight = event.window.data2;
                    window->SetWindowSize(screenWidth, screenHeight);
                    if (m_windowWasBackgrounded)
                    {
                        m_resumeFrameResetRequested = true;
                        m_windowWasBackgrounded = false;
                    }
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                    m_unfilteredMouseRel.x += event.motion.xrel;
                    m_unfilteredMouseRel.y += event.motion.yrel;
                #if CANIS_EDITOR
                if (imguiWantsMouse && eventWindowID != mainWindowID && eventWindowID != gameWindowID)
                    continue;
                #endif
                    if (!AcceptsGameMouseEvent(eventWindowID, event.motion.x, event.motion.y, gameWindowID, window->IsMouseLocked()))
                        continue;

                    UpdateMousePosition(eventWindowID, event.motion.x, event.motion.y, screenHeight, gameWindowID);
                    // SDL may queue several relative-motion events between rendered
                    // frames. Accumulate all of them so mouse look does not become
                    // slower when the frame rate drops.
                    mouseRel.x += event.motion.xrel;
                    mouseRel.y += event.motion.yrel;
                    
                    m_lastInputDeviceType = (mouseRel != Vector2(0.0f)) ? InputDevice::MOUSE : m_lastInputDeviceType;
                break;
            case SDL_EVENT_MOUSE_WHEEL:
            #if CANIS_EDITOR
                if (imguiWantsMouse && eventWindowID != mainWindowID && eventWindowID != gameWindowID)
                    continue;
                #endif
                if (!AcceptsGameMouseEvent(eventWindowID, event.wheel.mouse_x, event.wheel.mouse_y, gameWindowID, window->IsMouseLocked()))
                    continue;
                UpdateMousePosition(eventWindowID, event.wheel.mouse_x, event.wheel.mouse_y, screenHeight, gameWindowID);
                m_scrollVertical = event.wheel.y;
                
                m_lastInputDeviceType = (m_scrollVertical != 0.0f) ? InputDevice::MOUSE : m_lastInputDeviceType;
                break;
            case SDL_EVENT_KEY_UP:
                #if CANIS_EDITOR
                if (imguiWantsKeyboard && eventWindowID != mainWindowID && eventWindowID != gameWindowID)
                    continue;
                #endif
                ReleasedKey(event.key.scancode);
                break;
            case SDL_EVENT_KEY_DOWN:
                #if CANIS_EDITOR
                if (imguiWantsKeyboard && eventWindowID != mainWindowID && eventWindowID != gameWindowID)
                    continue;
                #endif
                PressKey(event.key.scancode);
                m_lastInputDeviceType = InputDevice::KEYBOARD;
                break;
            case SDL_EVENT_TEXT_INPUT:
                #if CANIS_EDITOR
                if (imguiWantsKeyboard && eventWindowID != mainWindowID && eventWindowID != gameWindowID)
                    continue;
                #endif
                if ((eventWindowID == mainWindowID || eventWindowID == gameWindowID) && event.text.text != nullptr)
                {
                    m_textInput += event.text.text;
                    m_lastInputDeviceType = InputDevice::KEYBOARD;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_RIGHT)
                    m_unfilteredRightClick = true;
                #if CANIS_EDITOR
                if (imguiWantsMouse && eventWindowID != mainWindowID && eventWindowID != gameWindowID)
                    continue;
                #endif
                if (!AcceptsGameMouseEvent(eventWindowID, event.button.x, event.button.y, gameWindowID, window->IsMouseLocked()))
                    continue;
                UpdateMousePosition(eventWindowID, event.button.x, event.button.y, screenHeight, gameWindowID);
                {
                    const char* names[] = {"", "Mouse/Left", "Mouse/Middle", "Mouse/Right", "Mouse/X1", "Mouse/X2"};
                    if (event.button.button > 0 && event.button.button < 6)
                        m_actions.Record(names[event.button.button], Vector2(1, 0));
                }
                if (event.button.button == SDL_BUTTON_LEFT)
                    m_leftClick = true;
                if (event.button.button == SDL_BUTTON_RIGHT)
                    m_rightClick = true;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event.button.button == SDL_BUTTON_RIGHT)
                    m_unfilteredRightClick = false;
                #if CANIS_EDITOR
                if (imguiWantsMouse && eventWindowID != mainWindowID && eventWindowID != gameWindowID)
                    continue;
                #endif
                if (Canis::IsEditorRuntimeEnabled() && eventWindowID != gameWindowID)
                    continue;
                UpdateMousePosition(eventWindowID, event.button.x, event.button.y, screenHeight, gameWindowID);
                {
                    const char* names[] = {"", "Mouse/Left", "Mouse/Middle", "Mouse/Right", "Mouse/X1", "Mouse/X2"};
                    if (event.button.button > 0 && event.button.button < 6)
                        m_actions.Record(names[event.button.button], Vector2(0, 0));
                }
                if (event.button.button == SDL_BUTTON_LEFT)
                    m_leftClick = false;
                if (event.button.button == SDL_BUTTON_RIGHT)
                    m_rightClick = false;
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
                m_lastInputDeviceType = InputDevice::GAMEPAD;
                OnGameControllerConnected(&event.cdevice);
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                OnGameControllerDisconnect(&event.cdevice);
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
            {
                const uint64_t source = uint64_t(event.gaxis.which) + 2;
                const float value = glm::clamp(event.gaxis.value / 32767.0f, -1.0f, 1.0f);
                switch (event.gaxis.axis) {
                    case SDL_GAMEPAD_AXIS_LEFTX: m_actions.RecordComponent("Gamepad/LeftStick",value,0,source); break;
                    case SDL_GAMEPAD_AXIS_LEFTY: m_actions.RecordComponent("Gamepad/LeftStick",-value,1,source); break;
                    case SDL_GAMEPAD_AXIS_RIGHTX: m_actions.RecordComponent("Gamepad/RightStick",value,0,source); break;
                    case SDL_GAMEPAD_AXIS_RIGHTY: m_actions.RecordComponent("Gamepad/RightStick",-value,1,source); break;
                    case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: m_actions.RecordComponent("Gamepad/LeftTrigger",value,0,source); break;
                    case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: m_actions.RecordComponent("Gamepad/RightTrigger",value,0,source); break;
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                for (const auto& c : InputControls()) if (c.button && event.gbutton.button < 32 && c.button == (1u << event.gbutton.button))
                    m_actions.Record(c.path, Vector2(event.gbutton.down ? 1.0f : 0.0f, 0), uint64_t(event.gbutton.which) + 2);
                if (event.gbutton.down) m_lastInputDeviceType = InputDevice::GAMEPAD;
                break;
            }
        }

        // update controllers
        int controllerID = 0;
        for(auto it = m_gameControllers.begin(); it != m_gameControllers.end(); it++)
        {
            if (it->controller)
            {
                it->oldData                     = it->currentData;
                it->currentData.buttons         = 0u;
                it->currentData.leftStick       = Vector2(0.0f);
                it->currentData.rightStick      = Vector2(0.0f);
                it->currentData.leftTrigger     = 0.0f;
                it->currentData.rightTrigger    = 0.0f;

                for (unsigned int i = 0; i < 15; i++) // 14 is the last button i care about for now
                {
                    // next line is cool
                    it->currentData.buttons |= ((1 << i) * SDL_GetGamepadButton((SDL_Gamepad*)it->controller, (SDL_GamepadButton)i));
                }

                if (it->currentData.buttons != 0)
                {
                    m_lastInputDeviceType = InputDevice::GAMEPAD;
                    it->lastButtonsPressed = it->currentData.buttons;
                    m_lastControllerID = controllerID;
                }

                it->currentData.leftStick.x     = SDL_GetGamepadAxis((SDL_Gamepad*)it->controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_LEFTX)/32767.0f;
                it->currentData.leftStick.x     = (abs(it->currentData.leftStick.x) < it->deadZone) ? 0.0f : it->currentData.leftStick.x;

                it->currentData.leftStick.y     = SDL_GetGamepadAxis((SDL_Gamepad*)it->controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_LEFTY)/32767.0f;
                it->currentData.leftStick.y     = (abs(it->currentData.leftStick.y) < it->deadZone) ? 0.0f : -(it->currentData.leftStick.y);

                it->currentData.rightStick.x    = SDL_GetGamepadAxis((SDL_Gamepad*)it->controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_RIGHTX)/32767.0f;
                it->currentData.rightStick.x    = (abs(it->currentData.rightStick.x) < it->deadZone) ? 0.0f : it->currentData.rightStick.x;

                it->currentData.rightStick.y    = SDL_GetGamepadAxis((SDL_Gamepad*)it->controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_RIGHTY)/32767.0f;
                it->currentData.rightStick.y    = (abs(it->currentData.rightStick.y) < it->deadZone) ? 0.0f : -(it->currentData.rightStick.y);

                it->currentData.rightTrigger    = SDL_GetGamepadAxis((SDL_Gamepad*)it->controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)/32767.0f;
                it->currentData.leftTrigger     = SDL_GetGamepadAxis((SDL_Gamepad*)it->controller, SDL_GamepadAxis::SDL_GAMEPAD_AXIS_LEFT_TRIGGER)/32767.0f;
            
                if (it->currentData.leftStick != Vector2(0.0f) || it->currentData.rightStick != Vector2(0.0f))
                {
                    //Debug::Log("left x: " + std::to_string(it->currentData.leftStick.x));
                    //Debug::Log("left y: " + std::to_string(it->currentData.leftStick.y));
                    //Debug::Log("right x: " + std::to_string(it->currentData.rightStick.x));
                    //Debug::Log("right y: " + std::to_string(it->currentData.rightStick.y));
                    m_lastInputDeviceType = InputDevice::GAMEPAD;
                    it->lastButtonsPressed = 0u;
                    m_lastControllerID = controllerID;
                }

                controllerID++;
            }
        }
        
        return true;
    }

    void InputManager::SetGameMouseViewport(float _x, float _y, float _drawWidth, float _drawHeight, float _logicalWidth, float _logicalHeight)
    {
        m_gameMouseViewportEnabled = true;
        m_gameMouseViewportX = _x;
        m_gameMouseViewportY = _y;
        m_gameMouseViewportDrawWidth = _drawWidth;
        m_gameMouseViewportDrawHeight = _drawHeight;
        m_gameMouseViewportLogicalWidth = _logicalWidth;
        m_gameMouseViewportLogicalHeight = _logicalHeight;
    }

    void InputManager::ClearGameMouseViewport()
    {
        m_gameMouseViewportEnabled = false;
        m_gameMouseViewportX = 0.0f;
        m_gameMouseViewportY = 0.0f;
        m_gameMouseViewportDrawWidth = 0.0f;
        m_gameMouseViewportDrawHeight = 0.0f;
        m_gameMouseViewportLogicalWidth = 0.0f;
        m_gameMouseViewportLogicalHeight = 0.0f;
    }

    void InputManager::PressKey(unsigned int _keyID)
    {
        for (const auto& c : InputControls()) if (c.key && c.key == _keyID) m_actions.Record(c.path, Vector2(1, 0));
        m_keyVec.push_back(InputData { _keyID , true});
    }

    void InputManager::ReleasedKey(unsigned int _keyID)
    {
        for (const auto& c : InputControls()) if (c.key && c.key == _keyID) m_actions.Record(c.path, Vector2(0));
        m_keyVec.push_back(InputData { _keyID , false});
    }

    void InputManager::SwapMaps()
    {
        m_wasLeftClick = m_leftClick;
        //m_leftClick = false;
        m_wasRightClick = m_rightClick;
        //m_rightClick = false;

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

    bool InputManager::GetKey(unsigned int _keyID)
    {
        const bool *keystate = SDL_GetKeyboardState(NULL);
        const auto synthetic = m_syntheticKeys.find(_keyID);
        return active && (keystate[_keyID] ||
            (synthetic != m_syntheticKeys.end() && synthetic->second));
    }

    bool InputManager::GetButton(unsigned int _gameControllerId, unsigned int _buttonId)
    {
        if (_gameControllerId == 0u && m_syntheticGamepadEnabled &&
            (m_syntheticGamepad.buttons & _buttonId) != 0u)
            return active;

        if (m_gameControllers.size() > _gameControllerId)
        {
            return ((m_gameControllers[_gameControllerId].currentData.buttons & _buttonId) > 0) && active;
        }

        return false;
    }

    bool InputManager::JustPressedButton(unsigned int _gameControllerId, unsigned int _buttonId)
    {
        if (_gameControllerId == 0u && m_syntheticGamepadEnabled &&
            (m_syntheticGamepad.buttons & _buttonId) != 0u &&
            (m_previousSyntheticGamepad.buttons & _buttonId) == 0u)
            return active;

        if (m_gameControllers.size() > _gameControllerId)
        {
            return ((m_gameControllers[_gameControllerId].currentData.buttons & _buttonId) > 0 &&
            (m_gameControllers[_gameControllerId].oldData.buttons & _buttonId) == 0) && active;
        }

        return false;
    }

    bool InputManager::JustReleasedButton(unsigned int _gameControllerId, unsigned int _buttonId)
    {
        if (_gameControllerId == 0u && m_syntheticGamepadEnabled &&
            (m_syntheticGamepad.buttons & _buttonId) == 0u &&
            (m_previousSyntheticGamepad.buttons & _buttonId) != 0u)
            return active;

        if (m_gameControllers.size() > _gameControllerId)
        {
            return ((m_gameControllers[_gameControllerId].currentData.buttons & _buttonId) == 0 &&
            (m_gameControllers[_gameControllerId].oldData.buttons & _buttonId) > 0) && active;
        }

        return false;
    }

    bool InputManager::LastButtonsPressed(unsigned int _gameControllerId, unsigned int _buttonId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return m_gameControllers[_gameControllerId].lastButtonsPressed & _buttonId && active;
        }

        return false;
    }

    Vector2 InputManager::GetLeftStick(unsigned int _gameControllerId)
    {
        if (_gameControllerId == 0u && m_syntheticGamepadEnabled && active)
            return m_syntheticGamepad.leftStick;

        if (m_gameControllers.size() > _gameControllerId && active)
        {
            return m_gameControllers[_gameControllerId].currentData.leftStick;
        }

        return Vector2(0.0f);
    }

    Vector2 InputManager::GetRightStick(unsigned int _gameControllerId)
    {
        if (_gameControllerId == 0u && m_syntheticGamepadEnabled && active)
            return m_syntheticGamepad.rightStick;

        if (m_gameControllers.size() > _gameControllerId && active)
        {
            return m_gameControllers[_gameControllerId].currentData.rightStick;
        }

        return Vector2(0.0f);
    }

    float InputManager::GetLeftTrigger(unsigned int _gameControllerId)
    {
        if (_gameControllerId == 0u && m_syntheticGamepadEnabled && active)
            return m_syntheticGamepad.leftTrigger;

        if (m_gameControllers.size() > _gameControllerId && active)
        {
            return m_gameControllers[_gameControllerId].currentData.leftTrigger;
        }

        return 0.0f;
    }

    float InputManager::GetRightTrigger(unsigned int _gameControllerId)
    {
        if (_gameControllerId == 0u && m_syntheticGamepadEnabled && active)
            return m_syntheticGamepad.rightTrigger;

        if (m_gameControllers.size() > _gameControllerId && active)
        {
            return m_gameControllers[_gameControllerId].currentData.rightTrigger;
        }

        return 0.0f;
    }

    bool InputManager::JustPressedKey(unsigned int _keyID)
    {
        const bool syntheticCurrent = m_syntheticKeys.contains(_keyID) && m_syntheticKeys[_keyID];
        const bool syntheticPrevious =
            m_previousSyntheticKeys.contains(_keyID) && m_previousSyntheticKeys[_keyID];
        if (syntheticCurrent && !syntheticPrevious && active)
            return true;

        bool currentValue = IsKeyDownInVec(&m_keyVec, _keyID);

        bool lastKnownValue = false;

        int index = IsInLastKnown(_keyID);
        if (index != -1)
        {
            lastKnownValue = m_lastKnown[index].value;
        }

        if (currentValue && !lastKnownValue && active)
            return true;
        
        return false;       
    }

    bool InputManager::JustReleasedKey(unsigned int _keyID)
    {
        const bool syntheticCurrent = m_syntheticKeys.contains(_keyID) && m_syntheticKeys[_keyID];
        const bool syntheticPrevious =
            m_previousSyntheticKeys.contains(_keyID) && m_previousSyntheticKeys[_keyID];
        return active && ((!syntheticCurrent && syntheticPrevious) ||
            IsKeyUpInVec(&m_keyVec, _keyID));
    }

    bool InputManager::GetLeftClick()
    {
        return active && (m_leftClick || m_syntheticLeftClick);
    }

    bool InputManager::LeftClickReleased()
    {
        return active &&
            ((!m_leftClick && m_wasLeftClick) ||
             (!m_syntheticLeftClick && m_previousSyntheticLeftClick));
    }

    bool InputManager::JustLeftClicked()
    {
        return active &&
            ((m_leftClick && !m_wasLeftClick) ||
             (m_syntheticLeftClick && !m_previousSyntheticLeftClick));
    }

    bool InputManager::GetRightClick()
    {
        return active && (m_rightClick || m_syntheticRightClick);
    }

    bool InputManager::RightClickReleased()
    {
        return active &&
            ((!m_rightClick && m_wasRightClick) ||
             (!m_syntheticRightClick && m_previousSyntheticRightClick));
    }

    bool InputManager::JustRightClicked()
    {
        return active &&
            ((m_rightClick && !m_wasRightClick) ||
             (m_syntheticRightClick && !m_previousSyntheticRightClick));
    }

    bool InputManager::IsKeyUpInVec(std::vector<InputData> *_arr, unsigned int _value)
    {
        for (int i = 0; i < _arr->size(); i++)
        {
            if ((*_arr)[i].key == _value)
                return !(*_arr)[i].value && active;
        }
        
        return false;
    }

    bool InputManager::IsKeyDownInVec(std::vector<InputData> *_arr, unsigned int _value)
    {
        for (int i = 0; i < _arr->size(); i++)
        {
            if ((*_arr)[i].key == _value)
                return (*_arr)[i].value && active;
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
                return m_lastKnown[i].value && active;
        }
        
        return false;
    }

    void InputManager::OnGameControllerConnected(void *_device)
    {
        SDL_GamepadDeviceEvent& device = (*(SDL_GamepadDeviceEvent*)(_device));

        if (SDL_IsGamepad(device.which))
        {
            GameController gameController = {};
            gameController.controller = SDL_OpenGamepad(device.which);
            if (gameController.controller)
            { 
                SDL_Joystick* j = SDL_GetGamepadJoystick((SDL_Gamepad*)gameController.controller);
                gameController.joyId = SDL_GetJoystickID(j);

                m_lastInputDeviceType = InputDevice::GAMEPAD;
                gameController.lastButtonsPressed = ControllerButton::DPAD_UP;
                m_lastControllerID = m_gameControllers.size();

                const char* name = SDL_GetGamepadName((SDL_Gamepad*)gameController.controller);
                std::string controllerName = name ? name : "Unknown";
                switch (SDL_GetGamepadType((SDL_Gamepad*)gameController.controller)) {
                    case SDL_GAMEPAD_TYPE_XBOX360: case SDL_GAMEPAD_TYPE_XBOXONE:
                        gameController.gameControllerType = XBOX; break;
                    case SDL_GAMEPAD_TYPE_PS3: case SDL_GAMEPAD_TYPE_PS4: case SDL_GAMEPAD_TYPE_PS5:
                        gameController.gameControllerType = PLAYSTATION; break;
                    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
                    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
                    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
                    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
                        gameController.gameControllerType = NINTENDO; break;
#if CANIS_SDL_HAS_STEAM_GAMEPAD_TYPE
                    case SDL_GAMEPAD_TYPE_STEAM:
                        gameController.gameControllerType = STEAM; break;
#endif
                    default: gameController.gameControllerType = UNKNOWN; break;
                }

                const auto vendor = SDL_GetGamepadVendor(static_cast<SDL_Gamepad*>(gameController.controller));
                const auto product = SDL_GetGamepadProduct(static_cast<SDL_Gamepad*>(gameController.controller));
                // These are the 2015 D0G IDs in SDL's controller database, not a broad Valve vendor match.
                gameController.originalSteamController = vendor == 0x28de &&
                    (product == 0x1102 || product == 0x1105 || product == 0x1106 || product == 0x1142);
                if (gameController.originalSteamController) gameController.gameControllerType = STEAM;
                m_gameControllers.push_back(gameController);

                Debug::Log("Game Controller Connected Joy ID: %s Name: %s", std::to_string(gameController.joyId).c_str(), controllerName.c_str());
            }
        }
    }
    
    void InputManager::OnGameControllerDisconnect(void* devicePointer)
    {
        const auto& device = *static_cast<SDL_GamepadDeviceEvent*>(devicePointer);
        m_actions.RemoveSource(uint64_t(device.which) + 2, InputCancellation::Disconnected);
        m_steamOwnedSources.erase(uint64_t(device.which) + 2);
        m_actions.ForgetController(uint64_t(device.which) + 2);
        for (auto it = m_gameControllers.begin(); it != m_gameControllers.end();) {
            if (it->joyId == device.which) {
                SDL_CloseGamepad(static_cast<SDL_Gamepad*>(it->controller));
                it = m_gameControllers.erase(it);
            } else ++it;
        }
        m_lastControllerID = 0;
        for (size_t i = 0; i < m_gameControllers.size(); ++i)
            SDL_SetGamepadPlayerIndex(static_cast<SDL_Gamepad*>(m_gameControllers[i].controller), static_cast<int>(i));
    }

    GameControllerType InputManager::GetControllerType() const
    {
        return m_lastControllerID < m_gameControllers.size() ? m_gameControllers[m_lastControllerID].gameControllerType : UNKNOWN;
    }

    ActionSnapshot InputManager::Action(ActionId id, unsigned int controllerIndex) const
    {
        if (controllerIndex >= m_gameControllers.size()) {
            ActionSnapshot neutral; neutral.type = m_actions.Action(id).type;
            return neutral;
        }
        return m_actions.Action(id, uint64_t(m_gameControllers[controllerIndex].joyId) + 2);
    }

    void InputManager::EvaluateActions(bool gameplayActive)
    {
        std::vector<uint64_t> steamHandles;
        for (const auto& pad : m_gameControllers)
            steamHandles.push_back(SDL_GetGamepadSteamHandle(static_cast<SDL_Gamepad*>(pad.controller)));
        SteamInputPlatform::Poll(m_actions, steamHandles);
        for (const auto& pad : m_gameControllers) {
            const uint64_t source = uint64_t(pad.joyId) + 2;
            const bool owned = SteamInputPlatform::OwnsController(SDL_GetGamepadSteamHandle(static_cast<SDL_Gamepad*>(pad.controller)));
            if (owned != m_steamOwnedSources.contains(source)) {
                m_actions.RemoveSource(source, InputCancellation::BackendChanged);
                if (owned) m_steamOwnedSources.insert(source); else m_steamOwnedSources.erase(source);
            }
            // Discard SDL events collected earlier this frame for Steam-owned devices.
            if (owned) m_actions.DiscardSourceEvents(source);
            const auto type = pad.gameControllerType;
            m_actions.TrackController(source, pad.originalSteamController ? "steam_controller" :
                type == XBOX ? "xbox" : type == PLAYSTATION ? "playstation" : type == NINTENDO ? "switch" : "unknown", owned);
        }
        const bool* keys = SDL_GetKeyboardState(nullptr);
        for (const auto& c : InputControls()) if (c.key)
            m_actions.Record(c.path, Vector2(keys[c.key] ? 1.0f : 0.0f, 0));
        // Reconcile releases outside the game viewport without accepting new
        // button presses outside its existing routing rules.
        const SDL_MouseButtonFlags mouseButtons = SDL_GetMouseState(nullptr, nullptr);
        const char* mousePaths[] = {"Mouse/Left", "Mouse/Middle", "Mouse/Right", "Mouse/X1", "Mouse/X2"};
        for (unsigned int i = 0; i < 5; ++i)
            if (!(mouseButtons & SDL_BUTTON_MASK(i + 1))) m_actions.Record(mousePaths[i], Vector2(0.0f));
        // Read unfiltered SDL axes here; the action processor applies its dead zone once.
        for (const auto& pad : m_gameControllers) {
            auto* device = static_cast<SDL_Gamepad*>(pad.controller);
            const uint64_t source = uint64_t(pad.joyId) + 2;
            if (m_steamOwnedSources.contains(source)) continue;
            for (const auto& c : InputControls()) if (c.button) {
                const int button = std::countr_zero(c.button);
                m_actions.Record(c.path, Vector2(SDL_GetGamepadButton(device, static_cast<SDL_GamepadButton>(button)) ? 1.0f : 0.0f, 0), source);
            }
            auto axis = [&](SDL_GamepadAxis a) { return glm::clamp(SDL_GetGamepadAxis(device, a) / 32767.0f, -1.0f, 1.0f); };
            m_actions.Record("Gamepad/LeftStick", Vector2(axis(SDL_GAMEPAD_AXIS_LEFTX), -axis(SDL_GAMEPAD_AXIS_LEFTY)), source);
            m_actions.Record("Gamepad/RightStick", Vector2(axis(SDL_GAMEPAD_AXIS_RIGHTX), -axis(SDL_GAMEPAD_AXIS_RIGHTY)), source);
            m_actions.Record("Gamepad/LeftTrigger", Vector2(axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER), 0), source);
            m_actions.Record("Gamepad/RightTrigger", Vector2(axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER), 0), source);
        }
        m_actions.Record("Mouse/Delta", mouseRel);
        m_actions.Record("Mouse/Wheel", Vector2(m_scrollVertical + m_syntheticScrollVertical, 0));
        const bool available = active && gameplayActive && !m_windowWasBackgrounded && !m_actionEditorCaptured;
        if (!available && m_actionsWereAvailable && !m_windowWasBackgrounded)
            ClearSyntheticState(InputCancellation::Capture);
        m_actionsWereAvailable = available;
        const auto family = GetControllerType();
        m_actions.SetGlyphFamily(family == XBOX ? "xbox" : family == PLAYSTATION ? "playstation" : family == NINTENDO ? "switch" : "unknown");
        if (m_lastControllerID < m_gameControllers.size() && m_gameControllers[m_lastControllerID].originalSteamController)
            m_actions.SetGlyphFamily("steam_controller");
        // The SDL Steam family spans several models; native Steam origins select their own glyphs.
        m_actions.Evaluate(available);
    }
} // namespace Canis
