#pragma once
#include <vector>
#include <algorithm>
#include <stdexcept>

#include <Canis/Math.hpp>
#include <Canis/Asset.hpp>
#include <Canis/AssetHandle.hpp>
#include <Canis/UUID.hpp>
#include <Canis/Data/Types.hpp>
#include <Canis/Data/Bit.hpp>
#include <Canis/External/entt.hpp>
#include <Canis/Scene.hpp>

namespace Canis
{
    class App;
    class Scene;
    class Editor;
    class ScriptableEntity;
    struct ScriptConf;

    class Entity
    {
    friend Scene;
    friend Editor;
    friend App;
    private:
        std::vector<ScriptableEntity*> m_scriptComponents = {};
        entt::entity m_entityHandle = entt::null;

        ScriptableEntity* AddScriptDirect(const ScriptConf& _conf, ScriptableEntity* _scriptableEntity, bool _callCreate = true);
        ScriptableEntity* GetScriptDirect(const ScriptConf& _conf);
        const ScriptableEntity* GetScriptDirect(const ScriptConf& _conf) const;
        void RemoveScriptDirect(const ScriptConf& _conf);
    public:
        int id = -1;
        Scene& scene;
        bool active = true;
        bool editorLocked = false;
        std::string name = "";
        std::string tag = "";

        template <typename Tag> requires requires(Tag value) { ToTagName(value); }
        bool HasTag(Tag value) const { return tag == ToTagName(value); }

        template <typename Tag> requires requires(Tag value) { ToTagName(value); }
        void SetTag(Tag value) { tag = ToTagName(value); }
        UUID uuid;
        
        Entity() = delete;
        explicit Entity(Scene& _scene) : scene(_scene) {}

        entt::entity GetHandle() const
        {
            return m_entityHandle;
        }

        template <typename T, typename... Args>
        T* AddComponent(Args&&... _args)
        {
            if (m_entityHandle == entt::null)
                throw std::runtime_error("Entity::AddComponent called on invalid entity.");

            if (HasComponent<T>())
                return &GetComponent<T>();

            T& component = scene.GetRegistry().emplace<T>(m_entityHandle, std::forward<Args>(_args)...);
            component.entity = this;
            return &component;
        }

        template <typename T, typename... Args>
        T& AddOrReplaceComponent(Args&&... _args)
        {
            if (m_entityHandle == entt::null)
                throw std::runtime_error("Entity::AddOrReplaceComponent called on invalid entity.");

            T& component = scene.GetRegistry().emplace_or_replace<T>(m_entityHandle, std::forward<Args>(_args)...);
            component.entity = this;
            return component;
        }

        template <typename T>
        bool HasComponent() const
        {
            if (m_entityHandle == entt::null)
                return false;

            return scene.GetRegistry().all_of<T>(m_entityHandle);
        }

        template <typename... T>
        bool HasComponents() const
        {
            static_assert(sizeof...(T) > 0, "Entity::HasComponents requires at least one component type.");

            if (m_entityHandle == entt::null)
                return false;

            return scene.GetRegistry().all_of<T...>(m_entityHandle);
        }

        template <typename T>
        T& GetComponent()
        {
            if (m_entityHandle == entt::null)
                throw std::runtime_error("Entity::GetComponent called on invalid entity.");

            if (!HasComponent<T>())
                return *AddComponent<T>();

            return scene.GetRegistry().get<T>(m_entityHandle);
        }

        template <typename T>
        const T& GetComponent() const
        {
            if (m_entityHandle == entt::null)
                throw std::runtime_error("Entity::GetComponent const called on invalid entity.");

            if (!HasComponent<T>())
                throw std::runtime_error("Entity::GetComponent const called for missing component.");

            return scene.GetRegistry().get<T>(m_entityHandle);
        }

        template <typename T>
        void RemoveComponent()
        {
            if (m_entityHandle == entt::null)
                return;

            if (HasComponent<T>())
                scene.GetRegistry().remove<T>(m_entityHandle);
        }

        template <typename T>
        T* AddScript(bool _callCreate = true)
        {
            for (ScriptableEntity* script : m_scriptComponents)
            {
                if (T* scriptableEntity = dynamic_cast<T*>(script))
                    return scriptableEntity;
            }

            T* scriptableEntity = new T(*this);
            m_scriptComponents.push_back(scriptableEntity);

            if (_callCreate)
                scriptableEntity->Create();

            return scriptableEntity;
        }

        template <typename T>
        T* GetScript()
        {
            for (ScriptableEntity* script : m_scriptComponents)
            {
                if (T* scriptableEntity = dynamic_cast<T*>(script))
                    return scriptableEntity;
            }

            return nullptr;
        }

        template <typename T>
        const T* GetScript() const
        {
            for (const ScriptableEntity* script : m_scriptComponents)
            {
                if (const T* scriptableEntity = dynamic_cast<const T*>(script))
                    return scriptableEntity;
            }

            return nullptr;
        }

        template <typename T>
        bool HasScript() const
        {
            for (const ScriptableEntity* script : m_scriptComponents)
            {
                if (const T* scriptableEntity = dynamic_cast<const T*>(script))
                    return true;
            }

            return false;
        }

        template <typename T>
        void RemoveScript()
        {
            for (size_t i = 0; i < m_scriptComponents.size(); ++i)
            {
                if (T* scriptableEntity = dynamic_cast<T*>(m_scriptComponents[i]))
                {
                    scriptableEntity->Destroy();
                    delete scriptableEntity;
                    m_scriptComponents.erase(m_scriptComponents.begin() + i);
                    return;
                }
            }
        }

        ScriptableEntity* AttachScript(const std::string& _scriptName, ScriptableEntity* _scriptableEntity, bool _callCreate = true);
        void RemoveScript(const std::string& _scriptName);
        void RemoveAllScripts();

        void Destroy();
    };

    class ScriptableEntity
    {
    friend Scene;
    private:
        bool m_onReadyCalled = false;
    public:        
        ScriptableEntity(Canis::Entity& _entity) : entity(_entity) {}

        Canis::Entity& entity;
        virtual void Create() {}
        virtual void Ready() {}
        virtual void Destroy() {}
        virtual void Update(float _dt) {}
        virtual bool UpdateWhenPaused() const { return false; }
    };

