#include "Canis/Entity.hpp"
#include "Canis/InputManager.hpp"
#include <Canis/ECS/Systems/ButtonSystem.hpp>

#include <Canis/Scene.hpp>

#include <Canis/DataStructure/List.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/ECS/Components/Color.hpp>
#include <Canis/ECS/Components/RectTransform.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/Math.hpp>

namespace Canis
{
    bool ButtonAndDepthAscending(void *a, void *b)
    {
        return static_cast<ButtonAndDepth *>(a)->depth >
               static_cast<ButtonAndDepth *>(b)->depth;
    }

    ButtonSystem::~ButtonSystem()
    {
        if (m_buttons != nullptr)
            List::Free(&m_buttons);
    }

    void ButtonSystem::Create()
    {
        if (m_buttons == nullptr)
            List::Init(&m_buttons, 10, sizeof(ButtonAndDepth));
    }

    void ButtonSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        auto view =
            _registry.view<RectTransform, Color, ButtonComponent>();
        glm::vec2 positionAnchor = glm::vec2(0.0f);
        glm::vec2 mouse = glm::vec2(0.0f);
        bool mouseLook = window->GetMouseLock();
        Entity defaultButton = Entity(scene);
        Entity targetButton = Entity(scene);

        List::Clear(&m_buttons);

        for (auto [entity, rect_transform, color, button] : view.each())
        {
            if (rect_transform.active)
            {
                ButtonAndDepth bad;
                bad.entity = Entity(entity, scene);
                bad.depth = rect_transform.depth;

                if (button.mouseOver &&
                    (button.up || button.down || button.left || button.right))
                    targetButton.entityHandle = entity;

                if (button.defaultSelected &&
                    (button.up || button.down || button.left || button.right))
                    defaultButton.entityHandle = entity;


                if (inputManager->GetLastDeviceType() != InputDevice::MOUSE)
                {
                    color.color = button.baseColor;
                    button.mouseOver = false;
                    rect_transform.scale = button.scale;
                }

                List::Add(&m_buttons, &bad);
            }
        }

        if (inputManager->GetLastDeviceType() == InputDevice::GAMEPAD &&
            (inputManager->LastButtonsPressed(ControllerButton::DPAD_UP) ||
            inputManager->LastButtonsPressed(ControllerButton::DPAD_DOWN) ||
            inputManager->LastButtonsPressed(ControllerButton::DPAD_LEFT) ||
            inputManager->LastButtonsPressed(ControllerButton::DPAD_RIGHT) ||
            inputManager->LastButtonsPressed(ControllerButton::A)))
        {
            if (!targetButton && defaultButton)
            {
                targetButton = defaultButton;
            }

            if (!targetButton)
                return;

            Entity lastTargetButton = Entity(scene);
            if (inputManager->JustPressedButton(ControllerButton::DPAD_UP) &&
                targetButton.GetComponent<ButtonComponent>().up)
            {
                lastTargetButton = targetButton;
                targetButton = targetButton.GetComponent<ButtonComponent>().up;
            }
            else if (inputManager->JustPressedButton(ControllerButton::DPAD_DOWN) &&
                     targetButton.GetComponent<ButtonComponent>().down)
            {
                lastTargetButton = targetButton;
                targetButton = targetButton.GetComponent<ButtonComponent>().down;
            }
            else if (inputManager->JustPressedButton(ControllerButton::DPAD_LEFT) &&
                     targetButton.GetComponent<ButtonComponent>().left)
            {
                lastTargetButton = targetButton;
                targetButton = targetButton.GetComponent<ButtonComponent>().left;
            }
            else if (inputManager->JustPressedButton(ControllerButton::DPAD_RIGHT) &&
                     targetButton.GetComponent<ButtonComponent>().right)
            {
                lastTargetButton = targetButton;
                targetButton = targetButton.GetComponent<ButtonComponent>().right;
            }

            if (lastTargetButton)
            {
                lastTargetButton.GetComponent<Color>().color = lastTargetButton.GetComponent<ButtonComponent>().baseColor;
                lastTargetButton.GetComponent<ButtonComponent>().mouseOver = false;
                lastTargetButton.GetComponent<RectTransform>().scale = lastTargetButton.GetComponent<ButtonComponent>().scale;
            }

            if (targetButton)
            {
                targetButton.GetComponent<Color>().color = targetButton.GetComponent<ButtonComponent>().hoverColor;
                targetButton.GetComponent<ButtonComponent>().mouseOver = true;
                targetButton.GetComponent<RectTransform>().scale = targetButton.GetComponent<ButtonComponent>().hoverScale;
            }
        }
        else
        {
            List::BubbleSort(&m_buttons, ButtonAndDepthAscending);

            targetButton.entityHandle = entt::null;

            for (int i = 0; i < List::GetCount(&m_buttons); i++)
            {
                RectTransform &rect_transform = m_buttons[i].entity.GetComponent<RectTransform>();
                Color &color = m_buttons[i].entity.GetComponent<Color>();
                ButtonComponent &button = m_buttons[i].entity.GetComponent<ButtonComponent>();

                positionAnchor = GetAnchor((Canis::RectAnchor)rect_transform.anchor,
                                           (float)window->GetScreenWidth(),
                                           (float)window->GetScreenHeight());

                mouse = GetInputManager().mouse;

                if (rect_transform.rotation != 0.0f)
                {
                    RotatePointAroundPivot(
                        mouse,
                        rect_transform.position + rect_transform.originOffset +
                            positionAnchor + rect_transform.rotationOriginOffset,
                        -rect_transform.rotation);
                }

                if (mouse.x > rect_transform.position.x + positionAnchor.x +
                                  rect_transform.originOffset.x &&
                    mouse.x < rect_transform.position.x +
                                  (rect_transform.size.x * rect_transform.scale) +
                                  rect_transform.originOffset.x + positionAnchor.x &&
                    mouse.y > rect_transform.position.y + positionAnchor.y +
                                  rect_transform.originOffset.y &&
                    mouse.y < rect_transform.position.y +
                                  (rect_transform.size.y * rect_transform.scale) +
                                  rect_transform.originOffset.y + positionAnchor.y &&
                    !targetButton && !mouseLook)
                {
                    color.color = button.hoverColor;
                    button.mouseOver = true;
                    rect_transform.scale = button.hoverScale;
                    targetButton = m_buttons[i].entity;
                }
                else
                {
                    color.color = button.baseColor;
                    button.mouseOver = false;
                    rect_transform.scale = button.scale;
                }
            }
        }

