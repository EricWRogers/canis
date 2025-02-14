#include <Canis/ECS/Systems/UISliderKnobSystem.hpp>

#include <Canis/Entity.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Math.hpp>

#include <Canis/ECS/Components/RectTransform.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>
#include <Canis/ECS/Components/UISliderKnobComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>

namespace Canis
{
    void UISliderKnobSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        bool mouseDown = GetInputManager().GetLeftClick();

        auto view = _registry.view<RectTransform, ButtonComponent, UISliderKnobComponent>();
        for (auto [entity, rect, button, knob] : view.each())
        {
            if (!knob.slider)
                continue;

            if (mouseDown == false)
                knob.grabbed = false;

            UISliderComponent &slider = knob.slider.GetComponent<UISliderComponent>();
            RectTransform &sliderRect = knob.slider.GetComponent<RectTransform>();

            vec2 halfSize{
                (rect.size.x * rect.scale) / 2.0f,
                (rect.size.y * rect.scale) / 2.0f};

            SetUISliderTarget(slider, knob.value, 0.0f);

            rect.anchor = sliderRect.anchor;
            rect.position.x = sliderRect.position.x + (knob.value * slider.maxWidth) - halfSize.x;
            rect.position.y = (sliderRect.position.y + (sliderRect.size.y * sliderRect.scale) / 2.0f) - halfSize.y;

            if (inputManager->GetLastDeviceType() == InputDevice::MOUSE)
            {
                if (mouseDown == false)
                    continue;
            }
            else
            {
                if (mouseDown == false && button.mouseOver == false)
                    continue;
            }

            if (!button.mouseOver && !knob.grabbed)
                continue;

            if (inputManager->GetLastDeviceType() == InputDevice::MOUSE)
            {
                
                rect.position.x = GetInputManager().mouse.x - (rect.size.x * rect.scale) / 2.0f;
                rect.position.x -= GetAnchor(
                                    (RectAnchor)rect.anchor,
                                    GetWindow().GetScreenWidth(),
                                    GetWindow().GetScreenHeight())
                                    .x;
            }
            else if (inputManager->GetLastDeviceType() == InputDevice::GAMEPAD)
            {
                float speed = 100.0f;
                float direction = 0;

                if (inputManager->GetButton(ControllerButton::DPAD_LEFT))
                {
                    direction = -1.0f;
                }
                else if (inputManager->GetButton(ControllerButton::DPAD_RIGHT))
                {
                    direction = 1.0f;
                }

                rect.position.x += direction * speed * _deltaTime;
            }

            knob.grabbed = true;

            Clamp(rect.position.x, sliderRect.position.x - halfSize.x, (sliderRect.position.x + slider.maxWidth) - halfSize.x);
            float newValue = (rect.position.x - sliderRect.position.x + halfSize.x) / slider.maxWidth;

            if (knob.value != newValue)
            {
                knob.value = newValue;

                if (knob.eventName.empty() == false)
                {
                    for (int i = 0; i < m_knobListeners.size(); i++)
                    {
                        KnobListener &kl = *m_knobListeners[i];

                        Canis::Entity e(scene);
                        e.entityHandle = entity; 

                        if (m_knobListeners[i]->name == knob.eventName)
                        {
                            m_knobListeners[i]->func(e, knob.value, m_knobListeners[i]->data);
                        }
                    }
                }
            }
        }
    }

    KnobListener &UISliderKnobSystem::AddKnobListener(std::string _name, void *_data,
                                                      std::function<void(Entity _entity, float _value, void *_data)> _func)
    {
        KnobListener* knobListener = new KnobListener();
        knobListener->_system = this;
        knobListener->_id = nextId;
        nextId++;

        m_knobListeners.push_back(knobListener);

        KnobListener &kl = *m_knobListeners[m_knobListeners.size() - 1];

        kl.name = _name;
        kl.data = _data;
        kl.func = _func;

        return kl;
    }

    void UISliderKnobSystem::RemoveKnobListener(int _id)
    {
        for (int i = 0; i < m_knobListeners.size(); i++)
        {
            if (_id == m_knobListeners[i]->_id)
            {
                m_knobListeners.erase(m_knobListeners.begin() + i);
            }
        }
    }

    KnobListener::~KnobListener()
    {
        if (_system != nullptr)
            _system->RemoveKnobListener(_id);
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