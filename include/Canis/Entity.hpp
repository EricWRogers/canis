#pragma once

#include <Canis/Debug.hpp>
#include <Canis/Scene.hpp>
#include <Canis/External/entt.hpp>

namespace Canis
{
class Entity
{    
public:
    entt::entity entityHandle{ entt::null };
    Canis::Scene *scene = nullptr;

    Entity() = default;
    Entity(entt::entity _handle, Scene* _scene) : entityHandle(_handle), scene(_scene) {};
    Entity(const Entity& _other) = default;

    /*template<typename T, typename... Args>
    T& AddComponent(Args&&... args)
    {
        if(HasComponent<T>())
            FatalError("Entity already has component!");
        
        T& component = scene->entityRegistry.emplace<T>(entityHandle, std::forward<Args>(args)...);
        scene->OnComponentAdded<T>(*this, component);
        return component;
    }

    template<typename T, typename... Args>
    T& AddOrReplaceComponent(Args&&... args)
    {
        T& component = scene->entityRegistry.emplace_or_replace<T>(entityHandle, std::forward<Args>(args)...);
        scene->OnComponentAdded<T>(*this, component);
        return component;
    }*/

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

    operator bool() const { return entityHandle != entt::null; }
    operator entt::entity() const { return entityHandle; }
    operator uint32_t() const { return (uint32_t)entityHandle; }

    //UUID GetUUID() { return GetComponent<IDComponent>().ID; }
    //const std::string& GetName() { return GetComponent<TagComponent>().Tag; }

    /*bool operator==(const Entity& other) const
    {
        return entityHandle == other.entityHandle && scene == other.scene;
    }

    bool operator!=(const Entity& other) const
    {
        return !(*this == other);
    }*/
};
} // end of Canis namespace