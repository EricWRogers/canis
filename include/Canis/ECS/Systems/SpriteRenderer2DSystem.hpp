#pragma once
#include <Canis/Scene.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Data/GLTexture.hpp>
#include <Canis/Data/Glyph.hpp>
#include <Canis/System.hpp>

namespace Canis
{
    class Shader;
    class Camera2D;
    class Entity;
    class RectTransform;
    class Text;

    class RenderBatch
    {
    public:
        RenderBatch(unsigned int Offset, unsigned int NumVertices, unsigned int Texture, bool SdfText = false) : offset(Offset),
                                                                         numVertices(NumVertices), texture(Texture), sdfText(SdfText) {}
        unsigned int offset;
        unsigned int numVertices;
        unsigned int texture;
        bool sdfText;
    };

    class SpriteRenderer2DSystem : public System
    {
    public:
        GlyphSortType glyphSortType = GlyphSortType::BACK_TO_FRONT;
        std::vector<Glyph *> glyphs;
        std::vector<SpriteVertex> vertices = {};
        std::vector<unsigned int> indices = {};
        std::vector<RenderBatch> spriteRenderBatch;
        Shader *spriteShader;
        Camera2D *camera2D;

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

        void DrawUI(const Vector4 &destRect, const Vector4 &uvRect, const GLTexture &texture, float depth, const Color &color, const float &angle, const Vector2 &origin, const Vector2 &rotationOriginOffset, bool sdfText = false);

        void Draw(const Vector4 &destRect, const Vector4 &uvRect, const GLTexture &texture, const float &depth, const Color &color, const float &angle, const Vector2 &origin);

        void SpriteRenderBatch(bool use2DCamera, const Matrix4* overrideProjection = nullptr);

        void CreateVertexArray();

        void SetSort(GlyphSortType _sortType);

        void Create() override;

        void Ready() override;

        void Update(entt::registry &_registry, float _deltaTime) override;
        void RenderWorldUI(entt::registry &_registry, float _deltaTime);
        void RenderScreenSpaceUI(entt::registry &_registry);
    private:
        void RenderLayers(entt::registry &_registry, float _deltaTime, bool _worldOnly, bool _screenOnly);
        void DrawText(
            Entity* _entity,
            RectTransform* _transform,
            Text* _text,
            const Vector2& _position,
            const Vector2& _size,
            const Vector2& _rectMin,
            const Vector2& _scale,
            float _depth,
            float _rotation);
        float m_time = 0.0f;
    };
} // end of Canis namespace
