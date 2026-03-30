#include <Canis/ECS/Systems/UIInteractionSystem.hpp>

#include <Canis/App.hpp>
#include <Canis/Entity.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Window.hpp>

#include <cfloat>

namespace Canis
{
    namespace
    {
        Vector2 GetCenteredMousePosition(Scene& _scene)
        {
            return _scene.GetInputManager().mouse - Vector2(
                _scene.GetWindow().GetScreenWidth() * 0.5f,
                _scene.GetWindow().GetScreenHeight() * 0.5f);
        }

        Vector2 GetCamera2DPosition(entt::registry &_registry, Scene& _scene)
        {
            if (_scene.HasEditorCamera2DOverride())
                return _scene.GetEditorCamera2DPosition();

            auto cameraView = _registry.view<Camera2D>();
            for (const entt::entity entityHandle : cameraView)
                return cameraView.get<Camera2D>(entityHandle).GetPosition();

            return Vector2(0.0f);
        }

        Vector2 GetMousePositionForRenderMode(entt::registry &_registry, Scene& _scene, unsigned int _renderMode)
        {
            const Vector2 centeredMouse = GetCenteredMousePosition(_scene);
            if (_renderMode == CanvasRenderMode::SCREEN_SPACE_CAMERA)
                return GetCamera2DPosition(_registry, _scene) + centeredMouse;

            return centeredMouse;
        }

        bool IsPointInsideRect(const RectTransform& _rect, const Vector2& _point)
        {
            const Vector2 min = _rect.GetRectMin() + _rect.originOffset;
            const Vector2 size = _rect.GetResolvedSize();
            return _point.x >= min.x &&
                _point.x <= (min.x + size.x) &&
                _point.y >= min.y &&
                _point.y <= (min.y + size.y);
        }

        void SetRectUniformScale(RectTransform& _rect, float _scale)
        {
            _rect.scale.x = (_rect.scale.x < 0.0f) ? -_scale : _scale;
            _rect.scale.y = (_rect.scale.y < 0.0f) ? -_scale : _scale;
        }

        void ApplyButtonVisual(Entity& _entity, UIButton& _button, RectTransform& _rect)
        {
            const bool pressed = _button.pressed;
            const bool hovered = _button.hovered;

            if (_entity.HasComponent<Sprite2D>())
            {
                Sprite2D& sprite = _entity.GetComponent<Sprite2D>();
                sprite.color = pressed ? _button.pressedColor : (hovered ? _button.hoverColor : _button.baseColor);
            }

            SetRectUniformScale(_rect, pressed ? _button.pressedScale : (hovered ? _button.hoverScale : _button.baseScale));
        }

        void ApplyDropTargetVisual(Entity& _entity, UIDropTarget& _dropTarget)
        {
            if (_entity.HasComponent<Sprite2D>())
            {
                Sprite2D& sprite = _entity.GetComponent<Sprite2D>();
                sprite.color = _dropTarget.hovered ? _dropTarget.hoverColor : _dropTarget.baseColor;
            }
        }
    }

    void UIInteractionSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        if (scene == nullptr || scene->app == nullptr || inputManager == nullptr || window == nullptr)
            return;

        auto resetDragSourceState = [](Entity* _entity) -> void
        {
            if (_entity == nullptr || !_entity->HasComponents<RectTransform, UIDragSource>())
                return;

            UIDragSource& dragSource = _entity->GetComponent<UIDragSource>();
            RectTransform& dragRect = _entity->GetComponent<RectTransform>();
            dragRect.position = dragSource.originalPosition;
            dragRect.depth = dragSource.originalDepth;
            dragSource.dragging = false;
        };

        if (m_pressedButton != nullptr &&
            (!m_pressedButton->active ||
             !m_pressedButton->HasComponents<RectTransform, UIButton>() ||
             !m_pressedButton->GetComponent<UIButton>().active ||
             !m_pressedButton->GetComponent<RectTransform>().IsActiveInHierarchy()))
        {
            m_pressedButton = nullptr;
        }

