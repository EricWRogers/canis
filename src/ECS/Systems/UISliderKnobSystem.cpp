#include <Canis/ECS/Systems/UISliderKnobSystem.hpp>

#include <Canis/Scene.hpp>
#include <Canis/Math.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>
#include <Canis/ECS/Components/UISliderKnobComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>

namespace Canis
{
    void UISliderKnobSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        bool mouseDown = GetInputManager().GetLeftClick();
        
        auto view = _registry.view<RectTransformComponent, ButtonComponent, UISliderKnobComponent>();
        for (auto [entity, rect, button, knob] : view.each())
        {
            if (!knob.slider)
                return;

            UISliderComponent& slider = knob.slider.GetComponent<UISliderComponent>();
            RectTransformComponent& sliderRect = knob.slider.GetComponent<RectTransformComponent>();

            vec2 halfSize {
                (rect.size.x * rect.scale) / 2.0f,
                (rect.size.y * rect.scale) / 2.0f
            };
            
            if (slider.targetValue != knob.value)
                SetUISliderTarget(slider, knob.value);
            
            rect.anchor = sliderRect.anchor;
            rect.position.x = sliderRect.position.x + (knob.value * slider.maxWidth) - halfSize.x;
            rect.position.y = (sliderRect.position.y + (sliderRect.size.y * sliderRect.scale) / 2.0f) - halfSize.y;
            
            //if (!knob.grabbed)
            //    return;

            if (mouseDown == false)
                return;

            if (!button.mouseOver)
                return;

            rect.position.x = GetInputManager().mouse.x - (rect.size.x * rect.scale) / 2.0f;
            rect.position.x -= GetAnchor(
                (RectAnchor)rect.anchor,
                GetWindow().GetScreenWidth(),
                GetWindow().GetScreenHeight()).x;
            
            Clamp(rect.position.x, sliderRect.position.x - halfSize.x, (sliderRect.position.x + slider.maxWidth) - halfSize.x);
            knob.value = (rect.position.x - sliderRect.position.x + halfSize.x) / slider.maxWidth;
            
            if (knob.OnChanged)
            {
                knob.OnChanged(knob.value);
            }
        }
    }

    bool DecodeUISliderKnobSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::UISliderKnobSystem")
        {
            _scene->CreateSystem<UISliderKnobSystem>();
            return true;
        }
        return false;
    }
} // end of Canis namespace