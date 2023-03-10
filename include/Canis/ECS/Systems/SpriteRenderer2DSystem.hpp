#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/Camera2D.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Data/Glyph.hpp>
#include <Canis/External/entt.hpp>

#include <Canis/ECS/Systems/System.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/Camera2DComponent.hpp>

namespace Canis
{
    enum class GlyphSortType
    {
        NONE,
        FRONT_TO_BACK,
        BACK_TO_FRONT,
        TEXTURE
    };

    class RenderBatch
    {
    public:
        RenderBatch(GLuint Offset, GLuint NumVertices, GLuint Texture) : offset(Offset),
                                                                         numVertices(NumVertices), texture(Texture) {}
        GLuint offset;
        GLuint numVertices;
        GLuint texture;
    };

    class SpriteRenderer2DSystem : public System
    {
    public:
        GlyphSortType glyphSortType = GlyphSortType::FRONT_TO_BACK;
        std::vector<Glyph *> glyphs;
        std::vector<SpriteVertex> vertices = {};
        std::vector<unsigned int> indices = {};
        std::vector<RenderBatch> spriteRenderBatch;
        Shader *spriteShader;
        Camera2D camera2D;

        unsigned int vbo = 0;
        unsigned int vao = 0;
        unsigned int ebo = 0;
        unsigned int glyphsCurrentIndex = 0;
        unsigned int glyphsMaxIndex = 0;

        static bool CompareFrontToBack(Glyph *a, Glyph *b) { return (a->depth < b->depth); }
        static bool CompareBackToFront(Glyph *a, Glyph *b) { return (a->depth > b->depth); }
        static bool CompareTexture(Glyph *a, Glyph *b) { return (a->textureId < b->textureId); }

        SpriteRenderer2DSystem() : System() {}

        ~SpriteRenderer2DSystem()
        {
            for (int i = 0; i < glyphs.size(); i++)
                delete glyphs[i];

            glyphs.clear();
        }

        void SortGlyphs()
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

        void CreateRenderBatches()
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

        void Begin(GlyphSortType sortType)
        {
            glyphSortType = sortType;
            spriteRenderBatch.clear();
            glyphsCurrentIndex = 0;

            // for (int i = 0; i < glyphs.size(); i++)
            //     delete glyphs[i];

            // glyphs.clear();
        }

        void End()
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

        inline void RotatePoint(glm::vec2 &point, const float &cAngle, const float &sAngle)
        {
            point.x = point.x * cAngle - point.y * sAngle;
            point.y = point.x * sAngle + point.y * cAngle;
        }

        void Draw(const glm::vec4 &destRect, const glm::vec4 &uvRect, const GLTexture &texture, float depth, const ColorComponent &color)
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
            newGlyph->angle = 0.0f;

            newGlyph->topLeft.position = glm::vec3(destRect.x, destRect.y + destRect.w, depth);
            newGlyph->topLeft.color = color.color;
            newGlyph->topLeft.uv = glm::vec2(uvRect.x, uvRect.y + uvRect.w);

            newGlyph->bottomLeft.position = glm::vec3(destRect.x, destRect.y, depth);
            newGlyph->bottomLeft.color = color.color;
            newGlyph->bottomLeft.uv = glm::vec2(uvRect.x, uvRect.y);

            newGlyph->bottomRight.position = glm::vec3(destRect.x + destRect.z, destRect.y, depth);
            newGlyph->bottomRight.color = color.color;
            newGlyph->bottomRight.uv = glm::vec2(uvRect.x + uvRect.z, uvRect.y);

            newGlyph->topRight.position = glm::vec3(destRect.x + destRect.z, destRect.y + destRect.w, depth);
            newGlyph->topRight.color = color.color;
            newGlyph->topRight.uv = glm::vec2(uvRect.x + uvRect.z, uvRect.y + uvRect.w);

            glyphsCurrentIndex++;
        }

        void Draw(const glm::vec4 &destRect, const glm::vec4 &uvRect, const GLTexture &texture, const float &depth, const ColorComponent &color, const float &angle, const glm::vec2 &origin)
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

            glm::vec2 halfDims(destRect.z / 2.0f, destRect.w / 2.0f);