    enum RectAnchor
	{
		TOPLEFT = 0,
		TOPCENTER = 1,
		TOPRIGHT = 2,
		CENTERLEFT = 3,
		CENTER = 4,
		CENTERRIGHT = 5,
		BOTTOMLEFT = 6,
		BOTTOMCENTER = 7,
		BOTTOMRIGHT = 8
	};

    static const char *RectAnchorLabels[] = {
		"Top Left", "Top Center", "Top Right",
		"Center Left", "Center", "Center Right",
		"Bottom Left", "Bottom Center", "Bottom Right"};

    namespace CanvasRenderMode
    {
        constexpr unsigned int SCREEN_SPACE_OVERLAY = 0u;
        constexpr unsigned int SCREEN_SPACE_CAMERA = 1u;
        constexpr unsigned int WORLD_SPACE = 2u;
    }

    static const char *CanvasRenderModeLabels[] = {
        "Screen Space Overlay",
        "Screen Space Camera",
        "World Space"};

    namespace CanvasScaleMode
    {
        constexpr unsigned int CONSTANT_PIXEL_SIZE = 0u;
        constexpr unsigned int SCALE_WITH_SCREEN_WIDTH = 1u;
        constexpr unsigned int SCALE_WITH_SCREEN_HEIGHT = 2u;
    }

    static const char *CanvasScaleModeLabels[] = {
        "Constant Pixel Size",
        "Scale With Screen Width",
        "Scale With Screen Height"};

    struct Canvas
    {
    public:
        static constexpr const char* ScriptName = "Canis::Canvas";

        Canvas() = default;
        explicit Canvas(Canis::Entity& _entity) : entity(&_entity) {}

        void Create() {}
        Entity* entity = nullptr;

        bool active = true;
        unsigned int renderMode = CanvasRenderMode::SCREEN_SPACE_OVERLAY;
        unsigned int scaleMode = CanvasScaleMode::SCALE_WITH_SCREEN_WIDTH;
        Vector2 screenSize = Vector2(1280.0f, 800.0f);
        bool receivesEvents = true;
        float interactionDistance = 3.0f;
    };

    struct RectTransform
    {
    public:
        static constexpr const char* ScriptName = "Canis::RectTransform";

        RectTransform() = default;


        explicit RectTransform(Canis::Entity& _entity) : entity(&_entity) {}

        void Create() {}
        Entity* entity = nullptr;

        bool active = true;
        Vector2 position = Vector2(0.0f);
        Vector2 size = Vector2(32.0f);
        Vector2 scale = Vector2(1.0f);
        Vector2 anchorMin = Vector2(0.5f, 0.5f);
        Vector2 anchorMax = Vector2(0.5f, 0.5f);
        Vector2 pivot = Vector2(0.5f, 0.5f);
        Vector2 originOffset = Vector2(0.0f);
        float   depth = 0.001f;
        float   rotation = 0.0f;
        Vector2 rotationOriginOffset = Vector2(0.0f);
        Entity*  parent = nullptr;
		std::vector<Entity*> children;

        Vector2 GetPosition() const;

        void SetPosition(Vector2 _globalPos);

        Vector2 GetResolvedSize() const;

        Vector2 GetRectMin() const;

        unsigned int GetCanvasRenderMode() const;
        const Canvas* GetCanvas() const { return FindCanvas(); }

        bool IsActiveInHierarchy() const;

        void SetAnchorPreset(RectAnchor _anchor);

        int GetAnchorPreset() const;

        void Move(Vector2 _delta)
        {
            position = position + _delta;
        }

        float GetRotation() const
		{
            if (parent)
			{
                if (parent->HasComponent<RectTransform>())
                {
                    const RectTransform& parentRT = parent->GetComponent<RectTransform>();
                    return rotation + parentRT.GetRotation();
                }
			}

			return rotation;
		}

        float GetDepth() const
		{
            if (parent)
			{
                if (parent->HasComponent<RectTransform>())
                {
                    const RectTransform& parentRT = parent->GetComponent<RectTransform>();
                    return depth + parentRT.GetDepth();
                }
			}

			return depth;
		}

        void SetDepth(Vector2 _globalDepth)
        {
            if (parent)
            {
                if (parent->HasComponent<RectTransform>())
                {
                    RectTransform& parentRT = parent->GetComponent<RectTransform>();
                    Vector2 parentDepth = parentRT.GetPosition();
                    
                    position = _globalDepth - parentDepth;
                    return;
                }
            }

            position = _globalDepth;
        }
    
        Vector2 GetScale() const
        {
            if (!parent)
            {
                if (GetCanvasRenderMode() == CanvasRenderMode::SCREEN_SPACE_OVERLAY)
                {
                    const float canvasScale = GetCanvasOverlayScaleFactor();
                    return Vector2(scale.x * canvasScale, scale.y * canvasScale);
                }

                return scale;
            }

            if (parent->HasComponent<RectTransform>())
            {
                const RectTransform& parentRT = parent->GetComponent<RectTransform>();
                Vector2 p = parentRT.GetScale();
                return Vector2(p.x * scale.x, p.y * scale.y);
            }

            return scale;
        }

        void SetScale(const Vector2& _globalScale)
        {
            if (parent)
            {
                if (parent->HasComponent<RectTransform>())
                {
                    RectTransform& parentRT = parent->GetComponent<RectTransform>();
                    Vector2 parentScale = parentRT.GetScale();

                    // avoid divide-by-zero
                    scale.x = (parentScale.x != 0.0f) ? (_globalScale.x / parentScale.x) : _globalScale.x;
                    scale.y = (parentScale.y != 0.0f) ? (_globalScale.y / parentScale.y) : _globalScale.y;
                    return;
                }
            }

            if (GetCanvasRenderMode() == CanvasRenderMode::SCREEN_SPACE_OVERLAY)
            {
                const float canvasScale = GetCanvasOverlayScaleFactor();
                scale.x = (canvasScale != 0.0f) ? (_globalScale.x / canvasScale) : _globalScale.x;
                scale.y = (canvasScale != 0.0f) ? (_globalScale.y / canvasScale) : _globalScale.y;
                return;
            }

            scale = _globalScale;
        }

        Vector2 GetRight() const
        {
            const float a = GetRotation();

            Vector2 right(std::cos(a), std::sin(a));

            return glm::normalize(right);
        }

        Vector2 GetUp() const
        {
            const float a = GetRotation();

            Vector2 up(-std::sin(a), std::cos(a));

            return glm::normalize(up);
        }

