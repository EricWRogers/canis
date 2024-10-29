#pragma once
#include <Canis/Scene.hpp>
#include <Canis/Debug.hpp>
#include <Canis/External/entt.hpp>
#include <Canis/ECS/Components/IDComponent.hpp>
#include <Canis/ECS/Components/TagComponent.hpp>
#include <Canis/ECS/Components/ScriptComponent.hpp>
#include <Canis/ECS/Components/TransformComponent.hpp>

namespace Canis
{
class SceneManager;
class Entity
{
private:
    friend class Scene;

    void* InitScriptableComponent();
    void RemoveScriptable(Canis::ScriptComponent &script);
public:
    entt::entity entityHandle{ entt::null };
    Canis::Scene *scene = nullptr;

    Entity() = default;
    Entity(Scene* _scene) : scene(_scene) {};
    Entity(entt::entity _handle, Scene* _scene) : entityHandle(_handle), scene(_scene) {};
    Entity(const Entity& _other) = default;

    void Destroy();

    SceneManager& GetSceneManager();

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        if(HasComponent<T>())
            FatalError("Entity already has component!");
        
        T& component = scene->entityRegistry.emplace<T>(entityHandle, std::forward<Args>(args)...);
        
        if constexpr (std::is_same_v<T, Canis::TransformComponent>) {
            component.registry = &(scene->entityRegistry);
        }

        return component;
    }

    /*template<>
    TransformComponent& Canis::Entity::AddComponent<TransformComponent>()
    {
        if(HasComponent<TransformComponent>())
            FatalError("Entity already has component!");
        
        TransformComponent& component = scene->entityRegistry.emplace<TransformComponent>(entityHandle);
        component.registry = &(scene->entityRegistry);
        return component;
    }*/

    template<typename T, typename... Args>
    T& AddOrReplaceComponent(Args&&... args)
    {
        T& component = scene->entityRegistry.emplace_or_replace<T>(entityHandle, std::forward<Args>(args)...);
        return component;
    }

    template<typename T>
    bool HasComponent()
    {
        return scene->entityRegistry.all_of<T>(entityHandle);
    }

    template<typename T>
    T& GetComponent()
    {
        if(!HasComponent<T>())
            FatalError("Entity does not have component!");
        return scene->entityRegistry.get<T>(entityHandle);
    }

    template<typename T>
    void RemoveComponent()
    {
        if(!HasComponent<T>())
            FatalError("Entity does not have component!");
        scene->entityRegistry.remove<T>(entityHandle);
    }

    template<typename T>
    T& AddScript()
    {
        if (HasComponent<ScriptComponent>())
        {
            FatalError("Entity already has script component");
        }

        Canis::ScriptComponent& sc = AddComponent<Canis::ScriptComponent>();
        sc.Bind<T>();

        return *(T*)InitScriptableComponent();
    }

    template<typename T>
    bool HasScript()
    {
        if (!HasComponent<ScriptComponent>())
            return false;
        
        Canis::ScriptComponent& sc = GetComponent<Canis::ScriptComponent>();
        T* scriptableEntity = nullptr;
        
        if ((scriptableEntity = dynamic_cast<T*>(sc.Instance)) != nullptr)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    template<typename T>
    T& GetScript()
    {
        if (!HasComponent<ScriptComponent>())
        {
            FatalError("Entity does not script component");
        }

        Canis::ScriptComponent& sc = GetComponent<Canis::ScriptComponent>();
        T* scriptableEntity = nullptr;
        
        if ((scriptableEntity = dynamic_cast<T*>(sc.Instance)) != nullptr)
        {
            return *scriptableEntity;
        }
        else
        {
            FatalError("Entity does not have that component");
            return *static_cast<T*>(sc.Instance);
        }
    }

    void RemoveScript()
    {
        if (!HasComponent<ScriptComponent>())
        {
            FatalError("Entity does not script component");
        }

        Canis::ScriptComponent& scriptComponent = GetComponent<Canis::ScriptComponent>();

        RemoveScriptable(scriptComponent);

        RemoveComponent<ScriptComponent>();
    }

    void SetTag(std::string _tag);

    bool TagEquals(const char a[20], const char b[20]);
    bool TagEquals(const std::string &_tag);
    Entity GetEntityWithTag(std::string _tag);
    std::vector<Entity> GetEntitiesWithTag(std::string _tag);

    Entity Duplicate();

    void SetParent(entt::entity _parent);
    void AddChild(entt::entity _child);
    int ChildCount();
    Entity GetChild(int _index);

    void SetPosition(glm::vec3 _postion);
    void MovePosition(glm::vec3 _delta);

    void SetRotation(glm::vec3 _radians);
    void Rotate(glm::vec3 _radians);

    void SetScale(glm::vec3 _scale);

    glm::vec3 GetGlobalPosition();

    operator bool() {
        if (entityHandle == entt::null)
            return false;
        
        if (scene == nullptr)
            return false;
        
        if (entityHandle == entt::tombstone || !scene->entityRegistry.valid(entityHandle))
        {
            entityHandle = entt::null;
            return false;
        }

        return true;
    }
    operator entt::entity() const { return entityHandle; }
    operator uint32_t() const { return (uint32_t)entityHandle; }

    UUID GetUUID() { return GetComponent<IDComponent>().ID; }
    //const std::string& GetName() { return GetComponent<TagComponent>().Tag; }

    bool operator==(const Entity& other) const
    {
        return entityHandle == other.entityHandle && scene == other.scene;
    }

    bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }
};
} // end of Canis namespace