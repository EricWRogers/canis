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
    }

    void ButtonSystem::Create()
    {
        if (m_buttons == nullptr)
            List::Init(&m_buttons, 10, sizeof(ButtonAndDepth));
    }

    void ButtonSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        auto view = _registry.view<RectTransformComponent, ColorComponent, ButtonComponent>();
        glm::vec2 positionAnchor = glm::vec2(0.0f);
        glm::vec2 mouse = glm::vec2(0.0f);
        bool buttonFound = false;

        List::Clear(&m_buttons);

        for (auto [entity, rect_transform, color, button] : view.each())
        {
            if (rect_transform.active)
            {
                ButtonAndDepth bad;
                bad.entity = entity;
                bad.depth = rect_transform.depth;

                List::Add(&m_buttons, &bad);
            }
        }

        List::BubbleSort(&m_buttons, ButtonAndDepthAscending);

        for (int i = 0; i < List::GetCount(&m_buttons); i++)
        {
            RectTransformComponent& rect_transform = _registry.get<RectTransformComponent>(m_buttons[i].entity);
            ColorComponent& color = _registry.get<ColorComponent>(m_buttons[i].entity);
            ButtonComponent& button = _registry.get<ButtonComponent>(m_buttons[i].entity);

            positionAnchor = GetAnchor((Canis::RectAnchor)rect_transform.anchor,
                                        (float)window->GetScreenWidth(),
                                        (float)window->GetScreenHeight());

            mouse = GetInputManager().mouse;

            if (rect_transform.rotation != 0.0f)
            {
                float cAngle = cos(-rect_transform.rotation);
                float sAngle = sin(-rect_transform.rotation);

                glm::vec2 mouseOffset = mouse - (rect_transform.position + rect_transform.originOffset + positionAnchor);// + rect_transform.rotationOriginOffset);

                RotatePoint(mouseOffset, cAngle, sAngle);

                mouse = mouseOffset + (rect_transform.position + rect_transform.originOffset + positionAnchor);// + rect_transform.rotationOriginOffset);
            }

            if (mouse.x > rect_transform.position.x + positionAnchor.x + rect_transform.originOffset.x &&
                mouse.x < rect_transform.position.x + (rect_transform.size.x * rect_transform.scale) + rect_transform.originOffset.x + positionAnchor.x &&
                mouse.y > rect_transform.position.y + positionAnchor.y + rect_transform.originOffset.y &&
                mouse.y < rect_transform.position.y + (rect_transform.size.y * rect_transform.scale) + rect_transform.originOffset.y + positionAnchor.y &&
                !buttonFound)
            {
                color.color = button.hoverColor;
                button.mouseOver = true;
                rect_transform.scale = button.hoverScale;

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
                buttonFound = true;                    
            } 
            else
            {
                color.color = button.baseColor;
                button.mouseOver = false;
                rect_transform.scale = button.scale;
            }           
        }
    }

    bool DecodeButtonSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::ButtonSystem")
        {
            _scene->CreateSystem<ButtonSystem>();
            return true;
        }
        return false;
    }
} // end of Canis namespace