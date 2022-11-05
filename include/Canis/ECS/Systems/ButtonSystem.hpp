#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/InputManager.hpp>
#include <Canis/External/entt.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>

namespace Canis
{
    class ButtonSystem
    {
    private:
        InputManager *inputManager;

    public:
        ButtonSystem(InputManager *im) { inputManager = im; }

        void UpdateComponents(float deltaTime, entt::registry &registry)
        {
            auto view = registry.view<RectTransformComponent, ColorComponent, ButtonComponent>();
            for (auto [entity, rect_transform, color, button] : view.each())
			{
                if (inputManager->mouse.x > rect_transform.position.x &&
                inputManager->mouse.x < rect_transform.position.x + rect_transform.size.x &&
                inputManager->mouse.y > rect_transform.position.y && inputManager->mouse.y < rect_transform.position.y + rect_transform.size.y) {
                    color.color = button.hoverColor;
                    if(inputManager->leftClick)
                        button.func(button.instance);
                }
                else {
                    color.color = button.baseColor;
                }
            }
        }
    };
} // end of Canis namespace