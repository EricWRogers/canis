#pragma once

#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Glyph;
    class Camera2D;

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

        ~SpriteRenderer2DSystem();

        void SortGlyphs();

        void CreateRenderBatches();

        void Begin(GlyphSortType sortType);

        void End();

        inline void RotatePoint(glm::vec2 &point, const float &cAngle, const float &sAngle);

        void Draw(const glm::vec4 &destRect, const glm::vec4 &uvRect, const GLTexture &texture, float depth, const ColorComponent &color);

        void Draw(const glm::vec4 &destRect, const glm::vec4 &uvRect, const GLTexture &texture, const float &depth, const ColorComponent &color, const float &angle, const glm::vec2 &origin);

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

    bool DecodeSpriteRenderer2DSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::SpriteRenderer2DSystem")
        {
            _scene->CreateRenderSystem<SpriteRenderer2DSystem>();
            return true;
        }
        return false;
    }
} // end of Canis namespace