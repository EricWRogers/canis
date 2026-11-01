#pragma once
#include <Canis/Entity.hpp>
#include <Canis/Math.hpp>

#include <Canis/System.hpp>
#include <vector>
#include <span>

namespace Canis
{
    struct UIInputField;
    class UIInteractionSystem : public System
    {
    private:
        std::vector<Entity> m_tabOrder;
        Entity m_selectedButton = nullptr;
        Entity m_navigationPressedButton = nullptr;
        bool m_navigationFocus = false;
        Entity m_navigationCanvas = nullptr;
        Vector2 m_navigationDirection = Vector2(0.0f);
        float m_navigationRepeat = 0.0f;
        Entity m_pressedButton = nullptr;
        Entity m_focusedInputField = nullptr;
        bool m_textInputActive = false;
        Entity m_dragSource = nullptr;
        Entity m_hoveredDropTarget = nullptr;

    public:
        UIInteractionSystem() : System() { m_name = type_name<UIInteractionSystem>(); }

        void SetTabOrder(const std::vector<Entity>& buttons) { m_tabOrder = buttons; }
        Entity SelectedButton() const { return m_selectedButton; }
        void SelectButton(Entity entity, bool showFocus = true) { m_selectedButton = entity; m_navigationFocus = showFocus; m_navigationPressedButton = nullptr; }
        // Geometry queries use the scene's current UI pointer and canvas mode.
        // Disabled buttons can still be queried for tooltips or world-input blocking.
        static bool HitTest(Entity widget);
        static Entity Pick(std::span<const Entity> widgets);
        static void SetButtonActive(Entity entity, bool active);
        bool FocusInputField(Entity* entity);
        static void SetInputText(UIInputField& field, const std::string& text);
        static void InsertInputText(UIInputField& field, const std::string& text);
        static void BackspaceInputText(UIInputField& field);
        void OnDestroy() override { FocusInputField(nullptr); }
        void Create() override {}
        void Ready() override {}
        void Update(entt::registry &_registry, float _deltaTime) override;
        bool UpdateWhenPaused() const override { return true; }
    };
}