        if (targetButton)
        {
            ButtonComponent &button = targetButton.GetComponent<ButtonComponent>();
            bool clicked = false;

            if (inputManager->GetLastDeviceType() == InputDevice::GAMEPAD)
            {
                if (button.action == 0u && inputManager->JustPressedButton(ControllerButton::A))
                {
                    clicked = true;
                }
                else if (button.action == 1u && inputManager->JustReleasedButton(ControllerButton::A))
                {
                    clicked = true;
                }
            }
            else
            {
                if (button.action == 0u)
                {
                    if (inputManager->JustLeftClicked())
                    {
                        clicked = true;
                    }
                }
                else if (button.action == 1u)
                {
                    if (inputManager->LeftClickReleased())
                    {
                        clicked = true;
                    }
                }
            }
        
            if (clicked)
            {
                for (int i = 0; i < m_buttonListeners.size(); i++)
                {
                    ButtonListener& bl = *m_buttonListeners[i];

                    if (m_buttonListeners[i]->name == button.eventName)
                    {
                        m_buttonListeners[i]->func(targetButton, m_buttonListeners[i]->data);
                    }
                }
            }
        }
    }

    ButtonListener& ButtonSystem::AddButtonListener(std::string _name, void *_data,
                                      std::function<void(Entity _entity, void* _data)> _func)
    {
        ButtonListener* buttonListener = new ButtonListener();
        buttonListener->_system = this;
        buttonListener->_id = nextId;
        nextId++;

        m_buttonListeners.push_back(buttonListener);

        ButtonListener &bl = *m_buttonListeners[ m_buttonListeners.size() - 1 ];

        bl.name = _name;
        bl.data = _data;
        bl.func = _func;

        return bl;
    }

    void ButtonSystem::RemoveButtonListener(int _id)
    {
        for (int i = 0; i < m_buttonListeners.size(); i++)
        {
            if (_id == m_buttonListeners[i]->_id)
            {
                m_buttonListeners.erase(m_buttonListeners.begin() + i);
            }
        }
    }

    ButtonListener::~ButtonListener()
    {
        if (_system != nullptr)
            _system->RemoveButtonListener(_id);
    }
} // namespace Canis