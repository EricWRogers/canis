#pragma once
#include <Canis/Data/Glyph.hpp>

#include <Canis/ECS/Systems/System.hpp>

namespace Canis
{
    class SpriteRenderer2DSystem;
    enum class GlyphSortType;
    class RenderHUDSystem : public System
    {
    private:
        SpriteRenderer2DSystem m_spriteRenderer;
        GlyphSortType m_glyphSortType = GlyphSortType::FRONT_TO_BACK;

    public:
        RenderHUDSystem() : System() {}

        void SetSort(GlyphSortType _sortType);

        void Create();
        void Ready() {}
        void Update(entt::registry &_registry, float _deltaTime);
    };

    extern bool DecodeRenderHUDSystem(const std::string &_name, Canis::Scene *_scene);
} // end of Canis namespace