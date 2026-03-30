#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>

#include <vector>
#include <algorithm>
#include <cfloat>
#include <cmath>

#include <Canis/Math.hpp>
#include <Canis/Time.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Shader.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetHandle.hpp>
#include <Canis/AssetManager.hpp>

#include <Canis/OpenGL.hpp>

namespace Canis
{
    void SpriteRenderer2DSystem::DrawText(Entity* _entity, RectTransform* _transform, Text* _text, const Vector2& _cameraPosition, float _halfWidth, float _halfHeight)
    {
        if (_entity == nullptr || _transform == nullptr || _text == nullptr || _text->assetId < 0)
            return;

        TextAsset* font = AssetManager::GetText(_text->assetId);

        if (font == nullptr || _text->text.empty())
            return;

        Vector2 transformPos = _transform->GetPosition();
        Vector2 transformScale = _transform->GetScale();
        const float scaleX = std::abs(transformScale.x);
        const float scaleY = std::abs(transformScale.y);
        const Vector2 rectSize = _transform->GetResolvedSize();
        const Vector2 rectMin = _transform->GetRectMin() + _transform->originOffset;
        const float maxWidth = rectSize.x;
        const bool wrap = (_text->horizontalBoundary == TextBoundary::WRAP) && (maxWidth > 0.0f);
        const float wrapWidth = (maxWidth > 0.0f) ? maxWidth : 0.0f;

        float lineHeight = 0.0f;
        for (unsigned char c = 32; c < 127; c++)
            lineHeight = std::max(lineHeight, (float)font->characters[c].sizeY * scaleY);

        if (lineHeight <= 0.0f)
            lineHeight = 16.0f * std::max(scaleY, 1.0f);

        std::vector<float> lineWidths = {};
        lineWidths.reserve(8);
        float currentLineWidth = 0.0f;

        for (const unsigned char c : _text->text)
        {
            if (c == '\n')
            {
                lineWidths.push_back(currentLineWidth);
                currentLineWidth = 0.0f;
                continue;
            }

            if (c < 32 || c >= 127)
                continue;

            const float advance = (font->characters[c].advance >> 6) * scaleX;

            if (wrap && currentLineWidth > 0.0f && (currentLineWidth + advance) > wrapWidth)
            {
                lineWidths.push_back(currentLineWidth);
                currentLineWidth = 0.0f;
            }

            currentLineWidth += advance;
        }

        lineWidths.push_back(currentLineWidth);

        float layoutWidth = 0.0f;
        for (float width : lineWidths)
            layoutWidth = std::max(layoutWidth, width);

        const float layoutHeight = std::max(lineHeight, lineHeight * (float)lineWidths.size());

        if ((_text->_status & BIT::ONE) > 0)
        {
            const bool canResizeWidth = _transform->anchorMin.x == _transform->anchorMax.x;
            const bool canResizeHeight = _transform->anchorMin.y == _transform->anchorMax.y;

            if (_text->horizontalBoundary == TextBoundary::TB_OVERFLOW && canResizeWidth && scaleX != 0.0f)
                _transform->size.x = layoutWidth / std::abs(scaleX);
            if (canResizeHeight && scaleY != 0.0f)
                _transform->size.y = layoutHeight / std::abs(scaleY);
            _text->_status &= ~BIT::ONE;
        }

        auto computeLineStart = [&](float _lineWidth) -> float
        {
            if (_text->alignment == TextAlignment::RIGHT)
                return rectMin.x + rectSize.x - _lineWidth;
            if (_text->alignment == TextAlignment::CENTER)
                return rectMin.x + ((rectSize.x - _lineWidth) * 0.5f);
            return rectMin.x;
        };

        const Vector2 textRotationPivot = transformPos + _transform->rotationOriginOffset;

        i32 lineIndex = 0;
        float x = computeLineStart(lineWidths[0]);
        float y = rectMin.y + ((rectSize.y + layoutHeight) * 0.5f) - lineHeight;

        for (const unsigned char c : _text->text)
        {
            if (c == '\n')
            {
                lineIndex++;
                if (lineIndex >= (i32)lineWidths.size())
                    break;
                x = computeLineStart(lineWidths[lineIndex]);
                y -= lineHeight;
                continue;
            }

            if (c < 32 || c >= 127)
                continue;

            Character ch = font->characters[c];
            const float advance = (ch.advance >> 6) * scaleX;

            if (wrap && (x + advance) > (computeLineStart(lineWidths[lineIndex]) + wrapWidth))
            {
                lineIndex++;
                if (lineIndex >= (i32)lineWidths.size())
                    break;
                x = computeLineStart(lineWidths[lineIndex]);
                y -= lineHeight;
            }

            const float xpos = x + (ch.bearingX * scaleX);
            const float ypos = y - ((ch.sizeY - ch.bearingY) * scaleY);
            const float w = ch.sizeX * scaleX;
            const float h = ch.sizeY * scaleY;

            if (w > 0.0f && h > 0.0f)
            {
                const float uvY = 1.0f - ch.atlasPos.y - ch.atlasSize.y;
                DrawUI(
                    Vector4(xpos, ypos, w, h),
                    Vector4(ch.atlasPos.x, uvY, ch.atlasSize.x, ch.atlasSize.y),
                    GLTexture{font->GetTexture(), 0, 0},
                    _transform->GetDepth(),
                    _text->color,
                    _transform->GetRotation(),
                    Vector2(0.0f),
                    textRotationPivot);
            }

            x += advance;
        }
    }

