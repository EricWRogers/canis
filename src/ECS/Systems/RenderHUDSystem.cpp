#include <Canis/ECS/Systems/RenderHUDSystem.hpp>

#include <glm/glm.hpp>
#include <vector>

#include <Canis/Camera2D.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetManager.hpp>

#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>

namespace Canis
{
    void RenderHUDSystem::SetSort(GlyphSortType _sortType)
    {
        m_glyphSortType = _sortType;
        m_spriteRenderer.SetSort(m_glyphSortType);
    }

    void RenderHUDSystem::Create()
    {
        m_spriteRenderer.m_isCreated = true;
        m_spriteRenderer.scene = scene;
        m_spriteRenderer.inputManager = inputManager;
        m_spriteRenderer.time = time;
        m_spriteRenderer.camera = camera;
        m_spriteRenderer.assetManager = assetManager;
        m_spriteRenderer.window = window;
        m_spriteRenderer.SetSort(m_glyphSortType);
        m_spriteRenderer.Create();
        m_spriteRenderer.Ready();
    }

    void RenderHUDSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        m_spriteRenderer.Begin(m_glyphSortType);

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

                m_spriteRenderer.Draw(
                    glm::vec4(rect_transform.position.x + positionAnchor.x + rect_transform.originOffset.x, rect_transform.position.y + positionAnchor.y + rect_transform.originOffset.y, rect_transform.size.x, rect_transform.size.y),
                    image.uv,
                    image.texture,
                    rect_transform.depth,
                    color);
            }
        }

        // Canis::FatalError("Stop");

        m_spriteRenderer.End();
        m_spriteRenderer.SpriteRenderBatch(false);
    }

    bool DecodeRenderHUDSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::RenderHUDSystem")
        {
            _scene->CreateRenderSystem<RenderHUDSystem>();
            return true;
        }
        return false;
    }
} // end of Canis namespace