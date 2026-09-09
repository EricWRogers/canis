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

        bool IsPointInsideRect(const RectTransform& _rect, const Vector2& _point)
        {
            const Vector2 min = _rect.GetRectMin() + _rect.originOffset;
            const Vector2 size = _rect.GetResolvedSize();
            return _point.x >= min.x &&
                _point.x <= (min.x + size.x) &&
                _point.y >= min.y &&
                _point.y <= (min.y + size.y);
        }

        bool TryGetWorldCanvasPointer(
            entt::registry &_registry,
            Scene &_scene,
            const RectTransform &_rect,
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

            Ray ray = {};
            const Vector2 rayPosition = _scene.GetWindow().IsMouseLocked()
                ? Vector2(
                    _scene.GetWindow().GetScreenWidth() * 0.5f,
                    _scene.GetWindow().GetScreenHeight() * 0.5f)
                : _scene.GetInputManager().mouse;
            if (!_scene.TryGetRayFromCamera(*cameraEntity, rayPosition, ray))
                return false;

            const Matrix4 model =
                canvas->entity->GetComponent<Transform>().GetModelMatrix();
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
            _pointerPosition = Vector2(localPoint.x, localPoint.y);
            return true;
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

        bool InputFieldAllowsText(const UIInputField& _inputField, const std::string& _text)
        {
            if (_inputField.allowedCharacters.empty())
                return true;

            for (char c : _text)
            {
                if (_inputField.allowedCharacters.find(c) == std::string::npos)
                    return false;
            }

            return true;
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

    void UIInteractionSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        if (scene == nullptr || scene->app == nullptr || inputManager == nullptr || window == nullptr)
            return;

        auto setFocusedInputField = [&](Entity* _entity) -> void
        {
            if (m_focusedInputField == _entity)
                return;

            if (m_focusedInputField != nullptr && m_focusedInputField->HasComponent<UIInputField>())
            {
                UIInputField& previousField = m_focusedInputField->GetComponent<UIInputField>();
                previousField.focused = false;
                previousField.caretVisible = true;
                previousField.caretBlinkTimer = 0.0f;
                RefreshInputFieldDisplay(previousField);
            }

            m_focusedInputField = nullptr;
            SDL_StopTextInput((SDL_Window*)window->GetSDLWindow());

            if (_entity != nullptr && _entity->HasComponent<UIInputField>())
            {
                UIInputField& nextField = _entity->GetComponent<UIInputField>();
                nextField.focused = true;
                nextField.caretVisible = true;
                nextField.caretBlinkTimer = 0.0f;
                m_focusedInputField = _entity;
                SDL_StartTextInput((SDL_Window*)window->GetSDLWindow());
                RefreshInputFieldDisplay(nextField);
            }
        };

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
            (!m_pressedButton->Active() ||
             !m_pressedButton->HasComponents<RectTransform, UIButton>() ||
             !m_pressedButton->GetComponent<UIButton>().active ||
             !m_pressedButton->GetComponent<RectTransform>().IsActiveInHierarchy()))
        {
            m_pressedButton = nullptr;
        }

        if (m_focusedInputField != nullptr &&
            (!m_focusedInputField->Active() ||
             !m_focusedInputField->HasComponents<RectTransform, UIInputField>() ||
             !m_focusedInputField->GetComponent<UIInputField>().active ||
             !m_focusedInputField->GetComponent<RectTransform>().IsActiveInHierarchy()))
        {
            setFocusedInputField(nullptr);
        }

        if (m_dragSource != nullptr &&
            (!m_dragSource->Active() ||
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

            auto inputFieldView = _registry.view<RectTransform, UIInputField>();
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

            setFocusedInputField(nullptr);
            m_hoveredDropTarget = nullptr;
        }

        for (auto [entityHandle, rect, button] : _registry.view<RectTransform, UIButton>().each())
        {
            (void)entityHandle;
            Entity* entity = button.entity;
            if (entity == nullptr || !button.active)
                continue;

            if (button.baseValuesSaved == false)
            {
                button.baseValuesSaved = true;
                button.baseScale = rect.scale.x;

                if (entity->HasComponent<Sprite2D>())
                    button.baseColor = entity->GetComponent<Sprite2D>().color;
                else if (entity->HasComponent<Text>())
                    button.baseColor = entity->GetComponent<Text>().color;
            }

            const bool visible = rect.IsActiveInHierarchy();
            button.hovered = false;
            button.pressed = visible && (m_pressedButton == entity) && inputManager->GetLeftClick();
            ApplyButtonVisual(*entity, button, rect);
        }

        for (auto [entityHandle, rect, inputField] : _registry.view<RectTransform, UIInputField>().each())
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
        Entity* hoveredInputField = nullptr;
        Entity* hoveredDragSource = nullptr;
        Entity* hoveredDropTarget = nullptr;
        float hoveredButtonDepth = FLT_MAX;
        float hoveredInputFieldDepth = FLT_MAX;
        float hoveredDragDepth = FLT_MAX;
        float hoveredDropDepth = FLT_MAX;

        auto evaluateRectEntity = [&](Entity* _entity, RectTransform& _rect, float& _bestDepth, Entity*& _bestEntity) -> void
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
            auto buttonView = _registry.view<RectTransform, UIButton>();
            for (auto [entityHandle, rect, button] : buttonView.each())
            {
                (void)entityHandle;
                if (button.active)
                    evaluateRectEntity(button.entity, rect, hoveredButtonDepth, hoveredButton);
            }

            auto inputFieldView = _registry.view<RectTransform, UIInputField>();
            for (auto [entityHandle, rect, inputField] : inputFieldView.each())
            {
                (void)entityHandle;
                if (inputField.active)
                    evaluateRectEntity(inputField.entity, rect, hoveredInputFieldDepth, hoveredInputField);
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

        if (hoveredButton != nullptr && hoveredButton->HasComponents<RectTransform, UIButton>())
        {
            UIButton& button = hoveredButton->GetComponent<UIButton>();
            RectTransform& rect = hoveredButton->GetComponent<RectTransform>();
            button.hovered = true;
            ApplyButtonVisual(*hoveredButton, button, rect);
        }

        if (hoveredInputField != nullptr && hoveredInputField->HasComponents<RectTransform, UIInputField>())
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
            if (inputManager->JustLeftClicked())
            {
                setFocusedInputField(hoveredInputField);

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
                else if (hoveredInputField == nullptr && hoveredButton != nullptr)
                {
                    m_pressedButton = hoveredButton;
                }
            }
            else if (m_pressedButton != nullptr && inputManager->LeftClickReleased())
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
        else if (m_dragSource->HasComponents<RectTransform, UIDragSource>() && inputManager->LeftClickReleased())
        {
            UIDragSource& dragSource = m_dragSource->GetComponent<UIDragSource>();
            RectTransform& dragRect = m_dragSource->GetComponent<RectTransform>();

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

        if (m_focusedInputField != nullptr && m_focusedInputField->HasComponent<UIInputField>())
        {
            UIInputField& inputField = m_focusedInputField->GetComponent<UIInputField>();
            bool textChanged = false;

            const std::string& textInput = inputManager->GetTextInput();
            if (!textInput.empty() && InputFieldAllowsText(inputField, textInput))
            {
                const std::size_t available = (inputField.maxLength <= 0)
                    ? std::string::npos
                    : static_cast<std::size_t>(inputField.maxLength) > inputField.text.size()
                        ? static_cast<std::size_t>(inputField.maxLength) - inputField.text.size()
                        : 0u;
                if (available == std::string::npos)
                {
                    inputField.text += textInput;
                    textChanged = true;
                }
                else if (available > 0u)
                {
                    inputField.text += textInput.substr(0u, available);
                    textChanged = true;
                }
            }

            if (inputManager->JustPressedKey(Key::BACKSPACE) && !inputField.text.empty())
            {
                inputField.text.pop_back();
                textChanged = true;
            }

            if (inputManager->JustPressedKey(Key::DELETE))
            {
                if (!inputField.text.empty())
                {
                    inputField.text.clear();
                    textChanged = true;
                }
            }

            if (textChanged)
                PushInputFieldToBinding(*scene, inputField);

            if (inputManager->JustPressedKey(Key::RETURN) || inputManager->JustPressedKey(Key::KP_ENTER))
                setFocusedInputField(nullptr);
            else
                RefreshInputFieldDisplay(inputField);
        }
    }
}
