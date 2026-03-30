#include <Canis/ECS/Systems/SpriteAnimationSystem.hpp>

#include <vector>

#include <Canis/Time.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>
#include <Canis/AssetManager.hpp>

namespace Canis
{
    void SpriteAnimationSystem::Ready()
    {
        // No cached views required.
    }

    void SpriteAnimationSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        f32 deltaTime = _deltaTime;
        SpriteAnimationAsset *spriteAnimationAsset = nullptr;
        int spriteAnimationId = 0;

        auto animationView = _registry.view<SpriteAnimation>();
        for (const entt::entity entityHandle : animationView)
        {
            SpriteAnimation& animation = animationView.get<SpriteAnimation>(entityHandle);

            animation.countDown -= deltaTime * animation.speed;

            if (animation.countDown < 0.0f)
            {
                Sprite2D* sprite = _registry.try_get<Sprite2D>(entityHandle);
                if (sprite == nullptr)
                    continue;

                animation.index++;
                animation.redraw = false;

                if (animation.id != spriteAnimationId || spriteAnimationAsset == nullptr)
                {
                    spriteAnimationId = animation.id;
                    spriteAnimationAsset = AssetManager::Get<SpriteAnimationAsset>(spriteAnimationId);
                }

                if (animation.index >= spriteAnimationAsset->frames.size())
                    animation.index = 0;

                animation.countDown = spriteAnimationAsset->frames[animation.index].timeOnFrame;
                sprite->textureHandle.id = spriteAnimationAsset->frames[animation.index].textureId;
                sprite->textureHandle.texture = AssetManager::GetTexture(sprite->textureHandle.id)->GetGLTexture();

                sprite->GetSpriteFromTextureAtlas(
                    spriteAnimationAsset->frames[animation.index].offsetX,
                    spriteAnimationAsset->frames[animation.index].offsetY,
                    spriteAnimationAsset->frames[animation.index].row,
                    spriteAnimationAsset->frames[animation.index].col,
                    spriteAnimationAsset->frames[animation.index].width,
                    spriteAnimationAsset->frames[animation.index].height);
            }

            if (animation.redraw)
            {
                Sprite2D* sprite = _registry.try_get<Sprite2D>(entityHandle);
                if (sprite == nullptr)
                    continue;
                
                animation.redraw = false;

                if (animation.id != spriteAnimationId || spriteAnimationAsset == nullptr)
                {
                    spriteAnimationId = animation.id;
                    spriteAnimationAsset = AssetManager::Get<SpriteAnimationAsset>(spriteAnimationId);
                }
                
                sprite->textureHandle.id = spriteAnimationAsset->frames[animation.index].textureId;
                sprite->textureHandle.texture = AssetManager::GetTexture(sprite->textureHandle.id)->GetGLTexture();

                sprite->GetSpriteFromTextureAtlas(
                    spriteAnimationAsset->frames[animation.index].offsetX,
                    spriteAnimationAsset->frames[animation.index].offsetY,
                    spriteAnimationAsset->frames[animation.index].row,
                    spriteAnimationAsset->frames[animation.index].col,
                    spriteAnimationAsset->frames[animation.index].width,
                    spriteAnimationAsset->frames[animation.index].height);
            }
        }
    }
}
