#pragma once
#include <Canis/Data/Glyph.hpp>
#include <Canis/ECS/Systems/System.hpp>
#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>
#include <Canis/ECS/Systems/RenderTextSystem.hpp>

namespace Canis
{
    struct HUDElementDepth
    {
        entt::entity element;
        float depth = 0.0f;
        float depthOffset = 0.0f;
        bool isText = false;
    };

    class RenderHUDSystem : public System
    {
    private:
        SpriteRenderer2DSystem m_spriteRenderer;
        RenderTextSystem m_textRenderer;
        GlyphSortType m_glyphSortType = GlyphSortType::FRONT_TO_BACK;
        std::vector<HUDElementDepth> elements = {};

    public:
        RenderHUDSystem() : System() {}

        void SetSort(GlyphSortType _sortType);

        void Create();
        void Ready() {}
        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeRenderHUDSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace