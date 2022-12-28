#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/Camera2D.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Data/Glyph.hpp>
#include <Canis/External/entt.hpp>

#include <Canis/ECS/Systems/System.hpp>
#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>

namespace Canis
{
    class RenderHUDSystem : public System
    {
    private:
        SpriteRenderer2DSystem spriteRenderer;
        GlyphSortType glyphSortType = GlyphSortType::FRONT_TO_BACK;

    public:
        RenderHUDSystem(std::string _name) : System(_name) {}

        void Init(GlyphSortType sortType, Shader *shader)
        {
            glyphSortType = sortType;
            spriteRenderer.window = window;
            spriteRenderer.Init(sortType, shader);
            spriteRenderer.Create();
            spriteRenderer.Ready();
        }

        void Create() {}
        void Ready() {}
        void Update(entt::registry &_registry, float _deltaTime)
        {
            spriteRenderer.Begin(glyphSortType);

            // Draw
            auto view = _registry.view<RectTransformComponent, ColorComponent, UIImageComponent>();
            glm::vec2 positionAnchor = glm::vec2(0.0f);

            for (auto [entity, rect_transform, color, image] : view.each())
            {
                if (rect_transform.active)
                {
                    positionAnchor = GetAnchor((Canis::RectAnchor)rect_transform.anchor,
                        (float)window->GetScreenWidth(),
                        (float)window->GetScreenHeight());
                    
                    spriteRenderer.Draw(
                        glm::vec4(rect_transform.position.x + positionAnchor.x, rect_transform.position.y + positionAnchor.y, rect_transform.size.x, rect_transform.size.y),
                        image.uv,
                        image.texture,
                        rect_transform.depth,
                        color);
                }
                // Canis::Log("Texture ID : " + std::to_string(image.texture.id));
            }

            // Canis::FatalError("Stop");

            spriteRenderer.End();
            spriteRenderer.SpriteRenderBatch(false);
        }
    };
} // end of Canis namespace