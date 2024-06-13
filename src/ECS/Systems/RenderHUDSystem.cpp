#include <Canis/ECS/Systems/RenderHUDSystem.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <SDL_keyboard.h>

#include <Canis/Entity.hpp>

#include <Canis/Camera2D.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetManager.hpp>

#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>

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

        m_textRenderer.m_isCreated = true;
        m_textRenderer.scene = scene;
        m_textRenderer.inputManager = inputManager;
        m_textRenderer.time = time;
        m_textRenderer.camera = camera;
        m_textRenderer.window = window;
        m_textRenderer.Create();
        m_textRenderer.Ready();
    }

    void RenderHUDSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        elements.clear();

        if (GetInputManager().JustPressedKey(SDLK_F2))
        {
            m_hide = !m_hide;
        }

        if (m_hide)
        {
            return;
        }

        // Draw
        auto viewUIImage = _registry.view<RectTransformComponent, ColorComponent, UIImageComponent>();
        auto viewText = _registry.view<RectTransformComponent, ColorComponent, TextComponent>();

        glm::vec2 positionAnchor = glm::vec2(0.0f);
        Canis::Entity e;
        e.scene = scene;

        // render text
        glm::mat4 projection = glm::mat4(1.0f);
        projection = glm::ortho(0.0f, static_cast<float>(window->GetScreenWidth()), 0.0f, static_cast<float>(window->GetScreenHeight()));

        float depthOffset = 0.0;
        
        for (auto [entity, rect_transform, color, image] : viewUIImage.each())
        {
            HUDElementDepth hudElementDepth;
            hudElementDepth.element = entity;
            hudElementDepth.depth = rect_transform.depth;
            hudElementDepth.isText = false;
            elements.push_back(hudElementDepth);

            if (depthOffset > hudElementDepth.depth)
                depthOffset = hudElementDepth.depth;
        }

        for (auto [entity, rect_transform, color, text] : viewText.each())
        {
            HUDElementDepth hudElementDepth;
            hudElementDepth.element = entity;
            hudElementDepth.depth = rect_transform.depth;
            hudElementDepth.isText = true;
            elements.push_back(hudElementDepth);

            if (depthOffset > hudElementDepth.depth)
                depthOffset = hudElementDepth.depth;
        }

        for (int i = 0; i < elements.size(); i++)
        {
            elements[i].depthOffset = depthOffset;
        }

        std::stable_sort(elements.begin(), elements.end(), [](const HUDElementDepth &a, const HUDElementDepth &b)
                         { return ((a.depth + a.depthOffset) > (b.depth + b.depthOffset)); });

        bool lastWasText = true;

        for (int i = 0; i < elements.size(); i++)
        {
            Canis::RectTransformComponent &rect_transform = _registry.get<RectTransformComponent>(elements[i].element);

            if (elements[i].isText == false)
            {
                if (rect_transform.active)
                {
                    if (lastWasText)
                    {
                        glEnable(GL_DEPTH_TEST);
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glDepthFunc(GL_ALWAYS);

                        m_spriteRenderer.Begin(m_glyphSortType);
                    }

                    lastWasText = false;

                    ColorComponent &color = _registry.get<ColorComponent>(elements[i].element);
                    UIImageComponent &image = _registry.get<UIImageComponent>(elements[i].element);

                    if (image.textureHandle.id == -1)
                    {
                        continue;
                    }

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
                        glm::vec4(rect_transform.position.x + positionAnchor.x, rect_transform.position.y + positionAnchor.y, size.x * rect_transform.scale, size.y * rect_transform.scale),
                        image.uv,
                        image.textureHandle.texture,
                        rect_transform.depth,
                        color,
                        rect_transform.rotation,
                        offset,
                        rect_transform.rotationOriginOffset);
                }
            }
            else
            {
                if (rect_transform.active)
                {
                    if (lastWasText == false)
                    {
                        m_spriteRenderer.End();
                        m_spriteRenderer.SpriteRenderBatch(false);

                        glDisable(GL_DEPTH_TEST);
                        glDisable(GL_BLEND);
                    }

                    lastWasText = true;

                    ColorComponent &color = _registry.get<ColorComponent>(elements[i].element);
                    TextComponent &text = _registry.get<TextComponent>(elements[i].element);

                    if (text.assetId == -1)
                    {
                        continue;
                    }

                    glEnable(GL_DEPTH_TEST);
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    glDepthFunc(GL_ALWAYS);

                    m_textRenderer.textShader.Use();
                    glUniformMatrix4fv(glGetUniformLocation(m_textRenderer.textShader.GetProgramID(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

                    e.entityHandle = elements[i].element;
                    positionAnchor = GetAnchor((Canis::RectAnchor)rect_transform.anchor,
                                               (float)window->GetScreenWidth(),
                                               (float)window->GetScreenHeight());

                    m_textRenderer.RenderText(&e,
                               m_textRenderer.textShader,
                               text.text,
                               rect_transform.position.x + positionAnchor.x,
                               rect_transform.position.y + positionAnchor.y,
                               rect_transform.scale,
                               color.color,
                               text.assetId,
                               text.alignment,
                               rect_transform.originOffset,
                               text._status,
                               rect_transform.rotation);

                    m_textRenderer.textShader.UnUse();

                    glDisable(GL_DEPTH_TEST);
                    glDisable(GL_BLEND);
                }
            }
        }

        if (lastWasText == false)
        {
            m_spriteRenderer.End();
            m_spriteRenderer.SpriteRenderBatch(false);

            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
        }
    }
} // end of Canis namespace