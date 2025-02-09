#include <Canis/ECS/Systems/DamageText3DSystem.hpp>

#include <Canis/Math.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>
#include <Canis/ECS/Components/Transform.hpp>
#include <Canis/ECS/Components/RectTransform.hpp>
#include <Canis/ECS/Components/Color.hpp>
#include <Canis/ECS/Components/Transform.hpp>
#include <Canis/ECS/Components/DamageText3DComponent.hpp>


namespace Canis
{
    void DamageText3DSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        auto view = _registry.view<Transform, RectTransform, Color, DamageText3DComponent>();
        glm::vec2 positionAnchor = glm::vec2(0.0f);
        for (auto [entity, transform, rectTransform, color, damageText] : view.each())
        {
            if (damageText._timeAlive > damageText.lifeTime) {
                _registry.destroy(entity);
                continue;
            }

            damageText._timeAlive += _deltaTime;

            transform.position += damageText.velocity * _deltaTime;

            rectTransform.position = WorldToScreenSpace(*camera,*window, *inputManager, transform.position);

            Lerp(color.color, damageText.startColor, damageText.endColor, damageText._timeAlive / damageText.lifeTime);
        }

        // could check _timeAlive == zero to move text start point away from each other
    }

    bool DecodeDamageText3DSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::DamageText3DSystem")
        {
            _scene->CreateSystem<DamageText3DSystem>();
            return true;
        }
        return false;
    }
} // end of Canis namespace