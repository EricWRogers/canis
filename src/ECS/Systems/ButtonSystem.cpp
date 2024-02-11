#include <Canis/ECS/Systems/ButtonSystem.hpp>

#include <Canis/Scene.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/Math.hpp>
#include <Canis/DataStructure/List.hpp>

namespace Canis
{
    bool ButtonAndDepthAscending(void* a, void *b) {
        return static_cast<ButtonAndDepth*>(a)->depth > static_cast<ButtonAndDepth*>(b)->depth;
    }

    ButtonSystem::~ButtonSystem()
    {
        if (m_buttons != nullptr)
            List::Free(&m_buttons);
        
        if (m_buttonListeners != nullptr)
            List::Free(&m_buttonListeners);
    }

    void ButtonSystem::Create()
    {
        if (m_buttons == nullptr)
            List::Init(&m_buttons, 10, sizeof(ButtonAndDepth));

        if (m_buttonListeners == nullptr)
            List::Init(&m_buttonListeners, 10, sizeof(ButtonListener));
    }

    void ButtonSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        auto view = _registry.view<RectTransformComponent, ColorComponent, ButtonComponent>();
        glm::vec2 positionAnchor = glm::vec2(0.0f);
        glm::vec2 mouse = glm::vec2(0.0f);
        Entity targetButton;

        List::Clear(&m_buttons);

        for (auto [entity, rect_transform, color, button] : view.each())
        {
            if (rect_transform.active)
            {
                ButtonAndDepth bad;
                bad.entity = Entity(entity, scene);
                bad.depth = rect_transform.depth;

                List::Add(&m_buttons, &bad);
            }
        }

        List::BubbleSort(&m_buttons, ButtonAndDepthAscending);

        for (int i = 0; i < List::GetCount(&m_buttons); i++)
        {
            RectTransformComponent& rect_transform = m_buttons[i].entity.GetComponent<RectTransformComponent>();
            ColorComponent& color = m_buttons[i].entity.GetComponent<ColorComponent>();
            ButtonComponent& button = m_buttons[i].entity.GetComponent<ButtonComponent>();

            positionAnchor = GetAnchor((Canis::RectAnchor)rect_transform.anchor,
                                        (float)window->GetScreenWidth(),
                                        (float)window->GetScreenHeight());

            mouse = GetInputManager().mouse;

            if (rect_transform.rotation != 0.0f)
            {                
                RotatePointAroundPivot(mouse,
                                        rect_transform.position + rect_transform.originOffset + positionAnchor + rect_transform.rotationOriginOffset,
                                        -rect_transform.rotation);
            }

            if (mouse.x > rect_transform.position.x + positionAnchor.x + rect_transform.originOffset.x &&
                mouse.x < rect_transform.position.x + (rect_transform.size.x * rect_transform.scale) + rect_transform.originOffset.x + positionAnchor.x &&
                mouse.y > rect_transform.position.y + positionAnchor.y + rect_transform.originOffset.y &&
                mouse.y < rect_transform.position.y + (rect_transform.size.y * rect_transform.scale) + rect_transform.originOffset.y + positionAnchor.y &&
                !targetButton)
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

        if (targetButton)
        {
            ButtonComponent& button = targetButton.GetComponent<ButtonComponent>();
            if (button.action == 0u)
            {
                if (inputManager->JustLeftClicked())
                {
                    button.func(button.instance);
                }
            }
            else if (button.action == 1u)
            {
                if (inputManager->LeftClickReleased())
                {
                    button.func(button.instance);
                }
            }
        }
    }

    void Update(entt::registry &_registry, float _deltaTime)
    {
        
    }

    ButtonListener* AddButtonListener(
        std::string _name,
        void* _data,
        std::function<void(void* _data)> func)
    {
        return nullptr;
    }
} // end of Canis namespace