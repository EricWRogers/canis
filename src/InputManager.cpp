#include <Canis/InputManager.hpp>
#include <Canis/Canis.hpp>
#include <SDL_events.h>
#include <SDL_gamecontroller.h>
#include <Canis/Debug.hpp>
#include <imgui_impl_sdl2.h>
#include <Canis/Window.hpp>

namespace Canis
{
    InputManager::InputManager()
    {
        
    }

    InputManager::~InputManager()
    {
        while(m_gameControllers.size())
        {
            SDL_GameControllerClose((SDL_GameController*)(m_gameControllers.begin()->controller));
            m_gameControllers.erase( m_gameControllers.begin() );
        }
    }

    bool InputManager::Update(int _screenWidth, int _screenHeight, void* _window)
    {
        SwapMaps();
        mouseRel = glm::vec2(0.0f);
        m_scrollVertical = 0;

        Window* window = (Window*)_window;
        window->SetResized(false);

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            #if CANIS_EDITOR
            if (GetProjectConfig().editor)
            {
                ImGui_ImplSDL2_ProcessEvent(&event);
            }
            #endif

            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID((SDL_Window*)window->GetSDLWindow()))
                return false;

            switch (event.type)
            {
            case SDL_QUIT:
                return false;
                break;
            case SDL_WINDOWEVENT:
                if(event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    _screenWidth = event.window.data1;
                    _screenHeight = event.window.data2;
                    window->SetWindowSize(_screenWidth, _screenHeight);
                }
                break;
            case SDL_MOUSEMOTION:
                    mouse.x = event.motion.x;
                    mouse.y = _screenHeight - event.motion.y;
                    mouseRel.x = event.motion.xrel;
                    mouseRel.y = event.motion.yrel;
                    m_lastInputDeviceType = InputDevice::MOUSE;
                break;
            case SDL_MOUSEWHEEL:
                m_scrollVertical = event.wheel.y;
                //Log("Y: " + std::to_string(event.wheel.y));
                //Log("Float Y: " + std::to_string(event.wheel.preciseY));
                //Log("Direction: " + std::to_string(event.wheel.direction));
                break;
            case SDL_KEYUP:
                ReleasedKey(event.key.keysym.sym);
                break;
            case SDL_KEYDOWN:
                PressKey(event.key.keysym.sym);
                m_lastInputDeviceType = InputDevice::KEYBOARD;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                    m_leftClick = true;
                if (event.button.button == SDL_BUTTON_RIGHT)
                    m_rightClick = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT)
                    m_leftClick = false;
                if (event.button.button == SDL_BUTTON_RIGHT)
                    m_rightClick = false;
                break;
            case SDL_CONTROLLERDEVICEADDED:
                m_lastInputDeviceType = InputDevice::GAMEPAD;
                OnGameControllerConnected(&event.cdevice);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                OnGameControllerDisconnect(&event.cdevice);
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                m_lastInputDeviceType = InputDevice::GAMEPAD;
                break;
            }
        }

        std::vector<SDL_GameController*> currentControllers;

        // find active controllers
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                SDL_GameController *controller = SDL_GameControllerOpen(i);
                currentControllers.push_back(controller);
            }
        }

        // remove unactive controllers
        for (int i = 0; i < m_gameControllers.size(); i++)
        {
            bool found = false;

            for (int x = 0; x < currentControllers.size(); x++)
            {
                if (m_gameControllers[i].controller == currentControllers[x])
                {
                    found = true;
                }
            }

            if (!found)
            {
                m_gameControllers.erase( m_gameControllers.begin() + i );
            }
        }

        // reorder controller order
        for (int i = 0; i < m_gameControllers.size(); i++)
        {
            SDL_GameControllerSetPlayerIndex(m_gameControllers[i].controller, i);
        }

        // update controllers
        int controllerID = 0;
        for(auto it = m_gameControllers.begin(); it != m_gameControllers.end(); it++)
        {
            if (it->controller)
            {
                it->oldData                     = it->currentData;
                it->currentData.buttons         = 0u;
                it->currentData.leftStick       = glm::vec2(0.0f);
                it->currentData.rightStick      = glm::vec2(0.0f);
                it->currentData.leftTrigger     = 0.0f;
                it->currentData.rightTrigger    = 0.0f;

                for (unsigned int i = 0; i < 15; i++) // 14 is the last button i care about for now
                {
                    // next line is cool
                    it->currentData.buttons |= ((1 << i) * SDL_GameControllerGetButton((SDL_GameController*)it->controller, (SDL_GameControllerButton)i));
                }

                if (it->currentData.buttons != 0)
                {
                    m_lastInputDeviceType = InputDevice::GAMEPAD;
                    it->lastButtonsPressed = it->currentData.buttons;
                    m_lastControllerID = controllerID;
                }

                it->currentData.leftStick.x     = SDL_GameControllerGetAxis((SDL_GameController*)it->controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTX)/32767.0f;
                it->currentData.leftStick.x     = (abs(it->currentData.leftStick.x) < it->deadZone) ? 0.0f : it->currentData.leftStick.x;

                it->currentData.leftStick.y     = SDL_GameControllerGetAxis((SDL_GameController*)it->controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_LEFTY)/32767.0f;
                it->currentData.leftStick.y     = (abs(it->currentData.leftStick.y) < it->deadZone) ? 0.0f : -(it->currentData.leftStick.y);

                it->currentData.rightStick.x    = SDL_GameControllerGetAxis((SDL_GameController*)it->controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTX)/32767.0f;
                it->currentData.rightStick.x    = (abs(it->currentData.rightStick.x) < it->deadZone) ? 0.0f : it->currentData.rightStick.x;

                it->currentData.rightStick.y    = SDL_GameControllerGetAxis((SDL_GameController*)it->controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_RIGHTY)/32767.0f;
                it->currentData.rightStick.y    = (abs(it->currentData.rightStick.y) < it->deadZone) ? 0.0f : -(it->currentData.rightStick.y);

                it->currentData.rightTrigger    = SDL_GameControllerGetAxis((SDL_GameController*)it->controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERRIGHT)/32767.0f;
                it->currentData.leftTrigger     = SDL_GameControllerGetAxis((SDL_GameController*)it->controller, SDL_GameControllerAxis::SDL_CONTROLLER_AXIS_TRIGGERLEFT)/32767.0f;
            
                if (it->currentData.leftStick != glm::vec2(0.0f) || it->currentData.rightStick != glm::vec2(0.0f))
                {
                    //Log("left x: " + std::to_string(it->currentData.leftStick.x));
                    //Log("left y: " + std::to_string(it->currentData.leftStick.y));
                    //Log("right x: " + std::to_string(it->currentData.rightStick.x));
                    //Log("right y: " + std::to_string(it->currentData.rightStick.y));
                    m_lastInputDeviceType = InputDevice::GAMEPAD;
                    it->lastButtonsPressed = 0u;
                    m_lastControllerID = controllerID;
                }

                controllerID++;
            }
        }
        
        return true;
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
        const Uint8 *keystate = SDL_GetKeyboardState(NULL);
        return keystate[_keyID];
    }

    bool InputManager::GetButton(unsigned int _gameControllerId, unsigned int _buttonId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return ((m_gameControllers[_gameControllerId].currentData.buttons & _buttonId) > 0);
        }

        return false;
    }

    bool InputManager::JustPressedButton(unsigned int _gameControllerId, unsigned int _buttonId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return ((m_gameControllers[_gameControllerId].currentData.buttons & _buttonId) > 0 &&
            (m_gameControllers[_gameControllerId].oldData.buttons & _buttonId) == 0);
        }

        return false;
    }

    bool InputManager::JustReleasedButton(unsigned int _gameControllerId, unsigned int _buttonId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return ((m_gameControllers[_gameControllerId].currentData.buttons & _buttonId) == 0 &&
            (m_gameControllers[_gameControllerId].oldData.buttons & _buttonId) > 0);
        }

        return false;
    }

    bool InputManager::LastButtonsPressed(unsigned int _gameControllerId, unsigned int _buttonId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return m_gameControllers[_gameControllerId].lastButtonsPressed & _buttonId;
        }

        return false;
    }

    glm::vec2 InputManager::GetLeftStick(unsigned int _gameControllerId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return m_gameControllers[_gameControllerId].currentData.leftStick;
        }

        return glm::vec2(0.0f);
    }

    glm::vec2 InputManager::GetRightStick(unsigned int _gameControllerId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return m_gameControllers[_gameControllerId].currentData.rightStick;
        }

        return glm::vec2(0.0f);
    }

    float InputManager::GetLeftTrigger(unsigned int _gameControllerId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return m_gameControllers[_gameControllerId].currentData.leftTrigger;
        }

        return 0.0f;
    }

    float InputManager::GetRightTrigger(unsigned int _gameControllerId)
    {
        if (m_gameControllers.size() > _gameControllerId)
        {
            return m_gameControllers[_gameControllerId].currentData.rightTrigger;
        }

        return 0.0f;
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

    void InputManager::OnGameControllerConnected(void *_device)
    {
        SDL_ControllerDeviceEvent& device = (*(SDL_ControllerDeviceEvent*)(_device));

        if (SDL_IsGameController(device.which))
        {
            GameController gameController = {};
            gameController.controller = SDL_GameControllerOpen(device.which);
            if (gameController.controller)
            {
                SDL_Joystick* j = SDL_GameControllerGetJoystick((SDL_GameController*)gameController.controller);
                gameController.joyId = SDL_JoystickInstanceID(j);
                m_gameControllers.push_back(gameController);

                Log("Game Controller Connected Joy ID: " + std::to_string(gameController.joyId));
            }
        }
    }
    
    void InputManager::OnGameControllerDisconnect(void *_device)
    {
        SDL_ControllerDeviceEvent& device = (*(SDL_ControllerDeviceEvent*)(_device));
        SDL_GameController * controller;
        SDL_JoystickID joyID = 0;

        std::vector<SDL_GameController*> currentControllers;

        // find active controllers
        for (int i = 0; i < SDL_NumJoysticks(); i++) {
            if (SDL_IsGameController(i)) {
                controller = SDL_GameControllerOpen(i);
                SDL_Joystick * js = SDL_GameControllerGetJoystick(controller);
                if (device.which == SDL_JoystickInstanceID(js)) {
                    SDL_GameControllerClose(controller);
                    joyID = device.which;
                }
                else
                {
                    currentControllers.push_back(controller);
                }
            }
        }

        // remove unactive controllers
        for (int i = 0; i < m_gameControllers.size(); i++)
        {
            bool found = false;

            for (int x = 0; x < currentControllers.size(); x++)
            {
                if (m_gameControllers[i].controller == currentControllers[x])
                {
                    found = true;
                }
            }

            if (!found)
            {
                m_gameControllers.erase( m_gameControllers.begin() + i );

                Log("Game Controller Disconnected");
            }
        }

        // reorder controller order
        for (int i = 0; i < m_gameControllers.size(); i++)
        {
            SDL_GameControllerSetPlayerIndex(m_gameControllers[i].controller, i);
        }
    }


} // end of Canis namespace
