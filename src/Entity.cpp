#include <Canis/App.hpp>
#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Window.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Debug.hpp>

#include <algorithm>
#include <cmath>


namespace Canis {
Scene& Entity::EmptyScene() { static Scene empty; return empty; }
Entity::Entity() : Entity(EmptyScene(), entt::null) {}
Entity::Entity(std::nullptr_t) : Entity() {}
Entity::Entity(Entity* entity) : Entity(entity ? entity->scene : EmptyScene(), entity ? entity->GetHandle() : entt::null) {}
Entity& Entity::operator=(const Entity& other) {
    if (this != &other) {
        // Reconstruct the value to bind its Scene& to the assigned reference's scene.
        std::destroy_at(this);
        return *std::construct_at(this, other);
    }
    return *this;
}
entt::registry& Entity::Registry() const { return scene.GetRegistry(); }
void Entity::RequestComponentCreate(entt::id_type type, std::function<void()> callback)
{
    scene.RequestComponentCreate(m_entityHandle, type, std::move(callback));
}
void Entity::CancelComponentCreate(entt::id_type type) { scene.CancelComponentCreate(m_entityHandle, type); }

ScriptableEntity* Entity::InvokeScriptCreate(ScriptableEntity* script) { return scene.InvokeScriptCallback(script, false); }
Entity* Entity::ResolveIncludingPending() const {
    return m_entityHandle != entt::null && Registry().valid(m_entityHandle)
        ? Registry().try_get<Entity>(m_entityHandle) : nullptr;
}
Entity* Entity::CanonicalEntity() const {
    if (auto* entity = ResolveIncludingPending()) return entity;
    throw std::runtime_error("Missing entity owner");
}
bool Entity::IsValid() const {
    if (m_entityHandle == entt::null || !Registry().valid(m_entityHandle)) return false;
    auto* state = scene.GetEntityState(m_entityHandle);
    return state && !state->pendingDestroy;
}
Entity* Entity::TryGet() const { return IsValid() ? Registry().try_get<Entity>(m_entityHandle) : nullptr; }
Entity* Entity::operator->() const {
    if (auto* entity = TryGet()) return entity;
    throw std::runtime_error("Access to a missing or destroyed entity");
}
UUID Entity::GetUUID() const { return m_entityHandle == entt::null ? UUID(0) : scene.GetReferenceUUID(m_entityHandle); }


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

    if (!IsValid()) { delete _scriptableEntity; return nullptr; }
    StoreScriptInstance(_scriptableEntity);

    if (_callCreate)
        return scene.InvokeScriptCallback(_scriptableEntity, false);

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