        if (m_dragSource != nullptr &&
            (!m_dragSource->active ||
             !m_dragSource->HasComponents<RectTransform, UIDragSource>() ||
             !m_dragSource->GetComponent<UIDragSource>().active ||
             !m_dragSource->GetComponent<RectTransform>().IsActiveInHierarchy()))
        {
            resetDragSourceState(m_dragSource);
            m_dragSource = nullptr;
            m_hoveredDropTarget = nullptr;
        }

        if (window->IsMouseLocked())
        {
            auto buttonView = _registry.view<RectTransform, UIButton>();
            for (auto [entityHandle, rect, button] : buttonView.each())
            {
                (void)entityHandle;
                Entity* entity = button.entity;
                if (entity == nullptr || !button.active)
                    continue;

                button.hovered = false;
                button.pressed = false;
                ApplyButtonVisual(*entity, button, rect);
            }

            auto dropView = _registry.view<RectTransform, UIDropTarget>();
            for (auto [entityHandle, rect, dropTarget] : dropView.each())
            {
                (void)entityHandle;
                (void)rect;
                Entity* entity = dropTarget.entity;
                if (entity == nullptr || !dropTarget.active)
                    continue;

                dropTarget.hovered = false;
                ApplyDropTargetVisual(*entity, dropTarget);
            }

            m_hoveredDropTarget = nullptr;
            return;
        }

        for (auto [entityHandle, rect, button] : _registry.view<RectTransform, UIButton>().each())
        {
            (void)entityHandle;
            Entity* entity = button.entity;
            if (entity == nullptr || !button.active)
                continue;

            const bool visible = rect.IsActiveInHierarchy();
            button.hovered = false;
            button.pressed = visible && (m_pressedButton == entity) && inputManager->GetLeftClick();
            ApplyButtonVisual(*entity, button, rect);
        }

        for (auto [entityHandle, rect, dropTarget] : _registry.view<RectTransform, UIDropTarget>().each())
        {
            (void)entityHandle;
            (void)rect;
            Entity* entity = dropTarget.entity;
            if (entity == nullptr || !dropTarget.active)
                continue;

            dropTarget.hovered = false;
            ApplyDropTargetVisual(*entity, dropTarget);
        }

        Entity* hoveredButton = nullptr;
        Entity* hoveredDragSource = nullptr;
        Entity* hoveredDropTarget = nullptr;
        float hoveredButtonDepth = FLT_MAX;
        float hoveredDragDepth = FLT_MAX;
        float hoveredDropDepth = FLT_MAX;

        auto evaluateRectEntity = [&](Entity* _entity, RectTransform& _rect, float& _bestDepth, Entity*& _bestEntity) -> void
        {
            if (_entity == nullptr || !_rect.IsActiveInHierarchy())
                return;

            const unsigned int renderMode = _rect.GetCanvasRenderMode();
            if (renderMode == CanvasRenderMode::WORLD_SPACE)
                return;

            const Vector2 pointerPosition = GetMousePositionForRenderMode(_registry, *scene, renderMode);
            if (!IsPointInsideRect(_rect, pointerPosition))
                return;

            const float depth = _rect.GetDepth();
            if (_bestEntity == nullptr || depth < _bestDepth)
            {
                _bestDepth = depth;
                _bestEntity = _entity;
            }
        };

        if (m_dragSource == nullptr)
        {
            auto buttonView = _registry.view<RectTransform, UIButton>();
            for (auto [entityHandle, rect, button] : buttonView.each())
            {
                (void)entityHandle;
                if (button.active)
                    evaluateRectEntity(button.entity, rect, hoveredButtonDepth, hoveredButton);
            }

            auto dragView = _registry.view<RectTransform, UIDragSource>();
            for (auto [entityHandle, rect, dragSource] : dragView.each())
            {
                (void)entityHandle;
                if (dragSource.active)
                    evaluateRectEntity(dragSource.entity, rect, hoveredDragDepth, hoveredDragSource);
            }
        }
        else
        {
            auto& dragRect = m_dragSource->GetComponent<RectTransform>();
            auto& dragSource = m_dragSource->GetComponent<UIDragSource>();

            if (dragSource.dragging && inputManager->GetLeftClick())
                dragRect.SetPosition(GetCenteredMousePosition(*scene) - dragSource.dragOffset);

            auto dropView = _registry.view<RectTransform, UIDropTarget>();
            for (auto [entityHandle, rect, dropTarget] : dropView.each())
            {
                (void)entityHandle;
                if (!dropTarget.active || dropTarget.entity == nullptr || dropTarget.entity == m_dragSource)
                    continue;

                if (!dropTarget.acceptedPayloadType.empty() && dropTarget.acceptedPayloadType != dragSource.payloadType)
                    continue;

                evaluateRectEntity(dropTarget.entity, rect, hoveredDropDepth, hoveredDropTarget);
            }
        }

