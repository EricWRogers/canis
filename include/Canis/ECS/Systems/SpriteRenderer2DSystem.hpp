#pragma once
#include <Canis/Scene.hpp>
#include <Canis/Camera2D.hpp>
#include <Canis/Data/Glyph.hpp>
#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class Glyph;
    struct Color;

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

        SpriteRenderer2DSystem() : System() { m_name = type_name<SpriteRenderer2DSystem>(); }

        ~SpriteRenderer2DSystem();

        void SortGlyphs();

        void CreateRenderBatches();

        void Begin(GlyphSortType sortType);

        void End();

        void DrawUI(const glm::vec4 &destRect, const glm::vec4 &uvRect, const GLTexture &texture, float depth, const Color &color, const float &angle, const glm::vec2 &origin, const glm::vec2 &rotationOriginOffset);

        void Draw(const glm::vec4 &destRect, const glm::vec4 &uvRect, const GLTexture &texture, const float &depth, const Color &color, const float &angle, const glm::vec2 &origin);

        void SpriteRenderBatch(bool use2DCamera);

        void CreateVertexArray();

        void SetSort(GlyphSortType _sortType);

        void Create();

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);
    private:
        float m_time = 0.0f;
    };
} // end of Canis namespace