    RemoveScriptInstance(script);
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

const std::vector<ScriptableEntity*>& Entity::ScriptInstances() const
{
    static const std::vector<ScriptableEntity*> empty;
    const auto* component = scene.GetRegistry().valid(m_entityHandle)
        ? scene.GetRegistry().try_get<ScriptComponent>(m_entityHandle) : nullptr;
    return component ? component->instances : empty;
}
void Entity::StoreScriptInstance(ScriptableEntity* script)
{
    auto& component = scene.GetRegistry().get_or_emplace<ScriptComponent>(m_entityHandle);
    component.instances.push_back(script);
    scene.QueueEntityForReady(*this);
}
void Entity::RemoveScriptInstance(ScriptableEntity* script)
{
    auto* component = scene.GetRegistry().try_get<ScriptComponent>(m_entityHandle);
    if (!component) return;
    auto& instances = component->instances;
    const auto it = std::find(instances.begin(), instances.end(), script);
    if (it == instances.end()) return;
    instances.erase(it);
    script->m_lifetime->script = nullptr;
    // Remove storage before callbacks: a callback can attach a fresh first script.
    if (instances.empty()) scene.GetRegistry().remove<ScriptComponent>(m_entityHandle);
    scene.RetireScript(script);
}
std::vector<ScriptHandle<ScriptableEntity>> Entity::GetScripts() const
{
    std::vector<ScriptHandle<ScriptableEntity>> result;
    for (auto* script : ScriptInstances()) result.emplace_back(script);
    return result;
}
void Entity::RemoveAllScripts()
{
    // Reacquire each time; removing the final instance also removes the component.
    while (!ScriptInstances().empty()) RemoveScriptInstance(ScriptInstances().back());
}

void Entity::Destroy() {
    scene.Destroy(*this);
}

void Transform::BeforeRemove()
{
    Destroy();
    if (entity) entity->RemoveComponent<RectLayout>();
}

void RectLayout::EnsureDependencies()
{
    if (entity && !entity->HasComponent<Transform>())
        entity->AddComponent<Transform>()->position.z = .001f;
}
Transform& RectLayout::Spatial() const { return entity->GetComponent<Transform>(); }
Entity RectLayout::GetParent() const { return Spatial().parent; }
std::vector<Entity>& RectLayout::GetChildren() const { return Spatial().children; }
Vector2 RectLayout::GetLocalPosition() const { return Vector2(Spatial().position); }
void RectLayout::SetLocalPosition(Vector2 value) { Spatial().position.x = value.x; Spatial().position.y = value.y; }
Vector2 RectLayout::GetLocalScale() const { return Vector2(Spatial().scale); }
void RectLayout::SetLocalScale(Vector2 value) { Spatial().scale.x = value.x; Spatial().scale.y = value.y; }
float RectLayout::GetLocalRotation() const { return glm::eulerAngles(Spatial().rotation).z; }
void RectLayout::SetLocalRotation(float value) { Spatial().rotation = Quaternion(Vector3(0, 0, value)); }
float RectLayout::GetLocalDepth() const { return Spatial().position.z; }
void RectLayout::SetLocalDepth(float value) { Spatial().position.z = value; }
bool RectLayout::IsWorldCanvasRoot() const
{
    auto* canvas = entity ? entity->TryGetComponent<Canvas>() : nullptr;
    return canvas && canvas->renderMode == CanvasRenderMode::WORLD_SPACE;
}

Vector2 RectLayout::GetLogicalSize() const
{
    if (!entity || IsWorldCanvasRoot()) return size;
    if (!entity->scene.TryGetWindow() && !GetParent()) return size;
    Vector2 extent(0);
    const auto parent = GetParent();
    if (parent && parent.HasComponent<RectLayout>()) extent = parent.GetComponent<RectLayout>().GetLogicalSize();
    else if (!parent && GetCanvasRenderMode() != CanvasRenderMode::WORLD_SPACE)
        extent = GetCanvasRenderMode() == CanvasRenderMode::SCREEN_SPACE_OVERLAY ? GetCanvasOverlayLogicalSize()
            : Vector2(entity->scene.GetWindow().GetScreenWidth(), entity->scene.GetWindow().GetScreenHeight());
    return size + extent * (anchorMax - anchorMin);
}

Matrix4 RectLayout::GetPlacementMatrix() const
{
    Matrix4 placement(1);
    if (!entity || IsWorldCanvasRoot()) return placement;
    if (!entity->scene.TryGetWindow() && !GetParent()) return placement;
    Vector2 extent(0), parentPivot(.5f), origin(0);
    const auto parent = GetParent();
    const auto mode = GetCanvasRenderMode();
    if (parent && parent.HasComponent<RectLayout>())
    {
        const auto& layout = parent.GetComponent<RectLayout>();
        extent = layout.GetLogicalSize(); parentPivot = layout.pivot;
    }
    else if (!parent && mode != CanvasRenderMode::WORLD_SPACE)
    {
        if (mode == CanvasRenderMode::SCREEN_SPACE_OVERLAY)
        {
            extent = GetCanvasOverlayLogicalSize();
            const float factor = GetCanvasOverlayScaleFactor();
            placement = glm::scale(placement, Vector3(factor, factor, 1));
        }
        else
        {
            extent = Vector2(entity->scene.GetWindow().GetScreenWidth(), entity->scene.GetWindow().GetScreenHeight());
            origin = GetActiveCamera2DPosition(entity->scene);
        }
    }
    const Vector2 anchor = anchorMin + (anchorMax - anchorMin) * pivot;
    return glm::translate(placement, Vector3(origin + extent * (anchor - parentPivot), 0));
}

Matrix4 Transform::GetLocalMatrix() const
{
    Matrix4 matrix(1);
    if (entity)
        if (auto* layout = entity->TryGetComponent<RectLayout>()) matrix = layout->GetPlacementMatrix();
    matrix = glm::translate(matrix, position);
    matrix *= glm::mat4_cast(glm::normalize(rotation));
    matrix = glm::scale(matrix, scale);
    return useLocalMatrixPrefix ? localMatrixPrefix * matrix : matrix;
}
void Transform::SetGlobalPosition(const Vector3& worldPosition)
{
    Matrix4 basis = parent && parent.HasComponent<Transform>() ? parent.GetComponent<Transform>().GetModelMatrix() : Matrix4(1);
    if (useLocalMatrixPrefix) basis *= localMatrixPrefix;
    if (entity)
        if (auto* layout = entity->TryGetComponent<RectLayout>()) basis *= layout->GetPlacementMatrix();
    if (const float determinant = glm::determinant(basis); std::isfinite(determinant) && determinant != 0)
        position = Vector3(glm::inverse(basis) * Vector4(worldPosition, 1));
}
Matrix4 RectLayout::GetCanvasLocalMatrix() const
{
    if (!entity) return Matrix4(1);
    if (const auto* canvas = FindCanvas(); canvas && canvas->renderMode == CanvasRenderMode::WORLD_SPACE && canvas->entity)
    {
        Matrix4 relative(1);
        for (Entity current = entity; current && current != canvas->entity;)
        {
            const auto& spatial = current.GetComponent<Transform>();
            relative = spatial.GetLocalMatrix() * relative;
            current = spatial.parent;
        }
        return relative;
    }
    return Spatial().GetModelMatrix();
}
RectLayout::LayoutData RectLayout::GetLayout() const
{
    LayoutData data;
    if (!entity) return data;
    const auto matrix = GetCanvasLocalMatrix();
    data.pivotPosition = Vector2(matrix[3]);
    data.size = GetLogicalSize() * Vector2(glm::length(Vector3(matrix[0])), glm::length(Vector3(matrix[1])));
    data.min = data.pivotPosition - data.size * pivot;
    return data;
}
const Canvas* RectLayout::FindCanvas() const
{
    for (Entity current = entity; current;)
    {
        if (auto* canvas = current.TryGetComponent<Canvas>()) return canvas;
        auto* transform = current.TryGetComponent<Transform>();
        current = transform ? transform->parent : Entity(nullptr);
    }
    return nullptr;
}
Vector2 RectLayout::GetPosition() const { return GetLayout().pivotPosition; }
void RectLayout::SetPosition(Vector2 value)
{
    if (IsWorldCanvasRoot())
    {
        const auto model = Spatial().GetModelMatrix();
        Spatial().SetGlobalPosition(Spatial().GetGlobalPosition() + Vector3(model * Vector4(value, 0, 0)));
        return;
    }
    Matrix4 basis(1);
    const auto parent = GetParent();
    if (const auto* canvas = FindCanvas(); canvas && canvas->renderMode == CanvasRenderMode::WORLD_SPACE)
    {
        for (Entity current = parent; current && current != canvas->entity;)
        {
            const auto& spatial = current.GetComponent<Transform>();
            basis = spatial.GetLocalMatrix() * basis;
            current = spatial.parent;
        }
    }
    else if (parent) basis = parent.GetComponent<Transform>().GetModelMatrix();
    if (Spatial().useLocalMatrixPrefix) basis *= Spatial().localMatrixPrefix;
    basis *= GetPlacementMatrix();
    const float determinant = glm::determinant(basis);
    if (std::isfinite(determinant) && determinant != 0)
        Spatial().position += Vector3(glm::inverse(basis) * Vector4(value - GetPosition(), 0, 0));
}
Vector2 RectLayout::GetScale() const
{
    const Matrix4 matrix = GetCanvasLocalMatrix();
    return {glm::length(Vector3(matrix[0])), glm::length(Vector3(matrix[1]))};
}
void RectLayout::SetScale(const Vector2& value)
{
    SetLocalScale(Vector2(1));
    const Vector2 parentScale = GetScale();
    SetLocalScale({parentScale.x != 0 ? value.x / parentScale.x : value.x, parentScale.y != 0 ? value.y / parentScale.y : value.y});
}
float RectLayout::GetRotation() const
{
    const auto matrix = GetCanvasLocalMatrix();
    return std::atan2(matrix[0].y, matrix[0].x);
}
float RectLayout::GetDepth() const { return GetCanvasLocalMatrix()[3].z; }
Vector2 RectLayout::GetRight() const { const float a = GetRotation(); return {std::cos(a), std::sin(a)}; }
Vector2 RectLayout::GetUp() const { const float a = GetRotation(); return {-std::sin(a), std::cos(a)}; }
Vector2 RectLayout::GetResolvedSize() const { return GetLayout().size; }
Vector2 RectLayout::GetRectMin() const { return GetLayout().min; }
unsigned int RectLayout::GetCanvasRenderMode() const
{
    const auto* canvas = FindCanvas();
    return canvas ? canvas->renderMode : CanvasRenderMode::SCREEN_SPACE_CAMERA;
}
bool RectLayout::IsActiveInHierarchy() const
{
    if (!entity || !active || !Spatial().IsActiveInHierarchy()) return false;
    for (Entity current = entity; current;)
    {
        if (auto* canvas = current.TryGetComponent<Canvas>(); canvas && !canvas->active) return false;
        if (auto* layout = current.TryGetComponent<RectLayout>(); layout && !layout->active) return false;
        auto* transform = current.TryGetComponent<Transform>();
        current = transform ? transform->parent : Entity(nullptr);
    }
    return true;
}
void RectLayout::SetAnchorPreset(RectAnchor anchor) { anchorMin = anchorMax = pivot = GetNormalizedAnchor(anchor); }
int RectLayout::GetAnchorPreset() const
{
    if (anchorMin != anchorMax || pivot != anchorMin) return -1;
    for (int i = 0; i < 9; ++i) if (anchorMin == GetNormalizedAnchor(RectAnchor(i))) return i;
    return -1;
}
Vector2 RectLayout::GetAnchor(const RectAnchor& anchor, float width, float height) { return GetNormalizedAnchor(anchor) * Vector2(width, height); }
void RectLayout::SetParentAtIndex(Entity* parent, std::size_t index) { Spatial().SetParentAtIndex(parent, index); }
void RectLayout::SetParent(Entity* parent) { Spatial().SetParent(parent); }
void RectLayout::Unparent() { Spatial().Unparent(); }
bool RectLayout::HasParent() const { return Spatial().HasParent(); }
bool RectLayout::HasChildren() const { return Spatial().HasChildren(); }
bool RectLayout::IsChildOf(Entity* parent) const { return Spatial().IsChildOf(parent); }
void RectLayout::AddChild(Entity* child) { Spatial().AddChild(child); }
void RectLayout::RemoveChild(Entity* child) { Spatial().RemoveChild(child); }
void RectLayout::RemoveAllChildren() { Spatial().RemoveAllChildren(); }

float RectLayout::GetCanvasOverlayScaleFactor() const
{
    if (entity == nullptr || !entity->scene.TryGetWindow())
        return 1.0f;

    const Canvas* canvas = FindCanvas();
    if (canvas == nullptr || canvas->renderMode != CanvasRenderMode::SCREEN_SPACE_OVERLAY)
        return 1.0f;

    const float referenceWidth = std::max(1.0f, canvas->screenSize.x);
    const float referenceHeight = std::max(1.0f, canvas->screenSize.y);
    const float screenWidth = static_cast<float>(entity->scene.GetWindow().GetScreenWidth());
    const float screenHeight = static_cast<float>(entity->scene.GetWindow().GetScreenHeight());

    // Preview authored UI as a complete reference canvas in the 2D Scene panel.
    // Runtime width/height scaling still uses the actual Game viewport.
    if (entity->scene.HasEditorCamera2DOverride())
        return std::min(screenWidth / referenceWidth, screenHeight / referenceHeight);

    switch (canvas->scaleMode)
    {
    case CanvasScaleMode::SCALE_WITH_SCREEN_WIDTH:
        return screenWidth / referenceWidth;
    case CanvasScaleMode::SCALE_WITH_SCREEN_HEIGHT:
        return screenHeight / referenceHeight;
    default:
        return 1.0f;
    }
}

Vector2 RectLayout::GetCanvasOverlayLogicalSize() const
{
    if (entity == nullptr || !entity->scene.TryGetWindow())
        return Vector2(0.0f);

    if (entity->scene.HasEditorCamera2DOverride())
        if (const auto* canvas = FindCanvas(); canvas && canvas->renderMode == CanvasRenderMode::SCREEN_SPACE_OVERLAY)
            return glm::max(canvas->screenSize, Vector2(1));

    const float screenWidth = static_cast<float>(entity->scene.GetWindow().GetScreenWidth());
    const float screenHeight = static_cast<float>(entity->scene.GetWindow().GetScreenHeight());
    const float canvasScale = GetCanvasOverlayScaleFactor();

    if (canvasScale == 0.0f)
        return Vector2(screenWidth, screenHeight);

    return Vector2(screenWidth / canvasScale, screenHeight / canvasScale);
}

Vector2 RectLayout::GetNormalizedAnchor(const RectAnchor &_anchor)
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

void Camera2D::Create() { UpdateMatrix(); }

void Camera2D::Destroy() {
    Debug::Log("DestroyCamera");
}

void Camera2D::UpdateMatrix()
{
    if (entity == nullptr || !entity->scene.TryGetWindow())
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
