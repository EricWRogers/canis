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
        m_spriteRenderer.window = window;
        m_spriteRenderer.SetSort(m_glyphSortType);
        m_spriteRenderer.Create();
        m_spriteRenderer.Ready();
    }

    void RenderHUDSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_ALPHA);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_ALWAYS);

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

                glm::vec2 size = rect_transform.size;
                glm::vec2 offset = rect_transform.originOffset;

                if (rect_transform.scaleWithScreen == ScaleWithScreen::WIDTH)
                {
                    float scaleX = static_cast<float>(window->GetScreenWidth()) / 1280.0f;
                    size.x *= scaleX;
                    size.y *= scaleX;
                    offset.x *= scaleX;
                    offset.y *= scaleX;
                }

                if (rect_transform.scaleWithScreen == ScaleWithScreen::HEIGHT)
                {
                    float scaleY = static_cast<float>(window->GetScreenHeight()) / 800.0f;
                    size.x *= scaleY;
                    size.y *= scaleY;
                    offset.x *= scaleY;
                    offset.y *= scaleY;
                }

                if (rect_transform.scaleWithScreen == ScaleWithScreen::WIDTHANDHEIGHT)
                {
                    float scaleX = static_cast<float>(window->GetScreenWidth()) / 1280.0f;
                    float scaleY = static_cast<float>(window->GetScreenHeight()) / 800.0f;
                    size.x *= scaleX;
                    size.y *= scaleY;
                    offset.x *= scaleX;
                    offset.y *= scaleY;
                }

                m_spriteRenderer.DrawUI(
                    glm::vec4(rect_transform.position.x + positionAnchor.x + offset.x, rect_transform.position.y + positionAnchor.y + offset.y, size.x * rect_transform.scale, size.y * rect_transform.scale),
                    image.uv,
                    image.texture,
                    rect_transform.depth,
                    color,
                    rect_transform.rotation,
                    rect_transform.originOffset);
            }
        }

        // Canis::FatalError("Stop");

        m_spriteRenderer.End();
        m_spriteRenderer.SpriteRenderBatch(false);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_ALPHA);
        glDisable(GL_BLEND);
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