    SpriteRenderer2DSystem::~SpriteRenderer2DSystem()
    {
        for (int i = 0; i < glyphs.size(); i++)
            delete glyphs[i];

        glyphs.clear();
    }

    void SpriteRenderer2DSystem::SortGlyphs()
    {
        switch (glyphSortType)
        {
        case GlyphSortType::BACK_TO_FRONT:
            std::stable_sort(glyphs.begin(), glyphs.end(), CompareFrontToBack);
            break;
        case GlyphSortType::FRONT_TO_BACK:
            std::stable_sort(glyphs.begin(), glyphs.end(), CompareBackToFront);
            break;
        case GlyphSortType::TEXTURE:
            std::stable_sort(glyphs.begin(), glyphs.end(), CompareTexture);
            break;
        default:
            break;
        }
    }

    void SpriteRenderer2DSystem::CreateRenderBatches()
    {
        int gSize = glyphs.size();
        if (indices.size() < gSize * 6)
        {
            indices.resize(gSize * 6);
            int ci = 0;
            int cv = 0;
            int size = gSize * 6;
            while (ci < size)
            {
                cv += 4;
                indices[ci++] = cv - 4;
                indices[ci++] = cv - 3;
                indices[ci++] = cv - 1;
                indices[ci++] = cv - 3;
                indices[ci++] = cv - 2;
                indices[ci++] = cv - 1;
            }
        }

        if (vertices.size() < gSize * 4)
            vertices.resize(gSize * 4);

        if (gSize == 0)
            return;

        int offset = 0;
        int cv = 0; // current vertex
        spriteRenderBatch.emplace_back(offset, 4, glyphs[0]->textureId);

        vertices[cv++] = glyphs[0]->topRight;
        vertices[cv++] = glyphs[0]->bottomRight;
        vertices[cv++] = glyphs[0]->bottomLeft;
        vertices[cv++] = glyphs[0]->topLeft;

        offset += 4;
        for (int cg = 1; cg < gSize; cg++)
        {
            if (glyphs[cg]->textureId != glyphs[cg - 1]->textureId)
            {
                spriteRenderBatch.emplace_back(offset, 4, glyphs[cg]->textureId);
            }
            else
            {
                spriteRenderBatch.back().numVertices += 4;
            }

            vertices[cv++] = glyphs[cg]->topRight;
            vertices[cv++] = glyphs[cg]->bottomRight;
            vertices[cv++] = glyphs[cg]->bottomLeft;
            vertices[cv++] = glyphs[cg]->topLeft;

            offset += 4;
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, gSize * 4 * sizeof(SpriteVertex), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, gSize * 4 * sizeof(SpriteVertex), vertices.data());

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, gSize * 6 * sizeof(unsigned int), nullptr, GL_DYNAMIC_DRAW);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, gSize * 6 * sizeof(unsigned int), indices.data());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    void SpriteRenderer2DSystem::Begin(GlyphSortType sortType)
    {
        glyphSortType = sortType;
        spriteRenderBatch.clear();
        glyphsCurrentIndex = 0;
    }

    void SpriteRenderer2DSystem::End()
    {
        if (glyphsCurrentIndex < glyphs.size())
        {
            for (int i = glyphsCurrentIndex; i < glyphs.size(); i++)
                delete glyphs[i];

            glyphs.resize(glyphsCurrentIndex);
        }

        SortGlyphs();
        CreateRenderBatches();
    }

    void SpriteRenderer2DSystem::DrawUI(const Vector4 &destRect, const Vector4 &uvRect, const GLTexture &texture, float depth, const Color &color, const float &angle, const Vector2 &origin, const Vector2 &rotationOriginOffset)
    {
        Glyph *newGlyph;

        if (glyphsCurrentIndex < glyphs.size())
        {
            newGlyph = glyphs[glyphsCurrentIndex];
        }
        else
        {
            newGlyph = new Glyph;
            glyphs.push_back(newGlyph);
        }

        Vector2 topLeft(destRect.x + origin.x, destRect.y + origin.y + destRect.w);
        Vector2 bottomLeft(destRect.x + origin.x, destRect.y + origin.y);
        Vector2 bottomRight(destRect.x + origin.x + destRect.z, destRect.y + origin.y);
        Vector2 topRight(destRect.x + origin.x + destRect.z, destRect.y + origin.y + destRect.w);

         /*

         Vector2 topLeft(origin.x, origin.y + destRect.w);
        Vector2 bottomLeft(origin.x, origin.y);
        Vector2 bottomRight(origin.x + destRect.z, origin.y);
        Vector2 topRight(origin.x + destRect.z, origin.y + destRect.w);
        
        newGlyph->topLeft.position.x = destRect.x;
        newGlyph->topLeft.position.y = destRect.y + destRect.w;
        newGlyph->topLeft.position.z = depth;
        newGlyph->topLeft.color = color;
        newGlyph->topLeft.uv = Vector2(uvRect.x, uvRect.y + uvRect.w);

        newGlyph->bottomLeft.position.x = destRect.x;
        newGlyph->bottomLeft.position.y = destRect.y;
        newGlyph->bottomLeft.position.z = depth;
        newGlyph->bottomLeft.color = color;
        newGlyph->bottomLeft.uv = Vector2(uvRect.x, uvRect.y);

        newGlyph->bottomRight.position.x = destRect.x + destRect.z;
        newGlyph->bottomRight.position.y = destRect.y;
        newGlyph->bottomRight.position.z = depth;
        newGlyph->bottomRight.color = color;
        newGlyph->bottomRight.uv = Vector2(uvRect.x + uvRect.z, uvRect.y);

        newGlyph->topRight.position.x = destRect.x + destRect.z;
        newGlyph->topRight.position.y = destRect.y + destRect.w;
        newGlyph->topRight.position.z = depth;
        newGlyph->topRight.color = color;
        newGlyph->topRight.uv = Vector2(uvRect.x + uvRect.z, uvRect.y + uvRect.w);*/

        if (angle != 0.0f)
        {
            RotatePointAroundPivot(topLeft, rotationOriginOffset, -angle);
            RotatePointAroundPivot(bottomLeft, rotationOriginOffset, -angle);
            RotatePointAroundPivot(bottomRight, rotationOriginOffset, -angle);
            RotatePointAroundPivot(topRight, rotationOriginOffset, -angle);
        }

        newGlyph->textureId = texture.id;
        newGlyph->depth = depth;
        newGlyph->angle = -angle;

        newGlyph->topLeft.position.x = topLeft.x;
        newGlyph->topLeft.position.y = topLeft.y;
        newGlyph->topLeft.position.z = depth;
        newGlyph->topLeft.color = color;
        newGlyph->topLeft.uv = Vector2(uvRect.x, uvRect.y + uvRect.w);

        newGlyph->bottomLeft.position.x = bottomLeft.x;
        newGlyph->bottomLeft.position.y = bottomLeft.y;
        newGlyph->bottomLeft.position.z = depth;
        newGlyph->bottomLeft.color = color;
        newGlyph->bottomLeft.uv = Vector2(uvRect.x, uvRect.y);

        newGlyph->bottomRight.position.x = bottomRight.x;
        newGlyph->bottomRight.position.y = bottomRight.y;
        newGlyph->bottomRight.position.z = depth;
        newGlyph->bottomRight.color = color;
        newGlyph->bottomRight.uv = Vector2(uvRect.x + uvRect.z, uvRect.y);

        newGlyph->topRight.position.x = topRight.x;
        newGlyph->topRight.position.y = topRight.y;
        newGlyph->topRight.position.z = depth;
        newGlyph->topRight.color = color;
        newGlyph->topRight.uv = Vector2(uvRect.x + uvRect.z, uvRect.y + uvRect.w);
        
        glyphsCurrentIndex++;
    }

    void SpriteRenderer2DSystem::Draw(const Vector4 &destRect, const Vector4 &uvRect, const GLTexture &texture, const float &depth, const Color &color, const float &angle, const Vector2 &origin)
    {
        Glyph *newGlyph;

        if (glyphsCurrentIndex < glyphs.size())
        {
            newGlyph = glyphs[glyphsCurrentIndex];
        }
        else
        {
            newGlyph = new Glyph;
            glyphs.push_back(newGlyph);
        }

        newGlyph->textureId = texture.id;
        newGlyph->depth = depth;
        newGlyph->angle = angle;

        Vector2 halfDims(destRect.z / 2.0f, destRect.w / 2.0f);

        Vector2 topLeft(-halfDims.x + origin.x, halfDims.y + origin.y);
        Vector2 bottomLeft(-halfDims.x + origin.x, -halfDims.y + origin.y);
        Vector2 bottomRight(halfDims.x + origin.x, -halfDims.y + origin.y);
        Vector2 topRight(halfDims.x + origin.x, halfDims.y + origin.y);

        if (angle != 0.0f)
        {
            float cAngle = cos(angle);
            float sAngle = sin(angle);
            RotatePoint(topLeft, cAngle, sAngle);
            RotatePoint(bottomLeft, cAngle, sAngle);
            RotatePoint(bottomRight, cAngle, sAngle);
            RotatePoint(topRight, cAngle, sAngle);
        }

        // Glyph

        // newGlyph->topLeft.position = Vector3(topLeft.x + destRect.x, topLeft.y + destRect.y, depth);
        newGlyph->topLeft.position.x = topLeft.x + destRect.x;
        newGlyph->topLeft.position.y = topLeft.y + destRect.y;
        newGlyph->topLeft.position.z = depth;
        newGlyph->topLeft.color = color;
        newGlyph->topLeft.uv.x = uvRect.x;
        newGlyph->topLeft.uv.y = uvRect.y + uvRect.w;

        // newGlyph->bottomLeft.position = Vector3(bottomLeft.x + destRect.x, bottomLeft.y + destRect.y, depth);
        newGlyph->bottomLeft.position.x = bottomLeft.x + destRect.x;
        newGlyph->bottomLeft.position.y = bottomLeft.y + destRect.y;
        newGlyph->bottomLeft.position.z = depth;
        newGlyph->bottomLeft.color = color;
        // newGlyph->bottomLeft.uv = Vector2(uvRect.x, uvRect.y);
        newGlyph->bottomLeft.uv.x = uvRect.x;
        newGlyph->bottomLeft.uv.y = uvRect.y;

        // newGlyph->bottomRight.position = Vector3(bottomRight.x + destRect.x, bottomRight.y + destRect.y, depth);
        newGlyph->bottomRight.position.x = bottomRight.x + destRect.x;
        newGlyph->bottomRight.position.y = bottomRight.y + destRect.y;
        newGlyph->bottomRight.position.z = depth;
        newGlyph->bottomRight.color = color;
        // newGlyph->bottomRight.uv = Vector2(uvRect.x + uvRect.z, uvRect.y);
        newGlyph->bottomRight.uv.x = uvRect.x + uvRect.z;
        newGlyph->bottomRight.uv.y = uvRect.y;

        // newGlyph->topRight.position = Vector3(topRight.x + destRect.x, topRight.y + destRect.y, depth);
        newGlyph->topRight.position.x = topRight.x + destRect.x;
        newGlyph->topRight.position.y = topRight.y + destRect.y;
        newGlyph->topRight.position.z = depth;
        newGlyph->topRight.color = color;
        // newGlyph->topRight.uv = Vector2(uvRect.x + uvRect.z, uvRect.y + uvRect.w);
        newGlyph->topRight.uv.x = uvRect.x + uvRect.z;
        newGlyph->topRight.uv.y = uvRect.y + uvRect.w;

        glyphsCurrentIndex++;
    }

    void SpriteRenderer2DSystem::SpriteRenderBatch(bool use2DCamera, const Matrix4* overrideProjection)
    {
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        spriteShader->Use();
        spriteShader->SetFloat("TIME", m_time);
        glBindVertexArray(vao);

        Matrix4 projection = Matrix4(1.0f);

        if (overrideProjection != nullptr) {
            projection = *overrideProjection;
        } else if (use2DCamera) {
            camera2D->UpdateMatrix();
            projection = camera2D->GetCameraMatrix();
        } else {
            projection = glm::ortho(0.0f, static_cast<float>(window->GetScreenWidth()), 0.0f, static_cast<float>(window->GetScreenHeight()), 0.0f, 100.0f);
        }

        spriteShader->SetMat4("P", projection);

        for (int i = 0; i < spriteRenderBatch.size(); i++)
        {
            glBindTexture(GL_TEXTURE_2D, spriteRenderBatch[i].texture);

            glDrawElements(GL_TRIANGLES, (spriteRenderBatch[i].numVertices / 4) * 6, GL_UNSIGNED_INT, (void *)((spriteRenderBatch[i].offset / 4) * 6 * sizeof(unsigned int))); // spriteRenderBatch[i].offset, spriteRenderBatch[i].numVertices);
        }

        glBindVertexArray(0);
        spriteShader->UnUse();
    }

    void SpriteRenderer2DSystem::CreateVertexArray()
    {
        if (vao == 0)
            glGenVertexArrays(1, &vao);

        glBindVertexArray(vao);

        if (vbo == 0)
            glGenBuffers(1, &vbo);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);

        if (ebo == 0)
            glGenBuffers(1, &ebo);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void *)0);
        // color
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void *)(3 * sizeof(float)));
        // uv
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void *)(7 * sizeof(float)));

        glBindVertexArray(0);
    }

    void SpriteRenderer2DSystem::SetSort(GlyphSortType _sortType)
    {
        glyphSortType = _sortType;
    }

    void SpriteRenderer2DSystem::Create()
    {
        int id = AssetManager::LoadShader("assets/shaders/sprite");
        Canis::Shader *shader = AssetManager::Get<Canis::ShaderAsset>(id)->GetShader();

        if (!shader->IsLinked())
        {
            Debug::Log("Link");
            shader->AddAttribute("vertexPosition");
            shader->AddAttribute("vertexColor");
            shader->AddAttribute("vertexUV");

            shader->Link();
        }

        spriteShader = shader;

        CreateVertexArray();
    }

    void SpriteRenderer2DSystem::Ready()
    {
        // No cached views required.
    }

    void SpriteRenderer2DSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        m_time += _deltaTime;
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_LESS);

        bool cameraFound = false;
        bool editorCameraOverride = scene->HasEditorCamera2DOverride();
        Matrix4 overrideProjection = Matrix4(1.0f);
        camera2D = nullptr;

        auto cameraView = _registry.view<Camera2D>();
        for (const entt::entity entityHandle : cameraView)
        {
            camera2D = &cameraView.get<Camera2D>(entityHandle);
            cameraFound = true;
            break;
        }

        if (editorCameraOverride)
        {
            cameraFound = true;
            overrideProjection = scene->GetEditorCamera2DMatrix();
        }

        float halfWidth = window->GetScreenWidth() / 2.0f;
        float halfHeight = window->GetScreenHeight() / 2.0f;
        Vector2 camPos;

        if (editorCameraOverride)
            camPos = scene->GetEditorCamera2DPosition();
        else if (cameraFound)
            camPos = camera2D->GetPosition();
        else
            camPos = Vector2(0.0f);

        auto renderPass = [&](unsigned int _renderMode, const Matrix4* _projectionOverride, bool _useCameraProjection) -> void
        {
            Begin(glyphSortType);

            auto renderView = _registry.view<RectTransform>();
            const std::vector<Entity*>& sceneEntities = scene->GetEntities();
            std::vector<bool> visited(sceneEntities.size(), false);

            auto wasVisited = [&](Entity* _entity) -> bool
            {
                return _entity != nullptr &&
                    _entity->id >= 0 &&
                    _entity->id < static_cast<int>(visited.size()) &&
                    visited[static_cast<std::size_t>(_entity->id)];
            };

            auto markVisited = [&](Entity* _entity) -> void
            {
                if (_entity == nullptr ||
                    _entity->id < 0 ||
                    _entity->id >= static_cast<int>(visited.size()))
                {
                    return;
                }

                visited[static_cast<std::size_t>(_entity->id)] = true;
            };

            auto renderTree = [&](auto&& _self, Entity* _entity) -> void
            {
                if (_entity == nullptr || !_entity->HasComponent<RectTransform>() || wasVisited(_entity))
                    return;

                markVisited(_entity);

                RectTransform &transform = _entity->GetComponent<RectTransform>();
                const bool shouldRender = transform.IsActiveInHierarchy() &&
                    transform.GetCanvasRenderMode() == _renderMode;

                if (shouldRender)
                {
                    Sprite2D* sprite = _registry.try_get<Sprite2D>(_entity->GetHandle());
                    Text* text = _registry.try_get<Text>(_entity->GetHandle());
                    const Vector2 position = transform.GetPosition();
                    const Vector2 size = transform.GetResolvedSize();

                    if (sprite != nullptr &&
                        (_renderMode == CanvasRenderMode::SCREEN_SPACE_OVERLAY ||
                         (position.x > camPos.x - size.x - halfWidth  &&
                          position.x < camPos.x + size.x + halfWidth  &&
                          position.y > camPos.y - size.y - halfHeight &&
                          position.y < camPos.y + size.y + halfHeight)))
                    {
                        const Vector2 pivotOffset(
                            ((0.5f - transform.pivot.x) * size.x) + transform.originOffset.x,
                            ((0.5f - transform.pivot.y) * size.y) + transform.originOffset.y);

                        Draw(
                            Vector4(position.x, position.y, size.x, size.y),
                            sprite->uv,
                            sprite->textureHandle.texture,
                            transform.GetDepth(),
                            sprite->color,
                            transform.GetRotation(),
                            pivotOffset);
                    }

                    if (text != nullptr)
                        DrawText(_entity, &transform, text, camPos, halfWidth, halfHeight);
                }

                for (Entity* child : transform.children)
                    _self(_self, child);
            };

            auto renderRootTree = [&](Entity* _entity) -> void
            {
                renderTree(renderTree, _entity);
            };

            auto canvasView = _registry.view<Canvas, RectTransform>();
            for (const entt::entity entityHandle : canvasView)
            {
                RectTransform &transform = canvasView.get<RectTransform>(entityHandle);
                Entity *entity = transform.entity;
                if (entity == nullptr || wasVisited(entity))
                    continue;

                if (transform.parent != nullptr && transform.parent->HasComponent<RectTransform>())
                    continue;

                renderRootTree(entity);
            }

            for (const entt::entity entityHandle : renderView)
            {
                RectTransform &transform = renderView.get<RectTransform>(entityHandle);
                Entity *entity = transform.entity;
                if (entity == nullptr || wasVisited(entity))
                    continue;

                if (transform.parent != nullptr && transform.parent->HasComponent<RectTransform>())
                    continue;

                renderRootTree(entity);
            }

            for (const entt::entity entityHandle : renderView)
            {
                RectTransform &transform = renderView.get<RectTransform>(entityHandle);
                Entity *entity = transform.entity;
                if (entity == nullptr || wasVisited(entity))
                    continue;

                renderRootTree(entity);
            }

            End();
            SpriteRenderBatch(_useCameraProjection, _projectionOverride);
        };

        Matrix4 centeredCameraProjection = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, -100.0f, 100.0f);

        renderPass(Canis::CanvasRenderMode::SCREEN_SPACE_OVERLAY, &centeredCameraProjection, false);

        const Matrix4* cameraProjectionOverride = nullptr;
        bool useCameraProjection = true;

        if (editorCameraOverride)
        {
            cameraProjectionOverride = &overrideProjection;
            useCameraProjection = false;
        }
        else if (!cameraFound)
        {
            cameraProjectionOverride = &centeredCameraProjection;
            useCameraProjection = false;
        }

        renderPass(Canis::CanvasRenderMode::SCREEN_SPACE_CAMERA, cameraProjectionOverride, useCameraProjection);
        renderPass(Canis::CanvasRenderMode::WORLD_SPACE, cameraProjectionOverride, useCameraProjection);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
} // end of Canis namespace
