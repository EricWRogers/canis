#pragma once
#include <glm/glm.hpp>
#include <vector>

#include <Canis/Camera.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Data/Glyph.hpp>
#include <Canis/External/entt.hpp>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>

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

    class SpriteRenderer2DSystem
    {
    private:
        GlyphSortType glyphSortType = GlyphSortType::FRONT_TO_BACK;
        std::vector<Glyph *> glyphs;
        std::vector<RenderBatch> spriteRenderBatch;
        Shader *spriteShader;

        unsigned int vbo = 0;
        unsigned int vao = 0;

        static bool CompareFrontToBack(Glyph *a, Glyph *b) { return (a->depth < b->depth); }
        static bool CompareBackToFront(Glyph *a, Glyph *b) { return (a->depth > b->depth); }
        static bool CompareTexture(Glyph *a, Glyph *b) { return (a->textureId < b->textureId); }

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
            std::vector<SpriteVertex> vertices;
            vertices.resize(glyphs.size() * 6);

            if (glyphs.empty())
                return;

            int offset = 0;
            int cv = 0; // current vertex
            spriteRenderBatch.emplace_back(offset, 6, glyphs[0]->textureId);

            
            vertices[cv++] = glyphs[0]->topLeft;
            vertices[cv++] = glyphs[0]->bottomLeft;
            vertices[cv++] = glyphs[0]->bottomRight;
            vertices[cv++] = glyphs[0]->bottomRight;
            vertices[cv++] = glyphs[0]->topRight;
            vertices[cv++] = glyphs[0]->topLeft;          
            
            

            offset += 6;

            for (int cg = 1; cg < glyphs.size(); cg++)
            {
                if (glyphs[cg]->textureId != glyphs[cg - 1]->textureId)
                {
                    spriteRenderBatch.emplace_back(offset, 6, glyphs[cg]->textureId);
                }
                else
                {
                    spriteRenderBatch.back().numVertices += 6;
                }
                
                vertices[cv++] = glyphs[cg]->topLeft;
                vertices[cv++] = glyphs[cg]->bottomLeft;
                vertices[cv++] = glyphs[cg]->bottomRight;
                vertices[cv++] = glyphs[cg]->bottomRight;
                vertices[cv++] = glyphs[cg]->topRight;
                vertices[cv++] = glyphs[cg]->topLeft;
                offset += 6;
            }

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SpriteVertex), nullptr, GL_DYNAMIC_DRAW);
            glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(SpriteVertex), vertices.data());

            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        void Begin(GlyphSortType sortType) {
            glyphSortType = sortType;
            spriteRenderBatch.clear();

            for (int i = 0; i < glyphs.size(); i++)
                delete glyphs[i];

            glyphs.clear();
        }

        void End() {
            SortGlyphs();
            CreateRenderBatches();
        }

        void Draw(const glm::vec4 &destRect, const glm::vec4 &uvRect, const GLTexture &texture, float depth, const ColorComponent &color) {
            Glyph *newGlyph = new Glyph;

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

            glyphs.push_back(newGlyph);
        }

        void SpriteRenderBatch()
        {
            spriteShader->Use();
            glBindVertexArray(vao);
            glm::mat4 projection = glm::mat4(1.0f);
            projection = glm::ortho(0.0f, static_cast<float>(window->GetScreenWidth()), 0.0f, static_cast<float>(window->GetScreenHeight()));
            spriteShader->SetMat4("P", projection);

            for (int i = 0; i < spriteRenderBatch.size(); i++)
            {
                glBindTexture(GL_TEXTURE_2D, spriteRenderBatch[i].texture);

                glDrawArrays(GL_TRIANGLES, spriteRenderBatch[i].offset, spriteRenderBatch[i].numVertices);
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

            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*)0);
            //color
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void*)(3 * sizeof(float)));
            //uv
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), (void *)(7 * sizeof(float)));

            glBindVertexArray(0);
        }

    public:
        Canis::Window *window;

        SpriteRenderer2DSystem() {}

        void Init(GlyphSortType sortType,Shader *shader)
        {
            glyphSortType = sortType;
            spriteShader = shader;
            CreateVertexArray();
        }

        void UpdateComponents(float deltaTime, entt::registry &registry)
        {
            Begin(glyphSortType);

            // Draw
            auto view = registry.view<RectTransformComponent, ColorComponent, Sprite2DComponent>();
            for (auto [entity, rect_transform, color, sprite] : view.each())
			{
                Draw(
                    glm::vec4(rect_transform.position.x,rect_transform.position.y,rect_transform.size.x,rect_transform.size.y),
                    sprite.uv,
                    sprite.texture,
                    rect_transform.depth,
                    color
                );
            }

            End();
            SpriteRenderBatch();
        }
    };
} // end of Canis namespace