        bool HasParent() const {
            return parent != nullptr;
        }

        struct LayoutData
        {
            Vector2 min = Vector2(0.0f);
            Vector2 size = Vector2(0.0f);
            Vector2 pivotPosition = Vector2(0.0f);
        };

        LayoutData GetLayout() const;

        void SetParentAtIndex(Entity* newParent, std::size_t index)
        {
            Entity* self = entity;
            if (self == nullptr)
                return;

            // same parent: just reorder within the same children list
            if (parent == newParent)
            {
                if (!parent)
                    return;

                if (parent->HasComponent<RectTransform>())
                {
                    RectTransform& parentRT = parent->GetComponent<RectTransform>();
                    auto& list = parentRT.children;
                    auto it = std::find(list.begin(), list.end(), self);
                    if (it == list.end())
                        return;

                    std::size_t currentIndex = std::distance(list.begin(), it);
                    if (currentIndex == index)
                        return; // nothing to do

                    list.erase(it);

                    index = std::clamp(index, static_cast<size_t>(0), list.size());

                    list.insert(list.begin() + index, self);
                }
                return;
            }

            Vector2 oldPos = GetPosition();

            // different parent: remove from old parent
            if (parent)
            {
                if (parent->HasComponent<RectTransform>())
                {
                    RectTransform& oldParentRT = parent->GetComponent<RectTransform>();
                    auto& list = oldParentRT.children;
                    list.erase(std::remove(list.begin(), list.end(), self), list.end());
                }
            }

            // set new parent pointer
            parent = newParent;

            // insert into new parent's children list at index
            if (newParent)
            {
                if (newParent->HasComponent<RectTransform>())
                {
                    RectTransform& newParentRT = newParent->GetComponent<RectTransform>();
                    auto& list = newParentRT.children;
                    index = std::clamp(index, static_cast<size_t>(0), list.size());

                    list.insert(list.begin() + index, self);
                }
            }

            SetPosition(oldPos);
        }

        void SetParent(Entity* newParent)
        {
            if (newParent && newParent->HasComponent<RectTransform>())
            {
                RectTransform& parentRT = newParent->GetComponent<RectTransform>();
                SetParentAtIndex(newParent, parentRT.children.size());
            }
            else
            {
                // unparent
                Unparent();
            }
        }

        void Unparent()
        {
            SetParentAtIndex(nullptr, 0);
        }

        bool IsChildOf(Entity* potentialParent) const
        {
            return parent == potentialParent;
        }

        bool HasChildren() const {
            return !children.empty();
        }

        void AddChild(Entity* child)
        {
            if (!child) return;
            if (!child->HasComponent<RectTransform>()) return;

            if (entity != nullptr)
            {
                RectTransform& childRT = child->GetComponent<RectTransform>();
                childRT.SetParent(entity);
            }
        }

        void RemoveChild(Entity* child)
        {
            if (!child) return;

            // remove from children list
            children.erase(std::remove(children.begin(), children.end(), child), children.end());

            // clear child's parent
            if (child->HasComponent<RectTransform>())
            {
                RectTransform& childRT = child->GetComponent<RectTransform>();
                childRT.parent = nullptr;
            }
        }

        void RemoveAllChildren()
        {
            for (auto* child : children)
            {
                if (child != nullptr && child->HasComponent<RectTransform>())
                {
                    RectTransform& childRT = child->GetComponent<RectTransform>();
                    childRT.parent = nullptr;
                }
            }
            
            children.clear();
        }

        Vector2 static GetAnchor(const RectAnchor &_anchor, const float &_windowWidth, const float &_windowHeight)
        {
            switch (_anchor)
            {
            case RectAnchor::TOPLEFT:
            {
                return Vector2(0.0f, _windowHeight);
            }
            case RectAnchor::TOPCENTER:
            {
                return Vector2(_windowWidth / 2.0f, _windowHeight);
            }
            case RectAnchor::TOPRIGHT:
            {
                return Vector2(_windowWidth, _windowHeight);
            }
            case RectAnchor::CENTERLEFT:
            {
                return Vector2(0.0f, _windowHeight / 2.0f);
            }
            case RectAnchor::CENTER:
            {
                return Vector2(_windowWidth / 2.0f, _windowHeight / 2.0f);
            }
            case RectAnchor::CENTERRIGHT:
            {
                return Vector2(_windowWidth, _windowHeight / 2.0f);
            }
            case RectAnchor::BOTTOMLEFT:
            {
                return Vector2(0.0f, 0.0f);
            }
            case RectAnchor::BOTTOMCENTER:
            {
                return Vector2(_windowWidth / 2.0f, 0.0f);
            }
            case RectAnchor::BOTTOMRIGHT:
            {
                return Vector2(_windowWidth, 0.0f);
            }
            default:
            {
                return Vector2(0.0f);
            }
            }
        }

    private:
        const Canvas* FindCanvas() const;
        float GetCanvasOverlayScaleFactor() const;
        Vector2 GetCanvasOverlayLogicalSize() const;
        static Vector2 GetNormalizedAnchor(const RectAnchor &_anchor);
    };

    struct Transform
    {
    public:
        static constexpr const char* ScriptName = "Canis::Transform";

        Transform() = default;


        explicit Transform(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}
        void Destroy()
        {
            Unparent();
            RemoveAllChildren();
        }

        bool active = true;
        Vector3 position = Vector3(0.0f);
        Quaternion rotation = Quaternion(Vector3(0.0f));
        Vector3 scale = Vector3(1.0f);
        Entity* parent = nullptr;
        std::vector<Entity*> children = {};

        // Optional runtime-authored space inserted between the parent and this
        // transform's local TRS. Bone attachments use this for the animated
        // bone pose while leaving position/rotation/scale editable as offsets.
        Matrix4 localMatrixPrefix = Matrix4(1.0f);
        bool useLocalMatrixPrefix = false;

        void SetLocalMatrixPrefix(const Matrix4& _matrix)
        {
            localMatrixPrefix = _matrix;
            useLocalMatrixPrefix = true;
        }

        void ClearLocalMatrixPrefix()
        {
            localMatrixPrefix = Matrix4(1.0f);
            useLocalMatrixPrefix = false;
        }

        void Rotate(const Vector3 &_eulerRadians)
        {
            Canis::Rotate(*this, _eulerRadians);
        }