            glm::vec2 topLeft(-halfDims.x + origin.x, halfDims.y + origin.y);
            glm::vec2 bottomLeft(-halfDims.x + origin.x, -halfDims.y + origin.y);
            glm::vec2 bottomRight(halfDims.x + origin.x, -halfDims.y + origin.y);
            glm::vec2 topRight(halfDims.x + origin.x, halfDims.y + origin.y);

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

            // newGlyph->topLeft.position = glm::vec3(topLeft.x + destRect.x, topLeft.y + destRect.y, depth);
            newGlyph->topLeft.position.x = topLeft.x + destRect.x;
            newGlyph->topLeft.position.y = topLeft.y + destRect.y;
            newGlyph->topLeft.position.z = depth;
            newGlyph->topLeft.color = color.color;
            newGlyph->topLeft.uv.x = uvRect.x;
            newGlyph->topLeft.uv.y = uvRect.y + uvRect.w;

            // newGlyph->bottomLeft.position = glm::vec3(bottomLeft.x + destRect.x, bottomLeft.y + destRect.y, depth);
            newGlyph->bottomLeft.position.x = bottomLeft.x + destRect.x;
            newGlyph->bottomLeft.position.y = bottomLeft.y + destRect.y;
            newGlyph->bottomLeft.position.z = depth;
            newGlyph->bottomLeft.color = color.color;
            // newGlyph->bottomLeft.uv = glm::vec2(uvRect.x, uvRect.y);
            newGlyph->bottomLeft.uv.x = uvRect.x;
            newGlyph->bottomLeft.uv.y = uvRect.y;

            // newGlyph->bottomRight.position = glm::vec3(bottomRight.x + destRect.x, bottomRight.y + destRect.y, depth);
            newGlyph->bottomRight.position.x = bottomRight.x + destRect.x;
            newGlyph->bottomRight.position.y = bottomRight.y + destRect.y;
            newGlyph->bottomRight.position.z = depth;
            newGlyph->bottomRight.color = color.color;
            // newGlyph->bottomRight.uv = glm::vec2(uvRect.x + uvRect.z, uvRect.y);
            newGlyph->bottomRight.uv.x = uvRect.x + uvRect.z;
            newGlyph->bottomRight.uv.y = uvRect.y;

            // newGlyph->topRight.position = glm::vec3(topRight.x + destRect.x, topRight.y + destRect.y, depth);
            newGlyph->topRight.position.x = topRight.x + destRect.x;
            newGlyph->topRight.position.y = topRight.y + destRect.y;
            newGlyph->topRight.position.z = depth;
            newGlyph->topRight.color = color.color;
            // newGlyph->topRight.uv = glm::vec2(uvRect.x + uvRect.z, uvRect.y + uvRect.w);
            newGlyph->topRight.uv.x = uvRect.x + uvRect.z;
            newGlyph->topRight.uv.y = uvRect.y + uvRect.w;

