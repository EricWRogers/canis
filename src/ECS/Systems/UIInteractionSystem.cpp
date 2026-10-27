#include <Canis/ECS/Systems/UIInteractionSystem.hpp>

#include <Canis/App.hpp>
#include <Canis/ConfigData.hpp>
#include <Canis/Components.hpp>
#include <Canis/InputManager.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Window.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_keyboard.h>

#include <cfloat>
#include <cmath>

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

        bool IsPointInsideRect(const RectLayout& _rect, const Vector2& _point)
        {
            const auto matrix = _rect.GetCanvasLocalMatrix();
            if (const float determinant = glm::determinant(matrix); !std::isfinite(determinant) || determinant == 0) return false;
            const Vector2 local = Vector2(glm::inverse(matrix) * Vector4(_point, _rect.GetDepth(), 1));
            const Vector2 min = -_rect.pivot * _rect.GetLogicalSize() + _rect.originOffset;
            const Vector2 size = _rect.GetLogicalSize();
            return local.x >= min.x && local.x <= min.x + size.x && local.y >= min.y && local.y <= min.y + size.y;
        }

        bool TryGetWorldCanvasPointer(
            entt::registry &_registry,
            Scene &_scene,
            const RectLayout &_rect,
            Vector2 &_pointerPosition)
        {
            const Canvas *canvas = _rect.GetCanvas();
            if (canvas == nullptr ||
                !canvas->active ||
                !canvas->receivesEvents ||
                canvas->entity == nullptr ||
                !canvas->entity->HasComponent<Transform>())
            {
                return false;
            }

            Ray ray = {};
            const auto& pointer = _scene.GetInputManager().worldUIPointer;
            if (pointer.enabled) {
                if (!pointer.active) return false;
                ray.origin = pointer.origin;
                ray.direction = pointer.direction;
            } else {
            Entity *cameraEntity = nullptr;
            auto cameraView = _registry.view<Camera, Transform>();
            for (const entt::entity cameraHandle : cameraView)
            {
                Camera &camera = cameraView.get<Camera>(cameraHandle);
                Transform &cameraTransform = cameraView.get<Transform>(cameraHandle);
                Entity *candidate = camera.entity != nullptr
                    ? camera.entity
                    : cameraTransform.entity;
                if (candidate == nullptr || !cameraTransform.IsActiveInHierarchy())
                    continue;
                if (camera.primary)
                {
                    cameraEntity = candidate;
                    break;
                }
                if (cameraEntity == nullptr)
                    cameraEntity = candidate;
            }

            if (cameraEntity == nullptr)
                return false;

            const Vector2 rayPosition = _scene.GetWindow().IsMouseLocked()
                ? Vector2(
                    _scene.GetWindow().GetScreenWidth() * 0.5f,
                    _scene.GetWindow().GetScreenHeight() * 0.5f)
                : _scene.GetInputManager().mouse;
            if (!_scene.TryGetRayFromCamera(*cameraEntity, rayPosition, ray))
                return false;

            }
            const Matrix4 model =
                _rect.Spatial().GetModelMatrix();
            const Vector3 planePoint = Vector3(model * Vector4(0.0f, 0.0f, 0.0f, 1.0f));
            const Vector3 planeNormal = glm::normalize(
                Vector3(model * Vector4(0.0f, 0.0f, 1.0f, 0.0f)));
            const float denominator = glm::dot(ray.direction, planeNormal);
            if (std::abs(denominator) <= 0.000001f)
                return false;

            const float distance =
                glm::dot(planePoint - ray.origin, planeNormal) / denominator;
            if (distance < 0.0f ||
                (canvas->interactionDistance > 0.0f &&
                 distance > canvas->interactionDistance))
            {
                return false;
            }

            const Vector3 hitPoint = ray.origin + ray.direction * distance;
            const Vector4 localPoint = glm::inverse(model) * Vector4(hitPoint, 1.0f);
            _pointerPosition = Vector2(_rect.GetCanvasLocalMatrix() * localPoint);
            return true;
        }

        void SetRectUniformScale(RectLayout& _rect, float _scale)
        {
            _rect.Spatial().scale.x = (_rect.Spatial().scale.x < 0.0f) ? -_scale : _scale;
            _rect.Spatial().scale.y = (_rect.Spatial().scale.y < 0.0f) ? -_scale : _scale;
        }

        void ApplyButtonVisual(Entity& _entity, UIButton& _button, RectLayout& _rect)
        {
            const bool pressed = _button.pressed;
            const bool hovered = _button.hovered;

            if (_entity.HasComponent<Sprite2D>())
            {
                Sprite2D& sprite = _entity.GetComponent<Sprite2D>();
                sprite.color = pressed ? _button.pressedColor : (hovered ? _button.hoverColor : _button.baseColor);
            }
            else if (_entity.HasComponent<Text>())
            {
                Text& text = _entity.GetComponent<Text>();
                text.color = pressed ? _button.pressedColor : (hovered ? _button.hoverColor : _button.baseColor);
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

        Entity* GetInputFieldDisplayEntity(UIInputField& _inputField)
        {
            if (_inputField.displayEntity != nullptr)
                return _inputField.displayEntity;
            return _inputField.entity;
        }

        void SyncInputFieldFromBinding(Scene& _scene, UIInputField& _inputField)
        {
            if (_scene.app == nullptr || _inputField.targetScript.empty() || _inputField.targetProperty.empty())
                return;

            Entity* targetEntity = (_inputField.targetEntity != nullptr) ? _inputField.targetEntity.TryGet() : _inputField.entity;
            if (targetEntity == nullptr)
                return;

            ScriptConf* scriptConf = _scene.app->GetScriptConf(_inputField.targetScript);
            if (scriptConf == nullptr || scriptConf->Get == nullptr)
                return;

            void* componentPtr = scriptConf->Get(*targetEntity);
            if (componentPtr == nullptr)
                return;

            auto getterIt = scriptConf->registry.getters.find(_inputField.targetProperty);
            if (getterIt == scriptConf->registry.getters.end())
                return;

            YAML::Node node = getterIt->second(componentPtr);
            if (node && node.IsScalar())
                _inputField.text = node.Scalar();
        }

        void PushInputFieldToBinding(Scene& _scene, UIInputField& _inputField)
        {
            if (_scene.app == nullptr || _inputField.targetScript.empty() || _inputField.targetProperty.empty())
                return;

            Entity* targetEntity = (_inputField.targetEntity != nullptr) ? _inputField.targetEntity.TryGet() : _inputField.entity;
            if (targetEntity == nullptr)
                return;

            ScriptConf* scriptConf = _scene.app->GetScriptConf(_inputField.targetScript);
            if (scriptConf == nullptr || scriptConf->Get == nullptr)
                return;

            void* componentPtr = scriptConf->Get(*targetEntity);
            if (componentPtr == nullptr)
                return;

            auto setterIt = scriptConf->registry.setters.find(_inputField.targetProperty);
            if (setterIt == scriptConf->registry.setters.end())
                return;

            YAML::Node valueNode(_inputField.text);
            setterIt->second(valueNode, componentPtr);
        }

        void RefreshInputFieldDisplay(UIInputField& _inputField)
        {
            Entity* displayEntity = GetInputFieldDisplayEntity(_inputField);
            if (displayEntity == nullptr || !displayEntity->HasComponent<Text>())
                return;

            Text& displayText = displayEntity->GetComponent<Text>();
            std::string displayValue = _inputField.text;
            if (displayValue.empty() && !_inputField.focused)
            {
                displayValue = _inputField.placeholder;
                displayText.color = _inputField.placeholderColor;
            }
            else
            {
                displayText.color = _inputField.textColor;
                if (_inputField.focused && _inputField.caretVisible)
                    displayValue += "|";
            }

            displayText.SetText(displayValue);
        }

        void ApplyInputFieldVisual(Entity& _entity, UIInputField& _inputField)
        {
            if (_entity.HasComponent<Sprite2D>())
            {
                Sprite2D& sprite = _entity.GetComponent<Sprite2D>();
                sprite.color = _inputField.focused ? _inputField.focusedColor : (_inputField.hovered ? _inputField.hoverColor : _inputField.baseColor);
            }

            RefreshInputFieldDisplay(_inputField);
        }
    }

    bool UIInteractionSystem::FocusInputField(Entity* entity)
    {
        if (window == nullptr)
            return false;
        if (entity != nullptr)
        {
            if (!entity->IsValid() || !entity->Active() || !entity->HasComponents<RectLayout, UIInputField>() ||
                !entity->GetComponent<UIInputField>().active || !entity->GetComponent<RectLayout>().IsActiveInHierarchy() ||
                window->IsMouseLocked() || (inputManager && !inputManager->active))
                return false;
            const auto* canvas = entity->GetComponent<RectLayout>().GetCanvas();
            if (canvas && (!canvas->active || !canvas->receivesEvents))
                return false;
        }
        if (m_focusedInputField == entity && (entity != nullptr || !m_textInputActive))
            return entity != nullptr;
        if (m_focusedInputField && m_focusedInputField->HasComponent<UIInputField>())
        {
            auto& previous = m_focusedInputField->GetComponent<UIInputField>();
            previous.focused = false;
            previous.caretVisible = true;
            previous.caretBlinkTimer = 0;
            RefreshInputFieldDisplay(previous);
        }
        m_focusedInputField = nullptr;
        if (m_textInputActive)
            SDL_StopTextInput(static_cast<SDL_Window*>(window->GetSDLWindow()));
        m_textInputActive = false;
        if (entity == nullptr)
            return false;
        auto& field = entity->GetComponent<UIInputField>();
        if (!SDL_StartTextInput(static_cast<SDL_Window*>(window->GetSDLWindow())))
            return false;
        m_focusedInputField = entity;
        m_textInputActive = true;
        field.focused = true;
        field.caretVisible = true;
        field.caretBlinkTimer = 0;
        m_navigationPressedButton = nullptr;
        RefreshInputFieldDisplay(field);
        return true;
    }

    void UIInteractionSystem::InsertInputText(UIInputField& field, const std::string& text)
    {
        const char* cursor = text.c_str();
        size_t remaining = text.size();
        size_t length = SDL_utf8strlen(field.text.c_str());
        while (remaining > 0)
        {
            const char* start = cursor;
            const Uint32 codepoint = SDL_StepUTF8(&cursor, &remaining);
            if (codepoint == SDL_INVALID_UNICODE_CODEPOINT || codepoint < 32 || codepoint == 127)
                continue;
            const std::string character(start, cursor - start);
            if (!field.allowedCharacters.empty() && field.allowedCharacters.find(character) == std::string::npos)
                continue;
            if (field.maxLength > 0 && length >= static_cast<size_t>(field.maxLength))
                break;
            field.text += character;
            ++length;
        }
        field.caretVisible = true;
        field.caretBlinkTimer = 0;
        if (field.entity && field.entity->IsValid())
            PushInputFieldToBinding(field.entity->scene, field);
        RefreshInputFieldDisplay(field);
    }

    void UIInteractionSystem::SetInputText(UIInputField& field, const std::string& text)
    {
        // Copy first so callers can pass field.text itself.
        const std::string value = text;
        field.text.clear();
        InsertInputText(field, value);
    }

    void UIInteractionSystem::BackspaceInputText(UIInputField& field)
    {
        if (field.text.empty())
            return;
        size_t start = field.text.size() - 1;
        while (start > 0 && (static_cast<unsigned char>(field.text[start]) & 0xc0) == 0x80)
            --start;
        field.text.erase(start);
        if (field.entity && field.entity->IsValid())
            PushInputFieldToBinding(field.entity->scene, field);
        RefreshInputFieldDisplay(field);
    }

    void UIInteractionSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        if (scene == nullptr || scene->app == nullptr || inputManager == nullptr || window == nullptr)
            return;

        const auto& pointer = inputManager->worldUIPointer;
        const bool pointerPressed = pointer.enabled ? pointer.pressed : inputManager->JustLeftClicked();
        const bool pointerDown = pointer.enabled ? pointer.down : inputManager->GetLeftClick();
        const bool pointerReleased = pointer.enabled ? pointer.released : inputManager->LeftClickReleased();
        if (pointer.enabled && !pointer.active) m_pressedButton = nullptr;
        const bool wasEditingText = m_textInputActive;
        if (m_textInputActive && (!m_focusedInputField || !inputManager->active))
            FocusInputField(nullptr);
        if ((!pointer.enabled && inputManager->mouseRel != Vector2(0.0f)) || pointerPressed)
        {
            m_navigationFocus = false;
            m_navigationPressedButton = nullptr;
        }

        auto resetDragSourceState = [](Entity* _entity) -> void
        {
            if (_entity == nullptr || !_entity->HasComponents<RectLayout, UIDragSource>())
                return;

            UIDragSource& dragSource = _entity->GetComponent<UIDragSource>();
            RectLayout& dragRect = _entity->GetComponent<RectLayout>();
            dragRect.SetLocalPosition(dragSource.originalPosition);
            dragRect.Spatial().position.z = dragSource.originalDepth;
            dragSource.dragging = false;
        };

        if (m_pressedButton != nullptr &&
            (!m_pressedButton->Active() ||
             !m_pressedButton->HasComponents<RectLayout, UIButton>() ||
             !m_pressedButton->GetComponent<UIButton>().active ||
             !m_pressedButton->GetComponent<RectLayout>().IsActiveInHierarchy()))
        {
            m_pressedButton = nullptr;
        }

        if (m_focusedInputField != nullptr &&
            (!m_focusedInputField->Active() ||
             !m_focusedInputField->HasComponents<RectLayout, UIInputField>() ||
             !m_focusedInputField->GetComponent<UIInputField>().active ||
             !m_focusedInputField->GetComponent<RectLayout>().IsActiveInHierarchy()))
        {
            FocusInputField(nullptr);
        }

        if (m_dragSource != nullptr &&
            (!m_dragSource->Active() ||
             !m_dragSource->HasComponents<RectLayout, UIDragSource>() ||
             !m_dragSource->GetComponent<UIDragSource>().active ||
             !m_dragSource->GetComponent<RectLayout>().IsActiveInHierarchy()))
        {
            resetDragSourceState(m_dragSource);
            m_dragSource = nullptr;
            m_hoveredDropTarget = nullptr;
        }

        if (window->IsMouseLocked())
        {
            auto buttonView = _registry.view<RectLayout, UIButton>();
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

            auto inputFieldView = _registry.view<RectLayout, UIInputField>();
            for (auto [entityHandle, rect, inputField] : inputFieldView.each())
            {
                (void)entityHandle;
                (void)rect;
                Entity* entity = inputField.entity;
                if (entity == nullptr || !inputField.active)
                    continue;

                inputField.hovered = false;
                ApplyInputFieldVisual(*entity, inputField);
            }

            auto dropView = _registry.view<RectLayout, UIDropTarget>();
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

            FocusInputField(nullptr);
            m_hoveredDropTarget = nullptr;
        }

        for (auto [entityHandle, rect, button] : _registry.view<RectLayout, UIButton>().each())
        {
            (void)entityHandle;
            Entity* entity = button.entity;
            if (entity == nullptr)
                continue;

            if (button.baseValuesSaved == false)
            {
                button.baseValuesSaved = true;
                button.baseScale = rect.Spatial().scale.x;

                if (entity->HasComponent<Sprite2D>())
                    button.baseColor = entity->GetComponent<Sprite2D>().color;
                else if (entity->HasComponent<Text>())
                    button.baseColor = entity->GetComponent<Text>().color;
            }

            const bool visible = button.active && rect.IsActiveInHierarchy();
            button.hovered = false;
            button.pressed = visible && (m_pressedButton == entity) && pointerDown;
            ApplyButtonVisual(*entity, button, rect);
        }

        for (auto [entityHandle, rect, inputField] : _registry.view<RectLayout, UIInputField>().each())
        {
            (void)entityHandle;
            (void)rect;
            Entity* entity = inputField.entity;
            if (entity == nullptr || !inputField.active)
                continue;

            if (inputField.baseValuesSaved == false)
            {
                inputField.baseValuesSaved = true;
                if (entity->HasComponent<Sprite2D>())
                    inputField.baseColor = entity->GetComponent<Sprite2D>().color;
                if (Entity* displayEntity = GetInputFieldDisplayEntity(inputField); displayEntity != nullptr && displayEntity->HasComponent<Text>())
                    inputField.baseTextColor = displayEntity->GetComponent<Text>().color;
            }

            if (!inputField.focused)
                SyncInputFieldFromBinding(*scene, inputField);

            inputField.hovered = false;
            inputField.caretBlinkTimer += _deltaTime;
            if (inputField.caretBlinkTimer >= 0.5f)
            {
                inputField.caretBlinkTimer = 0.0f;
                inputField.caretVisible = !inputField.caretVisible;
            }

            ApplyInputFieldVisual(*entity, inputField);
        }

        for (auto [entityHandle, rect, dropTarget] : _registry.view<RectLayout, UIDropTarget>().each())
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
        Entity* hoveredInputField = nullptr;
        Entity* hoveredDragSource = nullptr;
        Entity* hoveredDropTarget = nullptr;
        float hoveredButtonDepth = FLT_MAX;
        float hoveredInputFieldDepth = FLT_MAX;
        float hoveredDragDepth = FLT_MAX;
        float hoveredDropDepth = FLT_MAX;

        auto evaluateRectEntity = [&](Entity* _entity, RectLayout& _rect, float& _bestDepth, Entity*& _bestEntity) -> void
        {
            if (_entity == nullptr || !_rect.IsActiveInHierarchy())
                return;

            const unsigned int renderMode = _rect.GetCanvasRenderMode();
            if (window->IsMouseLocked() &&
                renderMode != CanvasRenderMode::WORLD_SPACE)
            {
                return;
            }
            if (renderMode == CanvasRenderMode::WORLD_SPACE)
            {
                Vector2 pointerPosition = Vector2(0.0f);
                if (!TryGetWorldCanvasPointer(_registry, *scene, _rect, pointerPosition) ||
                    !IsPointInsideRect(_rect, pointerPosition))
                {
                    return;
                }
            }
            else
            {
                const Vector2 pointerPosition =
                    GetMousePositionForRenderMode(_registry, *scene, renderMode);
                if (!IsPointInsideRect(_rect, pointerPosition))
                    return;
            }

            const float depth = _rect.GetDepth();
            if (_bestEntity == nullptr || depth < _bestDepth)
            {
                _bestDepth = depth;
                _bestEntity = _entity;
            }
        };

        if (m_dragSource == nullptr)
        {
            auto buttonView = _registry.view<RectLayout, UIButton>();
            for (auto [entityHandle, rect, button] : buttonView.each())
            {
                (void)entityHandle;
                if (!button.active || !rect.IsActiveInHierarchy()) continue;
                auto* hitEntity = button.hitRect.TryGet();
                auto& hitRect = hitEntity && hitEntity->HasComponent<RectLayout>()
                    ? hitEntity->GetComponent<RectLayout>() : rect;
                evaluateRectEntity(button.entity, hitRect, hoveredButtonDepth, hoveredButton);
            }

            auto inputFieldView = _registry.view<RectLayout, UIInputField>();
            for (auto [entityHandle, rect, inputField] : inputFieldView.each())
            {
                (void)entityHandle;
                if (inputField.active)
                    evaluateRectEntity(inputField.entity, rect, hoveredInputFieldDepth, hoveredInputField);
            }

            auto dragView = _registry.view<RectLayout, UIDragSource>();
            for (auto [entityHandle, rect, dragSource] : dragView.each())
            {
                (void)entityHandle;
                if (dragSource.active)
                    evaluateRectEntity(dragSource.entity, rect, hoveredDragDepth, hoveredDragSource);
            }
        }
        else
        {
            auto& dragRect = m_dragSource->GetComponent<RectLayout>();
            auto& dragSource = m_dragSource->GetComponent<UIDragSource>();

            if (dragSource.dragging && pointerDown)
                dragRect.SetPosition(GetCenteredMousePosition(*scene) - dragSource.dragOffset);

            auto dropView = _registry.view<RectLayout, UIDropTarget>();
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

        Entity navigationPointerButton = hoveredButton;
        inputManager->worldUIPointer.hovered = pointer.enabled && hoveredButton != nullptr;
        if (pointer.enabled && pointer.moved && hoveredButton && hoveredButton != m_selectedButton)
        {
            m_navigationFocus = false;
            m_navigationPressedButton = nullptr;
        }
        if (hoveredButton != nullptr && hoveredButton->HasComponents<RectLayout, UIButton>())
        {
            UIButton& button = hoveredButton->GetComponent<UIButton>();
            RectLayout& rect = hoveredButton->GetComponent<RectLayout>();
            button.hovered = !m_navigationFocus;
            if (!m_navigationFocus) m_selectedButton = hoveredButton;
            ApplyButtonVisual(*hoveredButton, button, rect);
        }

        if (hoveredInputField != nullptr && hoveredInputField->HasComponents<RectLayout, UIInputField>())
        {
            UIInputField& inputField = hoveredInputField->GetComponent<UIInputField>();
            inputField.hovered = true;
            ApplyInputFieldVisual(*hoveredInputField, inputField);
        }

        if (hoveredDropTarget != nullptr && hoveredDropTarget->HasComponent<UIDropTarget>())
        {
            UIDropTarget& dropTarget = hoveredDropTarget->GetComponent<UIDropTarget>();
            dropTarget.hovered = true;
            ApplyDropTargetVisual(*hoveredDropTarget, dropTarget);
        }

        if (m_dragSource == nullptr)
        {
            if (pointerPressed)
            {
                FocusInputField(hoveredInputField);

                if (hoveredDragSource != nullptr && hoveredDragSource->HasComponents<RectLayout, UIDragSource>())
                {
                    UIDragSource& dragSource = hoveredDragSource->GetComponent<UIDragSource>();
                    RectLayout& dragRect = hoveredDragSource->GetComponent<RectLayout>();
                    dragSource.dragging = true;
                    dragSource.originalPosition = dragRect.GetLocalPosition();
                    dragSource.originalDepth = dragRect.Spatial().position.z;
                    dragSource.dragOffset = GetCenteredMousePosition(*scene) - dragRect.GetPosition();
                    m_dragSource = hoveredDragSource;
                }
                else if (hoveredInputField == nullptr && hoveredButton != nullptr)
                {
                    m_pressedButton = hoveredButton;
                }
            }
            else if (m_pressedButton != nullptr && pointerReleased)
            {
                if (m_pressedButton == hoveredButton && m_pressedButton->HasComponent<UIButton>())
                {
                    UIButton& button = m_pressedButton->GetComponent<UIButton>();
                    Entity* receiver = button.targetEntity != nullptr ? button.targetEntity.TryGet() : m_pressedButton.TryGet();
                    UIActionContext context = {};
                    context.sourceEntity = m_pressedButton;
                    context.targetEntity = receiver;
                    context.pointerPosition = GetCenteredMousePosition(*scene);
                    scene->app->DispatchUIAction(*receiver, button.targetScript, button.actionName, context);
                }

                m_pressedButton = nullptr;
            }
        }
        else if (m_dragSource->HasComponents<RectLayout, UIDragSource>() && pointerReleased)
        {
            UIDragSource& dragSource = m_dragSource->GetComponent<UIDragSource>();
            RectLayout& dragRect = m_dragSource->GetComponent<RectLayout>();

            if (hoveredDropTarget != nullptr && hoveredDropTarget->HasComponent<UIDropTarget>())
            {
                UIDropTarget& dropTarget = hoveredDropTarget->GetComponent<UIDropTarget>();
                Entity* receiver = dropTarget.targetEntity != nullptr ? dropTarget.targetEntity.TryGet() : hoveredDropTarget;

                UIActionContext context = {};
                context.sourceEntity = m_dragSource;
                context.targetEntity = receiver;
                context.pointerPosition = GetCenteredMousePosition(*scene);
                context.payloadType = dragSource.payloadType;
                context.payloadValue = dragSource.payloadValue;
                scene->app->DispatchUIAction(*receiver, dropTarget.targetScript, dropTarget.actionName, context);
            }

            dragRect.SetLocalPosition(dragSource.originalPosition);
            dragRect.Spatial().position.z = dragSource.originalDepth;
            dragSource.dragging = false;
            m_dragSource = nullptr;
            m_hoveredDropTarget = nullptr;
        }
        else
        {
            m_hoveredDropTarget = hoveredDropTarget;
        }

        if (m_focusedInputField != nullptr && m_focusedInputField->HasComponent<UIInputField>())
        {
            UIInputField& inputField = m_focusedInputField->GetComponent<UIInputField>();
            const auto& text = inputManager->GetTextInput();
            if (!text.empty())
                InsertInputText(inputField, text);
            const bool backspace = inputField.backspaceAction != 0
                ? inputManager->Action(ActionId{inputField.backspaceAction}).pressed
                : inputManager->JustPressedKey(Key::BACKSPACE);
            if (backspace)
                BackspaceInputText(inputField);
            if (inputManager->JustPressedKey(Key::DELETE))
                SetInputText(inputField, "");
            if (inputField.submitOnEnter && (inputManager->JustPressedKey(Key::RETURN) || inputManager->JustPressedKey(Key::KP_ENTER)))
                FocusInputField(nullptr);
            else if (inputManager->JustPressedKey(Key::ESCAPE))
                FocusInputField(nullptr);
            else
                RefreshInputFieldDisplay(inputField);
        }
        // Navigation shares the existing button visuals and action dispatch path.
        // World canvases remain pointer-driven while the mouse is captured.
        auto selectable = [&](Entity candidate) {
            if (!candidate || !candidate->Active() ||
                !candidate->HasComponents<RectLayout, UIButton>()) return false;
            const auto& rect = candidate->GetComponent<RectLayout>();
            const auto* canvas = rect.GetCanvas();
            return candidate->GetComponent<UIButton>().active && rect.IsActiveInHierarchy() &&
                (canvas == nullptr || (canvas->active && canvas->receivesEvents));
        };
        if (!selectable(m_selectedButton)) m_selectedButton = nullptr;
        if (!selectable(m_navigationPressedButton)) m_navigationPressedButton = nullptr;
        if (!inputManager->active || window->IsMouseLocked() || wasEditingText || m_focusedInputField || m_dragSource)
        {
            m_navigationPressedButton = nullptr;
            return;
        }
        bool up = inputManager->JustPressedKey(Key::UP) || inputManager->JustPressedButton(ControllerButton::DPAD_UP);
        bool down = inputManager->JustPressedKey(Key::DOWN) || inputManager->JustPressedButton(ControllerButton::DPAD_DOWN);
        bool left = inputManager->JustPressedKey(Key::LEFT) || inputManager->JustPressedButton(ControllerButton::DPAD_LEFT);
        bool right = inputManager->JustPressedKey(Key::RIGHT) || inputManager->JustPressedButton(ControllerButton::DPAD_RIGHT);
        bool confirm = inputManager->JustPressedKey(Key::RETURN) || inputManager->JustPressedKey(Key::KP_ENTER) ||
            inputManager->JustPressedKey(Key::SPACE) || inputManager->JustPressedButton(ControllerButton::A);
        bool held = inputManager->GetKey(Key::RETURN) || inputManager->GetKey(Key::KP_ENTER) ||
            inputManager->GetKey(Key::SPACE) || inputManager->GetButton(ControllerButton::A);
        bool released = inputManager->JustReleasedKey(Key::RETURN) || inputManager->JustReleasedKey(Key::KP_ENTER) ||
            inputManager->JustReleasedKey(Key::SPACE) || inputManager->JustReleasedButton(ControllerButton::A);
        Entity defaultButton = nullptr;
        if (!m_selectedButton)
            for (auto [handle, rect, button] : _registry.view<RectLayout, UIButton>().each())
                if (button.defaultSelected && selectable(button.entity)) { defaultButton = button.entity; break; }
        Entity inputButton = m_selectedButton ? m_selectedButton : defaultButton;
        const Canvas* canvas = inputButton ? inputButton->GetComponent<RectLayout>().GetCanvas() : nullptr;
        Entity canvasEntity = canvas ? canvas->entity : nullptr;
        if (canvasEntity != m_navigationCanvas)
        {
            m_navigationCanvas = canvasEntity;
            m_navigationDirection = Vector2(0.0f);
            m_navigationRepeat = 0.0f;
        }
        if (canvas && !canvas->navigationEnabled)
        {
            m_navigationPressedButton = nullptr;
            m_navigationDirection = Vector2(0.0f);
            return;
        }
        if (canvas && canvas->navigateAction != 0)
        {
            if (!m_selectedButton) m_selectedButton = defaultButton;
            const auto value = inputManager->Action(ActionId{canvas->navigateAction}).value;
            const Vector2 direction(std::abs(value.x) > .45f ? (value.x > 0 ? 1.f : -1.f) : 0.f,
                                    std::abs(value.y) > .45f ? (value.y > 0 ? 1.f : -1.f) : 0.f);
            bool step = direction != Vector2(0.0f) && direction != m_navigationDirection;
            if (direction == Vector2(0.0f)) m_navigationRepeat = 0;
            else if (step) m_navigationRepeat = .35f;
            else { m_navigationRepeat -= std::max(0.f, _deltaTime); if (m_navigationRepeat <= 0) { step = true; m_navigationRepeat = .12f; } }
            m_navigationDirection = direction;
            up = step && direction.y > 0; down = step && direction.y < 0;
            left = step && direction.x < 0; right = step && direction.x > 0;
        }
        if (canvas && canvas->confirmAction != 0)
        {
            const auto action = inputManager->Action(ActionId{canvas->confirmAction});
            confirm = action.pressed; held = action.down; released = action.released;
            if (action.canceled) { m_navigationPressedButton = nullptr; released = false; }
        }
        if (up || down || left || right || confirm) m_navigationFocus = true;
        if (!m_navigationFocus) return;
        bool acquired = false;
        if (!m_selectedButton)
        {
            for (auto [handle, rect, button] : _registry.view<RectLayout, UIButton>().each())
            {
                if (button.defaultSelected && selectable(button.entity))
                {
                    m_selectedButton = button.entity;
                    acquired = true;
                    break;
                }
            }
        }
        if (!m_selectedButton) return;
        auto& selected = m_selectedButton->GetComponent<UIButton>();
        Entity next = up ? selected.up : down ? selected.down : left ? selected.left : right ? selected.right : Entity(nullptr);
        // Follow the same direction through disabled controls, with a bounded
        // traversal so a disabled cycle or stale link cannot trap navigation.
        for (std::size_t visited = 0; next && !selectable(next) && visited < _registry.view<UIButton>().size(); ++visited)
        {
            if (!next->HasComponent<UIButton>()) { next = nullptr; break; }
            const auto& link = next->GetComponent<UIButton>();
            next = up ? link.up : down ? link.down : left ? link.left : link.right;
        }
        if (!acquired && selectable(next) && next != m_selectedButton)
        {
            m_selectedButton = next;
            m_navigationPressedButton = nullptr;
        }
        if (confirm) m_navigationPressedButton = m_selectedButton;
        if (navigationPointerButton && navigationPointerButton != m_selectedButton &&
            navigationPointerButton->HasComponents<UIButton, RectLayout>())
        {
            auto& hovered = navigationPointerButton->GetComponent<UIButton>();
            hovered.hovered = false;
            ApplyButtonVisual(*navigationPointerButton, hovered, navigationPointerButton->GetComponent<RectLayout>());
        }
        auto& button = m_selectedButton->GetComponent<UIButton>();
        button.hovered = true;
        button.pressed = m_navigationPressedButton == m_selectedButton && held;
        ApplyButtonVisual(*m_selectedButton, button, m_selectedButton->GetComponent<RectLayout>());
        if (released && !held && m_navigationPressedButton)
        {
            Entity source = m_navigationPressedButton;
            m_navigationPressedButton = nullptr;
            if (source != m_selectedButton) return;
            Entity receiver = button.targetEntity != nullptr ? button.targetEntity : source;
            if (!receiver) return;
            UIActionContext context = {};
            context.sourceEntity = source;
            context.targetEntity = receiver;
            context.pointerPosition = source->GetComponent<RectLayout>().GetPosition();
            scene->app->DispatchUIAction(*receiver, button.targetScript, button.actionName, context);
        }
    }
}
