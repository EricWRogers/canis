#pragma once
#include <vector>
#include <algorithm>
#include <stdexcept>

#include <Canis/Math.hpp>
#include <Canis/Asset.hpp>
#include <Canis/AssetHandle.hpp>
#include <Canis/UUID.hpp>
#include <Canis/Tag.hpp>
#include <Canis/ScriptHandle.hpp>
#include <memory>
#include <Canis/Data/Types.hpp>
#include <Canis/Data/Bit.hpp>
#include <Canis/External/entt.hpp>

namespace Canis
{
    class App;
    class Scene;
    class Editor;
    class ScriptableEntity;
    struct ScriptConf;

    struct EntityMetadata
    {
        static constexpr bool in_place_delete = true;
        UUID uuid;
        std::string name;
        TagId tag = NoTag;
        bool active = true, editorLocked = false;
    };
    struct ScriptComponent
    {
        static constexpr bool in_place_delete = true;
        std::vector<ScriptableEntity*> instances;
    };

    // Non-owning wrapper. The referenced Scene must outlive this value.
    class Entity
    {
    friend Scene;
    friend Editor;
    friend App;
    friend class ScriptableEntity;
    private:
        static Scene& EmptyScene();
        entt::registry& Registry() const;
        ScriptableEntity* InvokeScriptCreate(ScriptableEntity* script);
        Entity* CanonicalEntity() const;
        const std::vector<ScriptableEntity*>& ScriptInstances() const;
        void StoreScriptInstance(ScriptableEntity* script);
        entt::entity m_entityHandle = entt::null;

        ScriptableEntity* AddScriptDirect(const ScriptConf& _conf, ScriptableEntity* _scriptableEntity, bool _callCreate = true);
        ScriptableEntity* GetScriptDirect(const ScriptConf& _conf);
        const ScriptableEntity* GetScriptDirect(const ScriptConf& _conf) const;
        void RemoveScriptDirect(const ScriptConf& _conf);
        void RemoveScriptInstance(ScriptableEntity* script);
    public:
        static constexpr bool in_place_delete = true;
        Scene& scene;
        Entity();
        Entity(std::nullptr_t);
        Entity(Entity* entity);
        Entity(Scene& owner, entt::entity handle) : m_entityHandle(handle), scene(owner) {}
        Entity(entt::entity handle, Scene& owner) : Entity(owner, handle) {}
        Entity(const Entity&) = default;
        Entity& operator=(const Entity& other);
        Entity& operator=(std::nullptr_t) { Reset(); return *this; }
        void Reset() { m_entityHandle = entt::null; }
        Entity* TryGet() const;
        Entity* ResolveIncludingPending() const;
        operator Entity*() const { return TryGet(); }
        Entity* operator->() const;
        Entity& operator*() const { return *operator->(); }
        bool operator==(const Entity& other) const { return &scene == &other.scene && m_entityHandle == other.m_entityHandle; }
        bool operator==(Entity* other) const { return other ? *this == *other : !IsValid(); }
        bool operator==(const Entity* other) const { return other ? *this == *other : !IsValid(); }
        bool operator==(std::nullptr_t) const { return !IsValid(); }

        bool IsValid() const;
        explicit operator bool() const { return IsValid(); }

        EntityMetadata& GetMetadata() { return GetComponent<EntityMetadata>(); }
        const EntityMetadata& GetMetadata() const { return GetComponent<EntityMetadata>(); }
        std::string& GetName() { return GetMetadata().name; }
        const std::string& GetName() const { return GetMetadata().name; }
        void SetName(std::string value) { GetName() = std::move(value); }
        UUID GetUUID() const;
        void SetUUID(UUID value) { GetMetadata().uuid = value; }
        TagId& GetTag() { return GetMetadata().tag; }
        TagId GetTag() const { return GetMetadata().tag; }
        bool& Active() { return GetMetadata().active; }
        bool Active() const { return GetMetadata().active; }
        bool IsActive() const { return IsValid() && Active(); }
        void SetActive(bool value) { Active() = value; }
        bool& EditorLocked() { return GetMetadata().editorLocked; }
        bool EditorLocked() const { return GetMetadata().editorLocked; }

