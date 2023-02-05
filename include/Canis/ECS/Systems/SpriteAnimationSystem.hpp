#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/External/entt.hpp>
#include <Canis/ECS/Systems/System.hpp>

#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/SpriteAnimationComponent.hpp>

namespace Canis
{
    class SpriteAnimationSystem : public System
    {
    private:
    public:
        SpriteAnimationSystem() : System() {}

        void Create() {}

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime)
        {
            auto view = _registry.view<Sprite2DComponent, SpriteAnimationComponent>();
            SpriteAnimationAsset *spriteAnimationAsset = nullptr;
            int spriteAnimationId = 0;
            for (auto [entity, sprite, animation] : view.each())
            {
                if (entity == entt::tombstone && !scene->entityRegistry.valid(entity))
                    continue;
                
                animation.countDown -= _deltaTime*animation.speed;

                if (animation.countDown < 0.0f)
                {
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
                        animation.flipY
                    );
                }

                if (animation.redraw)
                {
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
                        animation.flipY
                    );
                }
            }
        }
    };
} // end of Canis namespace