        void LookAt(const Vector3 &_target, const Vector3 &_up = Vector3(0.0f, 1.0f, 0.0f))
        {
            Canis::LookAt(*this, _target, _up);
        }

        Matrix4 GetLocalMatrix() const
        {
            Matrix4 matrix = Matrix4(1.0f);
            matrix = glm::translate(matrix, position);
            matrix *= glm::mat4_cast(glm::normalize(rotation));
            matrix = glm::scale(matrix, scale);
            return useLocalMatrixPrefix ? localMatrixPrefix * matrix : matrix;
        }

        Matrix4 GetModelMatrix() const
        {
            Matrix4 local = GetLocalMatrix();

            if (parent != nullptr)
            {
                if (parent->HasComponent<Transform>())
                {
                    const Transform& parentTransform = parent->GetComponent<Transform>();
                    return parentTransform.GetModelMatrix() * local;
                }
            }

            return local;
        }

        Vector3 GetGlobalPosition() const
        {
            const Matrix4 model = GetModelMatrix();
            const Vector4 world = model * Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            return Vector3(world.x, world.y, world.z);
        }

        Quaternion GetGlobalRotation() const
        {
            const Matrix4 model = GetModelMatrix();
            glm::mat3 rotationMatrix(model);
            for (int column = 0; column < 3; ++column)
            {
                const float length = glm::length(Vector3(rotationMatrix[column]));
                if (length > 0.000001f)
                    rotationMatrix[column] /= length;
            }
            if (glm::determinant(rotationMatrix) < 0.0f)
                rotationMatrix[0] = -rotationMatrix[0];
            return glm::normalize(glm::quat_cast(rotationMatrix));
        }

        Vector3 GetGlobalScale() const
        {
            const Matrix4 model = GetModelMatrix();
            return Vector3(
                glm::length(Vector3(model[0])),
                glm::length(Vector3(model[1])),
                glm::length(Vector3(model[2])));
        }

        Vector3 GetForward() const
        {
            const Matrix4 matrix = GetModelMatrix();
            Vector4 forward4 = matrix * Vector4(0.0f, 0.0f, -1.0f, 0.0f);
            return glm::normalize(Vector3(forward4.x, forward4.y, forward4.z));
        }

        Vector3 GetUp() const
        {
            const Matrix4 matrix = GetModelMatrix();
            Vector4 up4 = matrix * Vector4(0.0f, 1.0f, 0.0f, 0.0f);
            return glm::normalize(Vector3(up4.x, up4.y, up4.z));
        }

        Vector3 GetRight() const
        {
            const Matrix4 matrix = GetModelMatrix();
            Vector4 right4 = matrix * Vector4(1.0f, 0.0f, 0.0f, 0.0f);
            return glm::normalize(Vector3(right4.x, right4.y, right4.z));
        }

        bool HasParent() const
        {
            return parent != nullptr;
        }

        bool IsActiveInHierarchy() const
        {
            if (!active || (entity != nullptr && !entity->active))
                return false;

            if (parent != nullptr && parent->HasComponent<Transform>())
                return parent->GetComponent<Transform>().IsActiveInHierarchy();

            return true;
        }

        void SetParentAtIndex(Entity* newParent, std::size_t index)
        {
            Entity* self = entity;
            if (self == nullptr)
                return;

            if (parent == newParent)
            {
                if (!parent)
                    return;

                if (parent->HasComponent<Transform>())
                {
                    Transform& parentTransform = parent->GetComponent<Transform>();
                    auto& list = parentTransform.children;
                    auto it = std::find(list.begin(), list.end(), self);
                    if (it == list.end())
                        return;

                    std::size_t currentIndex = std::distance(list.begin(), it);
                    if (currentIndex == index)
                        return;

                    list.erase(it);
                    index = std::clamp(index, static_cast<size_t>(0), list.size());
                    list.insert(list.begin() + index, self);
                }
                return;
            }

            const Vector3 oldWorldPosition = GetGlobalPosition();
            const Quaternion oldWorldRotation = GetGlobalRotation();
            const Vector3 oldWorldScale = GetGlobalScale();

            if (parent)
            {
                if (parent->HasComponent<Transform>())
                {
                    Transform& oldParentTransform = parent->GetComponent<Transform>();
                    auto& list = oldParentTransform.children;
                    list.erase(std::remove(list.begin(), list.end(), self), list.end());
                }
            }

            parent = newParent;

            if (newParent)
            {
                if (newParent->HasComponent<Transform>())
                {
                    Transform& newParentTransform = newParent->GetComponent<Transform>();
                    auto& list = newParentTransform.children;
                    index = std::clamp(index, static_cast<size_t>(0), list.size());
                    list.insert(list.begin() + index, self);

                    const Vector3 parentWorldPosition = newParentTransform.GetGlobalPosition();
                    const Quaternion parentWorldRotation = newParentTransform.GetGlobalRotation();
                    const Vector3 parentWorldScale = newParentTransform.GetGlobalScale();

                    Vector3 parentSpacePosition = oldWorldPosition - parentWorldPosition;
                    const Matrix4 inverseParentRotation = glm::mat4_cast(glm::inverse(parentWorldRotation));
                    Vector4 localPosition4 = inverseParentRotation * Vector4(
                        parentSpacePosition.x,
                        parentSpacePosition.y,
                        parentSpacePosition.z,
                        0.0f);

                    position.x = (parentWorldScale.x != 0.0f) ? (localPosition4.x / parentWorldScale.x) : localPosition4.x;
                    position.y = (parentWorldScale.y != 0.0f) ? (localPosition4.y / parentWorldScale.y) : localPosition4.y;
                    position.z = (parentWorldScale.z != 0.0f) ? (localPosition4.z / parentWorldScale.z) : localPosition4.z;
                    rotation = glm::normalize(glm::inverse(parentWorldRotation) * oldWorldRotation);
                    scale.x = (parentWorldScale.x != 0.0f) ? (oldWorldScale.x / parentWorldScale.x) : oldWorldScale.x;
                    scale.y = (parentWorldScale.y != 0.0f) ? (oldWorldScale.y / parentWorldScale.y) : oldWorldScale.y;
                    scale.z = (parentWorldScale.z != 0.0f) ? (oldWorldScale.z / parentWorldScale.z) : oldWorldScale.z;
                    return;
                }
            }

            position = oldWorldPosition;
            rotation = oldWorldRotation;
            scale = oldWorldScale;
        }

