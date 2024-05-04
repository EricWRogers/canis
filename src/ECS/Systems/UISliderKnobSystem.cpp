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
        auto view = _registry.view<RectTransformComponent, UIImageComponent, UISliderComponent>();
        for (auto [entity, rect, image, slider] : view.each())
        {

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