        template <typename Tag> requires requires(Tag value) { ToTagId(value); ToTagName(value); }
        bool HasTag(Tag value) const { return GetTag() == ToTagId(value); }
        template <typename Tag> requires requires(Tag value) { ToTagId(value); ToTagName(value); }
        void SetTag(Tag value) { SetTag(ToTagName(value)); }
        bool HasTag(TagId value) const { return GetTag() == value; }
        bool HasTag(std::string_view value) const { return GetTag() == TagIdFromName(value); }
        void SetTag(std::string_view value) { GetTag() = RegisterTag(value); }
        const std::string& GetTagName() const { return Canis::GetTagName(GetTag()); }

        template<class T> T* TryGetComponent() {
            return IsValid() ? Registry().try_get<T>(m_entityHandle) : nullptr;
        }
        template<class T, class... Args> T& GetOrAddComponent(Args&&... args) {
            if (auto* component = TryGetComponent<T>()) return *component;
            return *AddComponent<T>(std::forward<Args>(args)...);
        }

        entt::entity GetHandle() const
        {
            return m_entityHandle;
        }

        template <typename T, typename... Args>
        T* AddComponent(Args&&... _args)
        {
            if (m_entityHandle == entt::null || !Registry().valid(m_entityHandle))
                throw std::runtime_error("Entity::AddComponent called on invalid entity.");

            if (HasComponent<T>())
                return &GetComponent<T>();

            T& component = Registry().emplace<T>(m_entityHandle, std::forward<Args>(_args)...);
            if constexpr (requires { component.entity = this; }) component.entity = CanonicalEntity();
            return &component;
        }

        template <typename T, typename... Args>
        T& AddOrReplaceComponent(Args&&... _args)
        {
            static_assert(!std::is_same_v<T, Entity> && !std::is_same_v<T, EntityMetadata> && !std::is_same_v<T, ScriptComponent>,
                "Entity records, metadata, and script collections are owned by Scene.");
            if (m_entityHandle == entt::null || !Registry().valid(m_entityHandle))
                throw std::runtime_error("Entity::AddOrReplaceComponent called on invalid entity.");

            T& component = Registry().emplace_or_replace<T>(m_entityHandle, std::forward<Args>(_args)...);
            if constexpr (requires { component.entity = this; }) component.entity = CanonicalEntity();
            return component;
        }

        template <typename T>
        bool HasComponent() const
        {
            if (m_entityHandle == entt::null || !Registry().valid(m_entityHandle))
                return false;

            return Registry().all_of<T>(m_entityHandle);
        }

        template <typename... T>
        bool HasComponents() const
        {
            static_assert(sizeof...(T) > 0, "Entity::HasComponents requires at least one component type.");

            if (m_entityHandle == entt::null || !Registry().valid(m_entityHandle))
                return false;

            return Registry().all_of<T...>(m_entityHandle);
        }

        template <typename T>
        T& GetComponent()
        {
            if (m_entityHandle == entt::null || !Registry().valid(m_entityHandle))
                throw std::runtime_error("Entity::GetComponent called on invalid entity.");

            if (!HasComponent<T>())
                throw std::runtime_error("Entity::GetComponent called for missing component; use GetOrAddComponent to create it.");

            return Registry().get<T>(m_entityHandle);
        }

        template <typename T>
        const T& GetComponent() const
        {
            if (m_entityHandle == entt::null || !Registry().valid(m_entityHandle))
                throw std::runtime_error("Entity::GetComponent const called on invalid entity.");

            if (!HasComponent<T>())
                throw std::runtime_error("Entity::GetComponent const called for missing component.");

            return Registry().get<T>(m_entityHandle);
        }