        void SetParent(Entity* newParent)
        {
            if (newParent && newParent->HasComponent<Transform>())
            {
                Transform& parentTransform = newParent->GetComponent<Transform>();
                SetParentAtIndex(newParent, parentTransform.children.size());
            }
            else
            {
                Unparent();
            }
        }

        void Unparent()
        {
            SetParentAtIndex(nullptr, 0);
        }

        bool IsChildOf(Entity* potentialParent) const
        {
            return parent == potentialParent;
        }

        bool HasChildren() const
        {
            return !children.empty();
        }

        void AddChild(Entity* child)
        {
            if (!child)
                return;
            if (!child->HasComponent<Transform>())
                return;

            if (entity != nullptr)
            {
                Transform& childTransform = child->GetComponent<Transform>();
                childTransform.SetParent(entity);
            }
        }

        void RemoveChild(Entity* child)
        {
            if (!child)
                return;

            children.erase(std::remove(children.begin(), children.end(), child), children.end());

            if (child->HasComponent<Transform>())
            {
                Transform& childTransform = child->GetComponent<Transform>();
                childTransform.parent = nullptr;
            }
        }

        void RemoveAllChildren()
        {
            for (auto* child : children)
            {
                if (child != nullptr && child->HasComponent<Transform>())
                {
                    Transform& childTransform = child->GetComponent<Transform>();
                    childTransform.parent = nullptr;
                }
            }

            children.clear();
        }
    };

    struct PrefabInstance
    {
    public:
        static constexpr const char* ScriptName = "Canis::PrefabInstance";

        PrefabInstance() = default;
        explicit PrefabInstance(Canis::Entity& _entity) : entity(&_entity) {}

        void Create() {}

        Entity* entity = nullptr;
        SceneAssetHandle prefab = {};
        Entity* firstEntity = nullptr;
    };

    namespace RigidbodyMotionType
    {
        constexpr int STATIC = 0;
        constexpr int KINEMATIC = 1;
        constexpr int DYNAMIC = 2;
    }

    namespace Rigidbody3DForceMode
    {
        // Applies a continuous force. Acceleration depends on the rigidbody's mass.
        constexpr int FORCE = 0;

        // Applies continuous acceleration directly, ignoring the rigidbody's mass.
        constexpr int ACCELERATION = 1;

        // Applies an instantaneous impulse. The velocity change depends on mass.
        constexpr int IMPULSE = 2;

        // Applies an instantaneous velocity change directly, ignoring mass.
        constexpr int VELOCITY_CHANGE = 3;
    }

    struct Rigidbody
    {
    public:
        static constexpr const char* ScriptName = "Canis::Rigidbody";
        static constexpr Mask DefaultLayer = Mask(1u);
        static constexpr Mask DefaultMask = Mask(u32_max);

        Rigidbody() = default;
        explicit Rigidbody(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}

        void AddForce(const Vector3& _force, int _forceMode = Rigidbody3DForceMode::FORCE)
        {
            if (!active || motionType != RigidbodyMotionType::DYNAMIC)
                return;

            switch (_forceMode)
            {
            case Rigidbody3DForceMode::ACCELERATION:
                pendingAcceleration += _force;
                break;
            case Rigidbody3DForceMode::IMPULSE:
                pendingImpulse += _force;
                break;
            case Rigidbody3DForceMode::VELOCITY_CHANGE:
                pendingVelocityChange += _force;
                break;
            case Rigidbody3DForceMode::FORCE:
            default:
                pendingForce += _force;
                break;
            }
        }

        void SetLinearVelocity(const Vector3 &_velocity)
        {
            if (!active || motionType != RigidbodyMotionType::DYNAMIC)
                return;

            pendingLinearVelocity = _velocity;
            hasPendingLinearVelocity = true;
        }

        void SetAngularVelocity(const Vector3 &_velocity)
        {
            if (!active || motionType != RigidbodyMotionType::DYNAMIC)
                return;

            pendingAngularVelocity = _velocity;
            hasPendingAngularVelocity = true;
        }

        void Move(const Vector3 &_translation)
        {
            if (!active || motionType != RigidbodyMotionType::KINEMATIC)
                return;

            pendingTranslation += _translation;
        }

        void StopKinematicMotion()
        {
            if (!active || motionType != RigidbodyMotionType::KINEMATIC)
                return;
            pendingTranslation = Vector3(0.0f);
            pendingRotation = Quaternion(Vector3(0.0f));
            hasPendingKinematicStop = true;
        }

        void Rotate(const Vector3 &_eulerRadians)
        {
            if (!active || motionType != RigidbodyMotionType::KINEMATIC)
                return;

            pendingRotation = glm::normalize(Quaternion(_eulerRadians) * pendingRotation);
        }

        bool active = true;
        int motionType = RigidbodyMotionType::DYNAMIC;
        float mass = 1.0f;
        float friction = 0.2f;
        float restitution = 0.0f;
        float linearDamping = 0.05f;
        float angularDamping = 0.05f;
        bool useGravity = true;
        float gravityFactor = 1.0f;
        bool isSensor = false;
        Mask layer = DefaultLayer;
        Mask mask = DefaultMask;
        bool allowSleeping = true;
        bool lockRotationX = false;
        bool lockRotationY = false;
        bool lockRotationZ = false;
        Vector3 linearVelocity = Vector3(0.0f);
        Vector3 angularVelocity = Vector3(0.0f);
        Vector3 pendingForce = Vector3(0.0f);
        Vector3 pendingAcceleration = Vector3(0.0f);
        Vector3 pendingImpulse = Vector3(0.0f);
        Vector3 pendingVelocityChange = Vector3(0.0f);
        Vector3 pendingLinearVelocity = Vector3(0.0f);
        bool hasPendingLinearVelocity = false;
        Vector3 pendingAngularVelocity = Vector3(0.0f);
        bool hasPendingAngularVelocity = false;
        bool hasPendingKinematicStop = false;
        Vector3 pendingTranslation = Vector3(0.0f);
        Quaternion pendingRotation = Quaternion(Vector3(0.0f));
    };

    struct BoxCollider
    {
    public:
        static constexpr const char* ScriptName = "Canis::BoxCollider";

        BoxCollider() = default;
        explicit BoxCollider(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}

        bool active = true;
        Vector3 offset = Vector3(0.0f);
        Vector3 size = Vector3(1.0f);
        std::vector<Entity*> entered = {};
        std::vector<Entity*> exited = {};
        std::vector<Entity*> stayed = {};
    };

