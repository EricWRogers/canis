#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/External/entt.hpp>
#include <Canis/ECS/Systems/System.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>

namespace Canis
{
    class UISliderSystem : public System
    {
    private:
    public:
        UISliderSystem() : System() {}

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime)
        {
            auto view = _registry.view<RectTransformComponent, UIImageComponent, UISliderComponent>();
            for (auto [entity, rect, image, slider] : view.each())
            {
                slider.value = (slider.value < 0.0f) ? 0.0f : slider.value;
                slider.value = (slider.value > 1.0f) ? 1.0f : slider.value;
                slider.minUVX = (slider.minUVX < 0.0f) ? 0.0f : slider.minUVX;
                slider.minUVX = (slider.minUVX > 1.0f) ? 1.0f : slider.minUVX;
                slider.maxUVX = (slider.maxUVX < 0.0f) ? 0.0f : slider.maxUVX;
                slider.maxUVX = (slider.maxUVX > 1.0f) ? 1.0f : slider.maxUVX;
                image.uv.x = slider.minUVX;
                image.uv.z = (slider.value * slider.maxUVX) + slider.minUVX;
                rect.size.x = slider.maxWidth * slider.value;
            }
        }
    };
} // end of Canis namespace 