            glyphsCurrentIndex++;
        }

        /*void SpriteBatch::draw(const glm::vec4 &destRect, const glm::vec4 &uvRect, const GLTexture &texture, float depth, const Color &color, glm::vec2 direction)
        {
            const glm::vec2 right(1.0f, 0.0f);
            float angle = acos(glm::dot(right, direction));
            if (direction.y < 0.0f) angle *= -1;
            draw(destRect,uvRect,texture,depth,color,angle);
        }*/

        void SpriteRenderBatch(bool use2DCamera)
        {
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE0);
            spriteShader->Use();
            glBindVertexArray(vao);
            glm::mat4 projection = glm::mat4(1.0f);

            if (use2DCamera)
                projection = camera2D.GetCameraMatrix();
            else
                projection = glm::ortho(0.0f, static_cast<float>(window->GetScreenWidth()), 0.0f, static_cast<float>(window->GetScreenHeight()));

            spriteShader->SetMat4("P", projection);

            for (int i = 0; i < spriteRenderBatch.size(); i++)
            {
                glBindTexture(GL_TEXTURE_2D, spriteRenderBatch[i].texture);

                glDrawElements(GL_TRIANGLES, (spriteRenderBatch[i].numVertices / 4) * 6, GL_UNSIGNED_INT, (void *)((spriteRenderBatch[i].offset / 4) * 6 * sizeof(unsigned int))); // spriteRenderBatch[i].offset, spriteRenderBatch[i].numVertices);
            }

            glBindVertexArray(0);
            spriteShader->UnUse();
        }

        void CreateVertexArray()
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

        void SetSort(GlyphSortType _sortType)
        {
            glyphSortType = _sortType;
        }

        void Create()
        {
            int id = assetManager->LoadShader("assets/shaders/sprite");
            Canis::Shader *shader = assetManager->Get<Canis::ShaderAsset>(id)->GetShader();

            if (!shader->IsLinked())
            {
                shader->AddAttribute("vertexPosition");
                shader->AddAttribute("vertexColor");
                shader->AddAttribute("vertexUV");

                shader->Link();
            }

            spriteShader = shader;

            camera2D.Init((int)window->GetScreenWidth(), (int)window->GetScreenHeight());
            CreateVertexArray();
        }

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime)
        {
            glDepthFunc(GL_ALWAYS);
            bool camFound = false;
            auto cam = _registry.view<const Camera2DComponent>();
            for (auto [entity, camera] : cam.each())
            {
                camera2D.SetPosition(camera.position);
                camera2D.SetScale(camera.scale);
                camera2D.Update();
                camFound = true;
                continue;
            }

            if (!camFound)
                return;

            Begin(glyphSortType);

            // Draw
            auto view = _registry.view<const RectTransformComponent, const Sprite2DComponent>();
            glm::vec2 positionAnchor = glm::vec2(0.0f);
            float halfWidth = window->GetScreenWidth() / 2;
            float halfHeight = window->GetScreenHeight() / 2;
            glm::vec2 camPos = camera2D.GetPosition();
            glm::vec2 anchorTable[] = {
                GetAnchor(Canis::RectAnchor::TOPLEFT, (float)window->GetScreenWidth(), (float)window->GetScreenHeight()),
                GetAnchor(Canis::RectAnchor::TOPCENTER, (float)window->GetScreenWidth(), (float)window->GetScreenHeight()),
                GetAnchor(Canis::RectAnchor::TOPRIGHT, (float)window->GetScreenWidth(), (float)window->GetScreenHeight()),
                GetAnchor(Canis::RectAnchor::CENTERLEFT, (float)window->GetScreenWidth(), (float)window->GetScreenHeight()),
                GetAnchor(Canis::RectAnchor::CENTER, (float)window->GetScreenWidth(), (float)window->GetScreenHeight()),
                GetAnchor(Canis::RectAnchor::CENTERRIGHT, (float)window->GetScreenWidth(), (float)window->GetScreenHeight()),
                GetAnchor(Canis::RectAnchor::BOTTOMLEFT, (float)window->GetScreenWidth(), (float)window->GetScreenHeight()),
                GetAnchor(Canis::RectAnchor::BOTTOMCENTER, (float)window->GetScreenWidth(), (float)window->GetScreenHeight()),
                GetAnchor(Canis::RectAnchor::BOTTOMRIGHT, (float)window->GetScreenWidth(), (float)window->GetScreenHeight())};
            ColorComponent color;
            glm::vec2 p;
            glm::vec2 s;

            for (auto [entity, rect_transform, sprite] : view.each())
            {
                p = rect_transform.position + anchorTable[rect_transform.anchor];
                s.x = rect_transform.size.x + halfWidth;
                s.y = rect_transform.size.y + halfHeight;
                if (p.x > camPos.x - s.x &&
                    p.x < camPos.x + s.x &&
                    p.y > camPos.y - s.y &&
                    p.y < camPos.y + s.y &&
                    rect_transform.active)
                {
                    color = _registry.get<const ColorComponent>(entity);
                    Draw(
                        glm::vec4(rect_transform.position.x + anchorTable[rect_transform.anchor].x, rect_transform.position.y + anchorTable[rect_transform.anchor].y, rect_transform.size.x, rect_transform.size.y),
                        sprite.uv,
                        sprite.texture,
                        rect_transform.depth,
                        color,
                        rect_transform.rotation,
                        rect_transform.originOffset);
                }
            }

            End();
            SpriteRenderBatch(true);
        }
    };
} // end of Canis namespace