    struct SphereCollider
    {
    public:
        static constexpr const char* ScriptName = "Canis::SphereCollider";

        SphereCollider() = default;
        explicit SphereCollider(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}

        bool active = true;
        Vector3 offset = Vector3(0.0f);
        float radius = 0.5f;
        std::vector<Entity*> entered = {};
        std::vector<Entity*> exited = {};
        std::vector<Entity*> stayed = {};
    };

    struct CapsuleCollider
    {
    public:
        static constexpr const char* ScriptName = "Canis::CapsuleCollider";

        CapsuleCollider() = default;
        explicit CapsuleCollider(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}

        bool active = true;
        Vector3 offset = Vector3(0.0f);
        float halfHeight = 0.5f;
        float radius = 0.25f;
        std::vector<Entity*> entered = {};
        std::vector<Entity*> exited = {};
        std::vector<Entity*> stayed = {};
    };

    struct MeshCollider
    {
    public:
        static constexpr const char* ScriptName = "Canis::MeshCollider";

        MeshCollider() = default;
        explicit MeshCollider(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}

        bool active = true;
        bool useAttachedModel = true;
        i32 modelId = -1;
        std::string modelPath = "";
        std::vector<Entity*> entered = {};
        std::vector<Entity*> exited = {};
        std::vector<Entity*> stayed = {};
    };

    struct ConvexMeshCollider
    {
    public:
        static constexpr const char* ScriptName = "Canis::ConvexMeshCollider";

        ConvexMeshCollider() = default;
        explicit ConvexMeshCollider(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}

        bool active = true;
        bool useAttachedModel = true;
        i32 modelId = -1;
        std::string modelPath = "";
        Vector3 offset = Vector3(0.0f);
        Vector3 scale = Vector3(1.0f);
        float convexRadius = 0.05f;
        std::vector<Entity*> entered = {};
        std::vector<Entity*> exited = {};
        std::vector<Entity*> stayed = {};
    };

    struct NavMeshSurface
    {
    public:
        static constexpr const char* ScriptName = "Canis::NavMeshSurface";

        NavMeshSurface() = default;
        explicit NavMeshSurface(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}
        void MarkDirty() { ++revision; }

        bool active = true;
        bool buildOnReady = true;
        bool showDebug = true;
        bool meshCollidersOnly = true;
        Vector3 size = Vector3(48.0f, 6.0f, 32.0f);
        float cellSize = 1.5f;
        float agentRadius = 0.5f;
        float agentHeight = 1.8f;
        float maxFloorDelta = 0.65f;
        Mask collisionMask = Rigidbody::DefaultMask;
        u64 revision = 1u;
    };

    // A three-dimensional navigation volume for flying agents. Points are
    // sampled throughout the box and connected when a spherical agent has a
    // clear path between them.
    struct CloudNavSurface
    {
    public:
        static constexpr const char* ScriptName = "Canis::CloudNavSurface";

        CloudNavSurface() = default;
        explicit CloudNavSurface(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}
        void MarkDirty() { ++revision; }

        bool active = true;
        bool buildOnReady = true;
        bool showDebug = true;
        Vector3 size = Vector3(48.0f, 8.0f, 48.0f);
        float nodeSpacing = 3.0f;
        float agentRadius = 0.5f;
        Mask collisionMask = Rigidbody::DefaultMask;
        u64 revision = 1u;
    };

    struct Terrain
    {
    public:
        static constexpr const char* ScriptName = "Canis::Terrain";

        Terrain() = default;
        explicit Terrain(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}

        TerrainAssetHandle terrain = {};
        i32 runtimeModelId = -1;
        u64 runtimeRevision = 0u;
    };

    namespace BlockoutShapeType
    {
        constexpr int BOX = 0;
        constexpr int PLANE = 1;
        constexpr int RAMP = 2;
        constexpr int CYLINDER = 3;
        constexpr int STAIRS = 4;
    }

    // Indices are stable IDs: edits append vertices/faces and leave removed face slots empty.
    struct BlockoutFace
    {
        std::vector<u32> vertices;
        std::vector<Vector2> uv;
    };

    struct EditableBlockoutMesh
    {
        std::vector<Vector3> vertices;
        std::vector<BlockoutFace> faces;
    };

    struct BlockoutShape
    {
    public:
        static constexpr const char* ScriptName = "Canis::BlockoutShape";

        BlockoutShape() = default;
        explicit BlockoutShape(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create() {}
        void MarkDirty() { ++revision; }

        bool active = true;
        int type = BlockoutShapeType::BOX;
        Vector3 size = Vector3(1.0f);
        int sides = 16;
        int stepCount = 6;
        Vector2 uvScale = Vector2(1.0f);
        int loopCutsX = 0;
        int loopCutsY = 0;
        int loopCutsZ = 0;
        Vector3 extrudeNegative = Vector3(0.0f);
        Vector3 extrudePositive = Vector3(0.0f);
        bool meshEdited = false;
        EditableBlockoutMesh editMesh;
        bool generateCollision = true;
        bool visibleInGame = true;
        bool castShadow = true;
        u64 revision = 1u;

        // Runtime geometry is rebuilt from the serialized parameters.
        i32 runtimeModelId = -1;
        u64 runtimeRevision = 0u;
    };

    struct Camera
    {
    public:
        static constexpr const char* ScriptName = "Canis::Camera";

        Camera() = default;


        explicit Camera(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}


        bool primary = true;
        float fovDegrees = 60.0f;
        float nearClip = 0.1f;
        float farClip = 1000.0f;
    };

    struct DirectionalLight
    {
    public:
        static constexpr const char* ScriptName = "Canis::DirectionalLight";

        DirectionalLight() = default;


        explicit DirectionalLight(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}


        bool enabled = true;
        Color color = Color(1.0f);
        float intensity = 1.0f;
        Vector3 direction = Vector3(-0.4f, -1.0f, -0.25f);
    };

    struct PointLight
    {
    public:
        static constexpr const char* ScriptName = "Canis::PointLight";

        PointLight() = default;


        explicit PointLight(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}


        bool enabled = true;
        Color color = Color(1.0f);
        float intensity = 1.2f;
        float range = 12.0f;
    };

    struct Model
    {
    public:
        static constexpr const char* ScriptName = "Canis::Model";

        Model() = default;