        template <typename T>
        void RemoveComponent()
        {
            static_assert(!std::is_same_v<T, Entity> && !std::is_same_v<T, EntityMetadata> && !std::is_same_v<T, ScriptComponent>,
                "Entity records, metadata, and script collections are owned by Scene.");
            if (m_entityHandle == entt::null || !Registry().valid(m_entityHandle))
                return;

            if (HasComponent<T>())
                Registry().remove<T>(m_entityHandle);
        }

        template <typename T>
        T* AddScript(bool _callCreate = true)
        {
            if (!IsValid()) throw std::runtime_error("Cannot attach a script to a destroyed entity");
            for (ScriptableEntity* script : ScriptInstances())
            {
                if (T* scriptableEntity = dynamic_cast<T*>(script))
                    return scriptableEntity;
            }

            T* scriptableEntity = new T(*this);
            StoreScriptInstance(scriptableEntity);

            if (_callCreate)
                return dynamic_cast<T*>(InvokeScriptCreate(scriptableEntity));

            return scriptableEntity;
        }

        template <typename T>
        T* GetScript()
        {
            for (ScriptableEntity* script : ScriptInstances())
            {
                if (T* scriptableEntity = dynamic_cast<T*>(script))
                    return scriptableEntity;
            }

            return nullptr;
        }

        template <typename T>
        const T* GetScript() const
        {
            for (const ScriptableEntity* script : ScriptInstances())
            {
                if (const T* scriptableEntity = dynamic_cast<const T*>(script))
                    return scriptableEntity;
            }

            return nullptr;
        }

        template <typename T>
        bool HasScript() const
        {
            for (const ScriptableEntity* script : ScriptInstances())
            {
                if (const T* scriptableEntity = dynamic_cast<const T*>(script))
                    return true;
            }

            return false;
        }

        template <typename T>
        void RemoveScript()
        {
            for (size_t i = 0; i < ScriptInstances().size(); ++i)
            {
                if (T* scriptableEntity = dynamic_cast<T*>(ScriptInstances()[i]))
                {
                    RemoveScriptInstance(scriptableEntity);
                    return;
                }
            }
        }

        template<class T> ScriptHandle<T> AddScriptHandle(bool callCreate = true) { return ScriptHandle<T>(AddScript<T>(callCreate)); }
        template<class T> ScriptHandle<T> GetScriptHandle() { return ScriptHandle<T>(GetScript<T>()); }
        template<class T> void RemoveScript(ScriptHandle<T> script) { if (auto* instance = script.TryGet()) RemoveScriptInstance(instance); }
        std::vector<ScriptHandle<ScriptableEntity>> GetScripts() const;

        ScriptableEntity* AttachScript(const std::string& _scriptName, ScriptableEntity* _scriptableEntity, bool _callCreate = true);
        void RemoveScript(const std::string& _scriptName);
        void RemoveAllScripts();

        void Destroy();
    };

    class ScriptableEntity
    {
    friend Scene;
    friend Entity;
    template<class T> friend class ScriptHandle;
    private:
        std::shared_ptr<ScriptLifetime> m_lifetime = std::make_shared<ScriptLifetime>();
        bool m_onReadyCalled = false;
    public:        
        ScriptableEntity(Canis::Entity& _entity) : entity(*_entity.CanonicalEntity()) { m_lifetime->script = this; m_lifetime->owner = &entity; }

        Canis::Entity& entity;
        virtual ~ScriptableEntity() = default;
        virtual void Create() {}
        virtual void Ready() {}
        virtual void Destroy() {}
        virtual void Update(float _dt) {}
        virtual bool UpdateWhenPaused() const { return false; }
    };

}

namespace Canis {
template<class T> ScriptHandle<T>::ScriptHandle(T* script) {
    if (auto* base = dynamic_cast<ScriptableEntity*>(script)) m_lifetime = base->m_lifetime;
}
template<class T> T* ScriptHandle<T>::TryGet() const {
    auto lifetime = m_lifetime.lock();
    return lifetime && lifetime->script && lifetime->owner && lifetime->owner->IsValid()
        ? dynamic_cast<T*>(lifetime->script) : nullptr;
}
}