        if (hoveredButton != nullptr && hoveredButton->HasComponent<UIButton>())
        {
            UIButton& button = hoveredButton->GetComponent<UIButton>();
            RectTransform& rect = hoveredButton->GetComponent<RectTransform>();
            button.hovered = true;
            ApplyButtonVisual(*hoveredButton, button, rect);
        }

        if (hoveredDropTarget != nullptr && hoveredDropTarget->HasComponent<UIDropTarget>())
        {
            UIDropTarget& dropTarget = hoveredDropTarget->GetComponent<UIDropTarget>();
            dropTarget.hovered = true;
            ApplyDropTargetVisual(*hoveredDropTarget, dropTarget);
        }

        if (m_dragSource == nullptr)
        {
            if (inputManager->JustLeftClicked())
            {
                if (hoveredDragSource != nullptr && hoveredDragSource->HasComponents<RectTransform, UIDragSource>())
                {
                    UIDragSource& dragSource = hoveredDragSource->GetComponent<UIDragSource>();
                    RectTransform& dragRect = hoveredDragSource->GetComponent<RectTransform>();
                    dragSource.dragging = true;
                    dragSource.originalPosition = dragRect.position;
                    dragSource.originalDepth = dragRect.depth;
                    dragSource.dragOffset = GetCenteredMousePosition(*scene) - dragRect.GetPosition();
                    m_dragSource = hoveredDragSource;
                }
                else if (hoveredButton != nullptr)
                {
                    m_pressedButton = hoveredButton;
                }
            }
            else if (m_pressedButton != nullptr && inputManager->LeftClickReleased())
            {
                if (m_pressedButton == hoveredButton && m_pressedButton->HasComponent<UIButton>())
                {
                    UIButton& button = m_pressedButton->GetComponent<UIButton>();
                    Entity* receiver = button.targetEntity != nullptr ? button.targetEntity : m_pressedButton;
                    UIActionContext context = {};
                    context.sourceEntity = m_pressedButton;
                    context.targetEntity = receiver;
                    context.pointerPosition = GetCenteredMousePosition(*scene);
                    scene->app->DispatchUIAction(*receiver, button.targetScript, button.actionName, context);
                }

                m_pressedButton = nullptr;
            }
        }
        else if (m_dragSource->HasComponents<RectTransform, UIDragSource>() && inputManager->LeftClickReleased())
        {
            UIDragSource& dragSource = m_dragSource->GetComponent<UIDragSource>();
            RectTransform& dragRect = m_dragSource->GetComponent<RectTransform>();

            if (hoveredDropTarget != nullptr && hoveredDropTarget->HasComponent<UIDropTarget>())
            {
                UIDropTarget& dropTarget = hoveredDropTarget->GetComponent<UIDropTarget>();
                Entity* receiver = dropTarget.targetEntity != nullptr ? dropTarget.targetEntity : hoveredDropTarget;

                UIActionContext context = {};
                context.sourceEntity = m_dragSource;
                context.targetEntity = receiver;
                context.pointerPosition = GetCenteredMousePosition(*scene);
                context.payloadType = dragSource.payloadType;
                context.payloadValue = dragSource.payloadValue;
                scene->app->DispatchUIAction(*receiver, dropTarget.targetScript, dropTarget.actionName, context);
            }

            dragRect.position = dragSource.originalPosition;
            dragRect.depth = dragSource.originalDepth;
            dragSource.dragging = false;
            m_dragSource = nullptr;
            m_hoveredDropTarget = nullptr;
        }
        else
        {
            m_hoveredDropTarget = hoveredDropTarget;
        }
    }
}