        explicit Model(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}


        i32 modelId = -1;
        i32 nodeIndex = -1;
        bool applyNodeTransform = true;
        bool staticModel = false;
        bool castShadow = true;
        Color color = Color(1.0f);
    };

    struct Material
    {
    public:
        static constexpr const char* ScriptName = "Canis::Material";

        Material() = default;


        explicit Material(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}


        i32 materialId = -1;
        std::vector<i32> materialIds = {};
        Color color = Color(1.0f);
        MaterialFields materialFields = {};
    };

    struct ModelAnimation
    {
    public:
        static constexpr const char* ScriptName = "Canis::ModelAnimation";

        ModelAnimation() = default;


        explicit ModelAnimation(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}


        bool playAnimation = true;
        bool loop = true;
        float animationSpeed = 1.0f;
        float animationTime = 0.0f;
        i32 animationIndex = 0;
        // Locks selected animated skeleton-root translation axes to their
        // first-frame values. A value of 1 removes motion on that axis.
        Vector3 rootMotionMask = Vector3(0.0f);
        // Optional short crossfade from a previously sampled compatible
        // skeleton pose into the current animation.
        std::vector<Matrix4> transitionLocalNodeMatrices = {};
        float transitionDuration = 0.0f;
        float transitionElapsed = 0.0f;

        // Animator controllers may sample a compatible animation from another
        // imported model while the renderer keeps its original mesh/material.
        // A negative value samples animations from the attached Model.
        i32 sourceModelId = -1;

        // Runtime pose cache for this entity's model instance.
        i32 poseModelId = -1;
        u64 poseGeometryRevision = 0u;
        ModelAsset::Pose3D pose = {};

        // Runtime caching to skip redundant animation evaluation.
        bool poseInitialized = false;
        i32 lastEvaluatedAnimationIndex = -1;
        float lastEvaluatedAnimationTime = 0.0f;
        Vector3 lastEvaluatedRootMotionMask = Vector3(0.0f);
    };

    struct AnimationPlayer
    {
    public:
        static constexpr const char* ScriptName = "Canis::AnimationPlayer";

        AnimationPlayer() = default;
        explicit AnimationPlayer(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}

        AnimationClipAssetHandle clip = {};
        bool playing = true;
        bool loop = true;
        float speed = 1.0f;
        float time = 0.0f;

        float lastEventSampleTime = 0.0f;
        bool lastEventSampleValid = false;
        std::string lastEventClipPath = "";
    };

    struct AnimatorParameterRuntime
    {
        std::string name = "";
        AnimationValue value = AnimationValue::Float(0.0f);
        bool triggerActive = false;
    };

    struct Animator
    {
    public:
        static constexpr const char* ScriptName = "Canis::Animator";

        Animator() = default;
        explicit Animator(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}

        AnimatorParameterRuntime* GetParameter(const std::string& _name)
        {
            for (AnimatorParameterRuntime& parameter : parameters)
            {
                if (parameter.name == _name)
                    return &parameter;
            }

            return nullptr;
        }

        const AnimatorParameterRuntime* GetParameter(const std::string& _name) const
        {
            for (const AnimatorParameterRuntime& parameter : parameters)
            {
                if (parameter.name == _name)
                    return &parameter;
            }

            return nullptr;
        }

        AnimatorParameterRuntime& GetOrCreateParameter(const std::string& _name)
        {
            if (AnimatorParameterRuntime* parameter = GetParameter(_name))
                return *parameter;

            AnimatorParameterRuntime parameter = {};
            parameter.name = _name;
            parameters.push_back(parameter);
            return parameters.back();
        }

        void SetFloat(const std::string& _name, float _value)
        {
            if (_name.empty())
                return;
            GetOrCreateParameter(_name).value = AnimationValue::Float(_value);
        }

        void SetInt(const std::string& _name, int _value)
        {
            if (_name.empty())
                return;
            GetOrCreateParameter(_name).value = AnimationValue::Int(_value);
        }

        void SetBool(const std::string& _name, bool _value)
        {
            if (_name.empty())
                return;
            GetOrCreateParameter(_name).value = AnimationValue::Bool(_value);
        }

        void SetTrigger(const std::string& _name)
        {
            if (_name.empty())
                return;
            AnimatorParameterRuntime& parameter = GetOrCreateParameter(_name);
            parameter.triggerActive = true;
            parameter.value = AnimationValue::Bool(true);
        }

        void ResetTrigger(const std::string& _name)
        {
            if (AnimatorParameterRuntime* parameter = GetParameter(_name))
            {
                parameter->triggerActive = false;
                parameter->value = AnimationValue::Bool(false);
            }
        }

        float GetFloat(const std::string& _name, float _default = 0.0f) const
        {
            if (const AnimatorParameterRuntime* parameter = GetParameter(_name))
                return parameter->value.AsFloat(_default);

            return _default;
        }

        int GetInt(const std::string& _name, int _default = 0) const
        {
            if (const AnimatorParameterRuntime* parameter = GetParameter(_name))
                return parameter->value.AsInt(_default);

            return _default;
        }

        bool GetBool(const std::string& _name, bool _default = false) const
        {
            if (const AnimatorParameterRuntime* parameter = GetParameter(_name))
                return parameter->value.AsBool(_default);

            return _default;
        }

        AnimatorControllerAssetHandle controller = {};
        bool playing = true;
        std::string currentState = "";
        float time = 0.0f;
        std::vector<AnimatorParameterRuntime> parameters = {};

        bool parametersInitialized = false;
        bool drivesModelAnimation = false;
        std::string appliedState = "";
        float lastEventSampleTime = 0.0f;
        bool lastEventSampleValid = false;
        std::string lastEventClipPath = "";
    };

    struct Sprite2D
    {
    public:
        static constexpr const char* ScriptName = "Canis::Sprite2D";

        Sprite2D() = default;


        explicit Sprite2D(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}


