#pragma once
#include <Canis/Scene.hpp>
#include <Canis/Debug.hpp>
#include <Canis/External/entt.hpp>
#include <Canis/ECS/Components/TagComponent.hpp>

namespace Canis
{
class Entity
{
private:
    friend class Scene;
public:
    entt::entity entityHandle{ entt::null };
    Canis::Scene *scene = nullptr;

    Entity() = default;
    Entity(Scene* _scene) : scene(_scene) {};
    Entity(entt::entity _handle, Scene* _scene) : entityHandle(_handle), scene(_scene) {};
    Entity(const Entity& _other) = default;

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        if(HasComponent<T>())
            FatalError("Entity already has component!");
        
        T& component = scene->entityRegistry.emplace<T>(entityHandle, std::forward<Args>(args)...);
        //scene->OnComponentAdded<T>(*this, component);
        return component;
    }

    template<typename T, typename... Args>
    T& AddOrReplaceComponent(Args&&... args)
    {
        T& component = scene->entityRegistry.emplace_or_replace<T>(entityHandle, std::forward<Args>(args)...);
        //scene->OnComponentAdded<T>(*this, component);
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

    bool TagEquals(const char a[20], const char b[20]);
    Entity GetEntityWithTag(std::string _tag);
    std::vector<Entity> GetEntitiesWithTag(std::string _tag);

    void SetParent(entt::entity _parent);
    void AddChild(entt::entity _child);

    void SetPosition(glm::vec3 _postion);
    void MovePosition(glm::vec3 _delta);

    void SetRotation(glm::vec3 _radians);
    void Rotate(glm::vec3 _radians);

    void SetScale(glm::vec3 _scale);

    glm::vec3 GetGlobalPosition();

    operator bool() const { return entityHandle != entt::null; }
    operator entt::entity() const { return entityHandle; }
    operator uint32_t() const { return (uint32_t)entityHandle; }

    //UUID GetUUID() { return GetComponent<IDComponent>().ID; }
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