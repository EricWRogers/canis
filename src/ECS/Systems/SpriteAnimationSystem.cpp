#include <Canis/ECS/Systems/SpriteAnimationSystem.hpp>

#include <glm/glm.hpp>
#include <vector>

#include <Canis/Scene.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/SpriteAnimationComponent.hpp>

namespace Canis
{
    void SpriteAnimationSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        auto view = _registry.view<SpriteAnimationComponent>();
        SpriteAnimationAsset *spriteAnimationAsset = nullptr;
        int spriteAnimationId = 0;
        for (auto [entity, animation] : view.each())
        {
            if (entity == entt::tombstone && !_registry.valid(entity))
                continue;

            animation.countDown -= _deltaTime * animation.speed;

            if (animation.countDown < 0.0f)
            {
                Sprite2DComponent &sprite = _registry.get<Sprite2DComponent>(entity);
                animation.index++;
                animation.redraw = false;

                if (animation.animationId != spriteAnimationId || spriteAnimationAsset == nullptr)
                {
                    spriteAnimationId = animation.animationId;
                    spriteAnimationAsset = assetManager->Get<SpriteAnimationAsset>(spriteAnimationId);
                }

                if (animation.index >= spriteAnimationAsset->frames.size())
                    animation.index = 0;

                animation.countDown = spriteAnimationAsset->frames[animation.index].timeOnFrame;
                sprite.texture = assetManager->Get<Canis::TextureAsset>(spriteAnimationAsset->frames[animation.index].textureId)->GetTexture();

                GetSpriteFromTextureAtlas(
                    sprite,
                    spriteAnimationAsset->frames[animation.index].offsetX,
                    spriteAnimationAsset->frames[animation.index].offsetY,
                    spriteAnimationAsset->frames[animation.index].row,
                    spriteAnimationAsset->frames[animation.index].col,
                    spriteAnimationAsset->frames[animation.index].width,
                    spriteAnimationAsset->frames[animation.index].height,
                    animation.flipX,
                    animation.flipY);
            }

            if (animation.redraw)
            {
                Sprite2DComponent &sprite = _registry.get<Sprite2DComponent>(entity);
                animation.redraw = false;

                if (animation.animationId != spriteAnimationId || spriteAnimationAsset == nullptr)
                {
                    spriteAnimationId = animation.animationId;
                    spriteAnimationAsset = assetManager->Get<SpriteAnimationAsset>(spriteAnimationId);
                }

                sprite.texture = assetManager->Get<Canis::TextureAsset>(spriteAnimationAsset->frames[animation.index].textureId)->GetTexture();

                GetSpriteFromTextureAtlas(
                    sprite,
                    spriteAnimationAsset->frames[animation.index].offsetX,
                    spriteAnimationAsset->frames[animation.index].offsetY,
                    spriteAnimationAsset->frames[animation.index].row,
                    spriteAnimationAsset->frames[animation.index].col,
                    spriteAnimationAsset->frames[animation.index].width,
                    spriteAnimationAsset->frames[animation.index].height,
                    animation.flipX,
                    animation.flipY);
            }
        }
    }

    bool DecodeSpriteAnimationSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::SpriteAnimationSystem")
        {
            _scene->CreateSystem<SpriteAnimationSystem>();
            return true;
        }
        return false;
    }
}