        void GetSpriteFromTextureAtlas(u8 _offsetX, u8 _offsetY, u16 _indexX, u16 _indexY, u16 _spriteWidth, u16 _spriteHeight)
        {
            uv.x = (flipX) ? (((_indexX+1) * _spriteWidth) + _offsetX)/(f32)textureHandle.texture.width : (_indexX == 0) ? 0.0f : ((_indexX * _spriteWidth) + _offsetX)/(f32)textureHandle.texture.width;
            uv.y = (flipY) ? (((_indexY+1) * _spriteHeight) + _offsetY)/(f32)textureHandle.texture.height : (_indexY == 0) ? 0.0f : ((_indexY * _spriteHeight) + _offsetY)/(f32)textureHandle.texture.height;
            uv.z = (flipX) ? (_spriteWidth*-1.0f)/(float)textureHandle.texture.width : _spriteWidth/(float)textureHandle.texture.width;
            uv.w = (flipY) ? (_spriteHeight*-1.0f)/(float)textureHandle.texture.height : _spriteHeight/(float)textureHandle.texture.height;
        }

        TextureHandle textureHandle;
        Color   color = Color(1.0f);
        Vector4 uv = Vector4(0.0f, 0.0f, 1.0f, 1.0f);
        bool flipX = false;
        bool flipY = false;
    };

    namespace TextAlignment
    {
        constexpr unsigned int LEFT = 0u;
        constexpr unsigned int RIGHT = 1u;
        constexpr unsigned int CENTER = 2u;
    }

    namespace TextBoundary
    {
        constexpr unsigned int TB_OVERFLOW = 0u;
        constexpr unsigned int WRAP = 1u;
    }

    struct Text
    {
    public:
        static constexpr const char* ScriptName = "Canis::Text";

        Text() = default;
        explicit Text(Canis::Entity &_entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}


        void SetText(const std::string &_text)
        {
            if (text == _text)
                return;

            text = _text;
            _status |= BIT::ONE;
        }

        i32 assetId = -1;
        std::string text = "";
        Color color = Color(1.0f);
        unsigned int alignment = TextAlignment::LEFT;
        unsigned int horizontalBoundary = TextBoundary::TB_OVERFLOW;
        unsigned int _status = BIT::ONE;
    };

    struct UIButton
    {
    public:
        static constexpr const char* ScriptName = "Canis::UIButton";

        UIButton() = default;
        explicit UIButton(Canis::Entity &_entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}

        bool active = true;
        Entity* targetEntity = nullptr;
        std::string targetScript = "";
        std::string actionName = "";
        
        Color baseColor = Color(1.0f); // set the first update in UIInteractionSystem
        Color hoverColor = Color(1.0f);
        Color pressedColor = Color(0.85f, 0.85f, 0.85f, 1.0f);
        
        float baseScale = 1.0f; // set the first update in UIInteractionSystem
        float hoverScale = 1.03f;
        float pressedScale = 0.98f;
        bool hovered = false;
        bool pressed = false;
        bool baseValuesSaved = false;
    };

    struct UIInputField
    {
    public:
        static constexpr const char* ScriptName = "Canis::UIInputField";

        UIInputField() = default;
        explicit UIInputField(Canis::Entity &_entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}

        bool active = true;
        Entity* displayEntity = nullptr;
        Entity* targetEntity = nullptr;
        std::string targetScript = "";
        std::string targetProperty = "";
        std::string text = "";
        std::string placeholder = "";
        std::string allowedCharacters = "";
        int maxLength = 64;

        Color baseColor = Color(1.0f);
        Color hoverColor = Color(0.95f, 0.95f, 0.95f, 1.0f);
        Color focusedColor = Color(1.0f);
        Color baseTextColor = Color(1.0f);
        Color textColor = Color(1.0f);
        Color placeholderColor = Color(0.7f, 0.7f, 0.7f, 1.0f);

        bool hovered = false;
        bool focused = false;
        bool baseValuesSaved = false;
        float caretBlinkTimer = 0.0f;
        bool caretVisible = true;
    };

    struct UIDragSource
    {
    public:
        static constexpr const char* ScriptName = "Canis::UIDragSource";

        UIDragSource() = default;
        explicit UIDragSource(Canis::Entity &_entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}

        bool active = true;
        std::string payloadType = "";
        std::string payloadValue = "";
        bool dragging = false;
        Vector2 dragOffset = Vector2(0.0f);
        Vector2 originalPosition = Vector2(0.0f);
        float originalDepth = 0.0f;
    };

    struct UIDropTarget
    {
    public:
        static constexpr const char* ScriptName = "Canis::UIDropTarget";

        UIDropTarget() = default;
        explicit UIDropTarget(Canis::Entity &_entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        void Create() {}

        bool active = true;
        Entity* targetEntity = nullptr;
        std::string targetScript = "";
        std::string actionName = "";
        std::string acceptedPayloadType = "";
        Color baseColor = Color(1.0f);
        Color hoverColor = Color(1.0f);
        bool hovered = false;
    };

    struct Camera2D
    {
    public:
        static constexpr const char* ScriptName = "Canis::Camera2D";

        Camera2D() = default;
        explicit Camera2D(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;

        void Create();
        void Destroy();

        void Update(float _dt);

        void SetPosition(const Vector2 &newPosition)
        {
            m_position = newPosition;
            UpdateMatrix();
        }
        void SetScale(float newScale)
        {
            m_scale = newScale;
            UpdateMatrix();
        }


        Vector2 GetPosition() const { return m_position; }
        Matrix4 GetCameraMatrix() const { return m_cameraMatrix; }
        Matrix4 GetViewMatrix() const { return m_view; }
        Matrix4 GetProjectionMatrix() const { return m_projection; }
        float GetScale() const { return m_scale; }
        void UpdateMatrix();


    private:
        int m_screenWidth = 500;
        int m_screenHeight = 500;
        bool m_needsMatrixUpdate = true;
        float m_scale = 1.0f;
        Vector2 m_position = Vector2(0.0f);
        Matrix4 m_cameraMatrix = Matrix4(1.0f);
        Matrix4 m_view = Matrix4(1.0f);
        Matrix4 m_projection = Matrix4(1.0f);
    };

    struct SpriteAnimation
    {
    public:
        static constexpr const char* ScriptName = "Canis::SpriteAnimation";

        SpriteAnimation() = default;


        explicit SpriteAnimation(Canis::Entity& _entity) : entity(&_entity) {}
        Entity* entity = nullptr;
        ~SpriteAnimation() {}

        void Create() {}
        void Destroy() {}
        void Update(float _dt) {}

        void Play(std::string _path);

        void Pause()
        {
            speed = 0.0f;
        }

        i32 id = 0u;
        f32 speed = 1.0f;

        // hidden in editor
        f32 countDown = 0.0f;
        u16 index = 0u;
        bool redraw = true;
    private:
    };
}
