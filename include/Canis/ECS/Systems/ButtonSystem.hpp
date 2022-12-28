#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/InputManager.hpp>
#include <Canis/External/entt.hpp>
#include <Canis/ECS/Systems/System.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>

namespace Canis
{
    class ButtonSystem : public System
    {
    private:
    public:
        ButtonSystem(std::string _name) : System(_name) {}

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime)
        {
            auto view = _registry.view<RectTransformComponent, ColorComponent, ButtonComponent>();
            glm::vec2 positionAnchor = glm::vec2(0.0f);
            for (auto [entity, rect_transform, color, button] : view.each())
            {
                if (rect_transform.active)
                {
                    positionAnchor = GetAnchor((Canis::RectAnchor)rect_transform.anchor,
                        (float)window->GetScreenWidth(),
                        (float)window->GetScreenHeight());
                    
                    if (inputManager->mouse.x > rect_transform.position.x + positionAnchor.x &&
                        inputManager->mouse.x < rect_transform.position.x + rect_transform.size.x + positionAnchor.x &&
                        inputManager->mouse.y > rect_transform.position.y + positionAnchor.y &&
                        inputManager->mouse.y < rect_transform.position.y + rect_transform.size.y + positionAnchor.y)
                    {
                        color.color = button.hoverColor;
                        if (inputManager->leftClick)
                            button.func(button.instance);
                    }
                    else
                    {
                        color.color = button.baseColor;
                    }
                }
            }
        }
    };
} // end of Canis namespace