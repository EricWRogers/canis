#include <Canis/ECS/Systems/ButtonSystem.hpp>

#include <Canis/Scene.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/Math.hpp>

namespace Canis
{
    void ButtonSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        auto view = _registry.view<RectTransformComponent, ColorComponent, ButtonComponent>();
        glm::vec2 positionAnchor = glm::vec2(0.0f);
        glm::vec2 mouse = glm::vec2(0.0f);

        for (auto [entity, rect_transform, color, button] : view.each())
        {
            if (rect_transform.active)
            {
                positionAnchor = GetAnchor((Canis::RectAnchor)rect_transform.anchor,
                                           (float)window->GetScreenWidth(),
                                           (float)window->GetScreenHeight());

                mouse = GetInputManager().mouse;

                if (rect_transform.rotation != 0.0f)
                {
                    float cAngle = cos(-rect_transform.rotation);
                    float sAngle = sin(-rect_transform.rotation);

                    glm::vec2 mouseOffset = mouse - (rect_transform.position + rect_transform.originOffset + positionAnchor);

                    RotatePoint(mouseOffset, cAngle, sAngle);

                    mouse = mouseOffset + (rect_transform.position + rect_transform.originOffset + positionAnchor);
                }

                if (mouse.x > rect_transform.position.x + positionAnchor.x + rect_transform.originOffset.x &&
                    mouse.x < rect_transform.position.x + (rect_transform.size.x * rect_transform.scale) + rect_transform.originOffset.x + positionAnchor.x &&
                    mouse.y > rect_transform.position.y + positionAnchor.y + rect_transform.originOffset.y &&
                    mouse.y < rect_transform.position.y + (rect_transform.size.y * rect_transform.scale) + rect_transform.originOffset.y + positionAnchor.y)
                {
                    color.color = button.hoverColor;
                    button.mouseOver = true;
                    rect_transform.scale = button.hoverScale;

                    if (button.action == 0u)
                    {
                        if (inputManager->JustLeftClicked())
                        {
                            button.func(button.instance);
                            return;
                        }
                    }
                    else if (button.action == 1u)
                    {
                        if (inputManager->LeftClickReleased())
                        {
                            button.func(button.instance);
                            return;
                        }
                    }                    
                }
                else
                {
                    color.color = button.baseColor;
                    button.mouseOver = false;
                    rect_transform.scale = button.scale;
                }
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