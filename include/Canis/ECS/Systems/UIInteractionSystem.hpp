#pragma once

#include <Canis/System.hpp>

namespace Canis
{
    class UIInteractionSystem : public System
    {
    private:
        Entity* m_pressedButton = nullptr;
        Entity* m_dragSource = nullptr;
        Entity* m_hoveredDropTarget = nullptr;

    public:
        UIInteractionSystem() : System() { m_name = type_name<UIInteractionSystem>(); }

        void Create() override {}
        void Ready() override {}
        void Update(entt::registry &_registry, float _deltaTime) override;
        bool UpdateWhenPaused() const override { return true; }
    };
}
