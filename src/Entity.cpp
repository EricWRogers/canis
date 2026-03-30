#include <Canis/App.hpp>
#include <Canis/Entity.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>

#include <cmath>


namespace Canis {

namespace
{
    Vector2 AbsVector2(const Vector2& _value)
    {
        return Vector2(std::abs(_value.x), std::abs(_value.y));
    }

    Vector2 GetActiveCamera2DPosition(const Scene& _scene)
    {
        if (_scene.HasEditorCamera2DOverride())
            return _scene.GetEditorCamera2DPosition();

        auto cameraView = _scene.GetRegistry().view<Camera2D>();
        for (const entt::entity entityHandle : cameraView)
            return cameraView.get<Camera2D>(entityHandle).GetPosition();

        return Vector2(0.0f);
    }
}

ScriptableEntity* Entity::AddScriptDirect(const ScriptConf& _conf, ScriptableEntity* _scriptableEntity, bool _callCreate)
{
    if (_scriptableEntity == nullptr)
        return nullptr;

    if (ScriptableEntity* existing = GetScriptDirect(_conf))
    {
        if (existing != _scriptableEntity)
            delete _scriptableEntity;
        return existing;
    }

    m_scriptComponents.push_back(_scriptableEntity);

    if (_callCreate)
        _scriptableEntity->Create();

    return _scriptableEntity;
}

ScriptableEntity* Entity::GetScriptDirect(const ScriptConf& _conf)
{
    if (_conf.Get == nullptr)
        return nullptr;

    return static_cast<ScriptableEntity*>(_conf.Get(*this));
}

const ScriptableEntity* Entity::GetScriptDirect(const ScriptConf& _conf) const
{
    if (_conf.Get == nullptr)
        return nullptr;

    return static_cast<const ScriptableEntity*>(_conf.Get(const_cast<Entity&>(*this)));
}

void Entity::RemoveScriptDirect(const ScriptConf& _conf)
{
    ScriptableEntity* script = GetScriptDirect(_conf);
    if (script == nullptr)
        return;

    for (size_t i = 0; i < m_scriptComponents.size(); ++i)
    {
        if (m_scriptComponents[i] != script)
            continue;

        script->Destroy();
        delete script;
        m_scriptComponents.erase(m_scriptComponents.begin() + i);
        break;
    }
}

ScriptableEntity* Entity::AttachScript(const std::string& _scriptName, ScriptableEntity* _scriptableEntity, bool _callCreate)
{
    if (scene.app == nullptr)
    {
        delete _scriptableEntity;
        return nullptr;
    }

    ScriptConf* conf = scene.app->GetScriptConf(_scriptName);
    if (conf == nullptr)
    {
        delete _scriptableEntity;
        return nullptr;
    }

    return AddScriptDirect(*conf, _scriptableEntity, _callCreate);
}

void Entity::RemoveScript(const std::string& _scriptName)
{
    if (scene.app == nullptr)
        return;

    ScriptConf* conf = scene.app->GetScriptConf(_scriptName);
    if (conf == nullptr)
        return;

    RemoveScriptDirect(*conf);
}

void Entity::RemoveAllScripts()
{
    for (int i = static_cast<int>(m_scriptComponents.size()) - 1; i >= 0; --i)
    {
        ScriptableEntity* script = m_scriptComponents[static_cast<size_t>(i)];

        if (script != nullptr)
        {
            script->Destroy();
            delete script;
        }
    }

    m_scriptComponents.clear();
}

void Entity::Destroy() {
    scene.Destroy(id);
}

RectTransform::LayoutData RectTransform::GetLayout() const
{
    LayoutData layout = {};

    if (entity == nullptr)
        return layout;

    const bool hasParentRect = parent != nullptr && parent->HasComponent<RectTransform>();
    const unsigned int renderMode = GetCanvasRenderMode();

    Vector2 parentMin = Vector2(0.0f);
    Vector2 parentSize = Vector2(0.0f);
    Vector2 parentPivot = Vector2(0.0f);
    Vector2 parentScale = Vector2(1.0f);
    float parentRotation = 0.0f;

    if (hasParentRect)
    {
        const RectTransform& parentRect = parent->GetComponent<RectTransform>();
        parentMin = parentRect.GetRectMin();
        parentSize = parentRect.GetResolvedSize();
        parentPivot = parentRect.GetPosition();
        parentScale = AbsVector2(parentRect.GetScale());
        parentRotation = parentRect.GetRotation();
    }
    else if (renderMode != CanvasRenderMode::WORLD_SPACE)
    {
        const float screenWidth = static_cast<float>(entity->scene.GetWindow().GetScreenWidth());
        const float screenHeight = static_cast<float>(entity->scene.GetWindow().GetScreenHeight());

        parentSize = Vector2(screenWidth, screenHeight);
        if (renderMode == CanvasRenderMode::SCREEN_SPACE_OVERLAY)
        {
            parentMin = Vector2(-screenWidth * 0.5f, -screenHeight * 0.5f);
            parentPivot = Vector2(0.0f);
        }
        else
        {
            parentMin = GetActiveCamera2DPosition(entity->scene) - (parentSize * 0.5f);
            parentPivot = parentMin + (parentSize * 0.5f);
        }
    }

    Vector2 resolvedSize = size;
    if (hasParentRect || renderMode != CanvasRenderMode::WORLD_SPACE)
        resolvedSize += parentSize * (anchorMax - anchorMin);

    resolvedSize.x *= parentScale.x * std::abs(scale.x);
    resolvedSize.y *= parentScale.y * std::abs(scale.y);

    Vector2 pivotPosition = position;

    if (hasParentRect || renderMode != CanvasRenderMode::WORLD_SPACE)
    {
        const Vector2 anchorRectMin = parentMin + parentSize * anchorMin;
        const Vector2 anchorRectMax = parentMin + parentSize * anchorMax;
        const Vector2 scaledPosition = Vector2(position.x * parentScale.x, position.y * parentScale.y);

        pivotPosition = anchorRectMin + ((anchorRectMax - anchorRectMin) * pivot) + scaledPosition;

        if (hasParentRect && parentRotation != 0.0f)
            pivotPosition = parentPivot + RotatePoint(pivotPosition - parentPivot, parentRotation);
    }

    layout.size = resolvedSize;
    layout.pivotPosition = pivotPosition;
    layout.min = pivotPosition - Vector2(layout.size.x * pivot.x, layout.size.y * pivot.y);
    return layout;
}

const Canvas* RectTransform::FindCanvas() const
{
    const Entity* current = entity;

    while (current != nullptr)
    {
        if (current->HasComponent<Canvas>())
            return &current->GetComponent<Canvas>();

        if (!current->HasComponent<RectTransform>())
            break;

        current = current->GetComponent<RectTransform>().parent;
    }

    return nullptr;
}

Vector2 RectTransform::GetNormalizedAnchor(const RectAnchor &_anchor)
{
    switch (_anchor)
    {
    case RectAnchor::TOPLEFT:
        return Vector2(0.0f, 1.0f);
    case RectAnchor::TOPCENTER:
        return Vector2(0.5f, 1.0f);
    case RectAnchor::TOPRIGHT:
        return Vector2(1.0f, 1.0f);
    case RectAnchor::CENTERLEFT:
        return Vector2(0.0f, 0.5f);
    case RectAnchor::CENTER:
        return Vector2(0.5f, 0.5f);
    case RectAnchor::CENTERRIGHT:
        return Vector2(1.0f, 0.5f);
    case RectAnchor::BOTTOMLEFT:
        return Vector2(0.0f, 0.0f);
    case RectAnchor::BOTTOMCENTER:
        return Vector2(0.5f, 0.0f);
    case RectAnchor::BOTTOMRIGHT:
        return Vector2(1.0f, 0.0f);
    default:
        return Vector2(0.5f, 0.5f);
    }
}

Vector2 RectTransform::GetPosition() const
{
    return GetLayout().pivotPosition;
}

void RectTransform::SetPosition(Vector2 _globalPos)
{
    Vector2 delta = _globalPos - GetPosition();

    if (parent != nullptr && parent->HasComponent<RectTransform>())
    {
        const RectTransform& parentRect = parent->GetComponent<RectTransform>();
        const Vector2 parentScale = AbsVector2(parentRect.GetScale());
        const float parentRotation = parentRect.GetRotation();

        if (parentRotation != 0.0f)
            delta = RotatePoint(delta, -parentRotation);

        if (parentScale.x != 0.0f)
            delta.x /= parentScale.x;
        if (parentScale.y != 0.0f)
            delta.y /= parentScale.y;
    }

    position += delta;
}

Vector2 RectTransform::GetResolvedSize() const
{
    return GetLayout().size;
}

Vector2 RectTransform::GetRectMin() const
{
    return GetLayout().min;
}

unsigned int RectTransform::GetCanvasRenderMode() const
{
    if (const Canvas* canvas = FindCanvas())
        return canvas->renderMode;

    return CanvasRenderMode::SCREEN_SPACE_CAMERA;
}

bool RectTransform::IsActiveInHierarchy() const
{
    if (entity == nullptr || !entity->active || !active)
        return false;

    if (entity->HasComponent<Canvas>() && !entity->GetComponent<Canvas>().active)
        return false;

    if (parent != nullptr && parent->HasComponent<RectTransform>())
        return parent->GetComponent<RectTransform>().IsActiveInHierarchy();

    return true;
}

void RectTransform::SetAnchorPreset(RectAnchor _anchor)
{
    const Vector2 normalizedAnchor = GetNormalizedAnchor(_anchor);
    anchorMin = normalizedAnchor;
    anchorMax = normalizedAnchor;
    pivot = normalizedAnchor;
}

int RectTransform::GetAnchorPreset() const
{
    if (anchorMin != anchorMax || pivot != anchorMin)
        return -1;

    for (int i = 0; i < 9; i++)
    {
        if (anchorMin == GetNormalizedAnchor(static_cast<RectAnchor>(i)))
            return i;
    }

    return -1;
}

void Camera2D::Create() {
    if (entity == nullptr)
        return;

    m_screenWidth = entity->scene.GetWindow().GetScreenWidth();
    m_screenHeight = entity->scene.GetWindow().GetScreenHeight();
    m_projection = glm::ortho(0.0f, (float)m_screenWidth, 0.0f,
                                      (float)m_screenHeight, -100.0f, 100.0f);
    SetPosition(Vector2(0.0f)); // Vector2((float)m_screenWidth / 2,
                                // (float)m_screenHeight / 2));
    SetScale(1.0f);
}

void Camera2D::Destroy() {
    Debug::Log("DestroyCamera");
}

void Camera2D::Update(float _dt) {}

void Camera2D::UpdateMatrix()
{
    if (entity == nullptr)
        return;

    m_screenWidth = entity->scene.GetWindow().GetScreenWidth();
    m_screenHeight = entity->scene.GetWindow().GetScreenHeight();
    m_projection = glm::ortho(0.0f, (float)m_screenWidth, 0.0f,
                                      (float)m_screenHeight, -100.0f, 100.0f);
                                      
    m_view = Matrix4(1.0f);
    m_view = glm::translate(m_view, Vector3(-m_position.x + m_screenWidth / 2,
                                             -m_position.y + m_screenHeight / 2, 0.0f));
    m_view = glm::scale(m_view, Vector3(m_scale, m_scale, 0.0f));

    m_cameraMatrix = m_projection * m_view;

    m_needsMatrixUpdate = false;
}

void SpriteAnimation::Play(std::string _path)
{
    speed = 1.0f;
    id = AssetManager::LoadSpriteAnimation(_path);
    index = 0;
    redraw = true;
}
} // namespace Canis
