#include <Canis/ECS/Systems/UISliderSystem.hpp>

#include <Canis/Scene.hpp>
#include <Canis/Math.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>

namespace Canis
{
    void UISliderSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        auto view = _registry.view<RectTransformComponent, UIImageComponent, UISliderComponent>();
        for (auto [entity, rect, image, slider] : view.each())
        {
            if (slider.value != slider.targetValue)
            {
                if (slider.value < slider.targetValue)
                {
                    slider._time += _deltaTime;
                    Lerp(slider.value, 0, 1, slider._time/slider.timeToMoveFullBar);
                    if (slider.value > slider.targetValue)
                        slider.value = slider.targetValue;
                }
                else
                {
                    slider._time -= _deltaTime;
                    Lerp(slider.value, 0, 1, slider._time/slider.timeToMoveFullBar);
                    if (slider.value < slider.targetValue)
                        slider.value = slider.targetValue;
                }
            }

            slider.value = (slider.value < 0.0f) ? 0.0f : slider.value;
            slider.value = (slider.value > 1.0f) ? 1.0f : slider.value;
            slider.minUVX = (slider.minUVX < 0.0f) ? 0.0f : slider.minUVX;
            slider.minUVX = (slider.minUVX > 1.0f) ? 1.0f : slider.minUVX;
            slider.maxUVX = (slider.maxUVX < 0.0f) ? 0.0f : slider.maxUVX;
            slider.maxUVX = (slider.maxUVX > 1.0f) ? 1.0f : slider.maxUVX;
            image.uv.x = slider.minUVX;
            image.uv.z = (slider.value * slider.maxUVX);
            rect.size.x = (slider.maxWidth * (slider.value * slider.maxUVX)) + (slider.maxWidth * slider.minUVX);
        }
    }

    bool DecodeUISliderSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::UISliderSystem")
        {
            _scene->CreateSystem<UISliderSystem>();
            return true;
        }
        return false;
    }
} // end of Canis namespace