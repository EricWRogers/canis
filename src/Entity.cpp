#include <Canis/Entity.hpp>
#include <Canis/Scene.hpp>
#include <Canis/SceneManager.hpp>
#include <Canis/Math.hpp>

#include <Canis/ScriptableEntity.hpp>
#include <Canis/ECS/Components/ScriptComponent.hpp>
#include <Canis/ECS/Components/Transform.hpp>

#include <iomanip>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <Canis/External/entt.hpp>


entt::entity DuplicateEntity(entt::registry& _from, entt::registry& _to, entt::entity _original) {
    entt::entity newEntity = _to.create();

    for(auto &&curr: _from.storage()) {
        if(auto &storage = curr.second; storage.contains(_original)) {
            auto* destinationStorage = _to.storage(curr.first);

            if (destinationStorage)
            {
                destinationStorage->push(newEntity, storage.value(_original));
            }
        }
    }

    return newEntity;
}

namespace Canis
{


void Entity::Destroy()
{
    if (scene != nullptr)
    {
        if (scene->entityRegistry.valid(entityHandle))
        {
            if (HasComponent<Transform>())
            {
                Transform& transform = GetComponent<Transform>();

                Entity child(scene);

                for (int i = 0; i < transform.children.size(); i++)
                {
                    child.entityHandle = transform.children[i];

                    child.Destroy();
                }
            }
            scene->entityRegistry.destroy(entityHandle);
        }
    }
}

SceneManager& Entity::GetSceneManager()
{
    return *(Canis::SceneManager*)scene->sceneManager;
}

void* Entity::InitScriptableComponent()
{
    Canis::ScriptComponent& sc = GetComponent<Canis::ScriptComponent>();
    sc.Instance = sc.InstantiateScript();
    sc.Instance->entity.entityHandle = entityHandle;
    sc.Instance->entity.scene = scene;
    sc.Instance->OnCreate();
    return sc.Instance;
}

void Entity::RemoveScriptable(Canis::ScriptComponent &script)
{
    if (script.Instance)
    {
        script.Instance->OnDestroy();
        delete script.Instance;
        script.Instance = nullptr;
    }
}

void Entity::SetTag(std::string _tag)
{
    if (HasComponent<TagComponent>() == false)
        AddComponent<TagComponent>();
    
    TagComponent& tagComponent = GetComponent<TagComponent>();

    char tag[20] = "";

    int i = 0;
    while(i < 20-1 && i < _tag.size())
    {
        tag[i] = _tag[i];
        i++;
    }
    tag[i] = '\0';

    strcpy(tagComponent.tag, tag);
}

bool Entity::TagEquals(const std::string &_tag)
{
    if (HasComponent<TagComponent>() == false)
        AddComponent<TagComponent>();
    
    TagComponent& tagComponent = GetComponent<TagComponent>();

    if (_tag.size() == 0)
    {
        if (tagComponent.tag[0] == 0)
        {
            return true;
        }

        return false;
    }

    for (int i = 0; i < 20; i++)
    {
        if (_tag.size() <= i)
        {
            if (tagComponent.tag[i] == 0)
            {
                return true;
            }

            return false;
        }

        if (tagComponent.tag[i] != _tag[i])
        {
            return false;
        }
    }

    return true;
}

Entity Entity::GetEntityWithTag(std::string _tag)
{
    Entity e = {};
    e.scene = scene;

    char tag[20] = "";

    int i = 0;
    while(i < 20-1 && i < _tag.size())
    {
        tag[i] = _tag[i];
        i++;
    }
    tag[i] = '\0';

    auto view = scene->entityRegistry.view<TagComponent>();

    for(auto [entity, tagComponent] : view.each())
    {
        if(TagEquals(tagComponent.tag, tag))
        {
            e.entityHandle = entity;
            break;
        }
    }

    return e;
}

std::vector<Entity> Entity::GetEntitiesWithTag(std::string _tag)
{
    std::vector<Entity> entities = {};
    char tag[20] = "";

    int i = 0;
    while(i < 20-1 && i < _tag.size())
    {
        tag[i] = _tag[i];
        i++;
    }
    tag[i] = '\0';

    auto view = scene->entityRegistry.view<const TagComponent>();

    for(auto [entity, tagComponent] : view.each())
        if(TagEquals(tagComponent.tag, tag))
            entities.push_back(Entity(entity, scene));

    return entities;
}

bool Entity::TagEquals(const char a[20], const char b[20])
{
    int i = 0;
    while(i < 20)
    {
        if ((int)a[i] - (int)b[i] != 0)
            return false;
        
        i++;
    }
    return true;
}

Entity Entity::Duplicate()
{
    Entity e(
        DuplicateEntity(scene->entityRegistry, scene->entityRegistry, entityHandle)
        ,scene
    );

    if (e.HasComponent<IDComponent>())
    {
        e.RemoveComponent<IDComponent>();
        e.AddComponent<IDComponent>();
    }

    return e;
}

void Entity::SetParent(entt::entity _parent, bool _updatePosition)
{
    if (HasComponent<Transform>())
    {
        if (scene->entityRegistry.all_of<Transform>(_parent))
        {
            Transform& transform = GetComponent<Transform>();
            glm::vec3 globalPosition = GetGlobalPosition();

            if (transform.parent != entt::null)
            {
                Transform& parentTransform = scene->entityRegistry.get<Transform>(transform.parent);

                for (int i = 0; i < parentTransform.children.size(); i++)
                {
                    if (parentTransform.children[i] == entityHandle)
                    {
                        parentTransform.children.erase(parentTransform.children.begin() + i);
                        break;
                    }
                }
            }

            transform.parent = _parent;

            Transform& parentTransform = scene->entityRegistry.get<Transform>(transform.parent);
            glm::vec3 parentGlobalPosition = Entity(transform.parent, scene).GetGlobalPosition();

            parentTransform.children.push_back(entityHandle);

            if (_updatePosition)
                SetPosition(globalPosition - parentGlobalPosition);

            UpdateModelMatrix(transform);
        }
    }
}

void Entity::AddChild(entt::entity _child)
{
    if (HasComponent<Transform>())
    {
        SetParent(_child);
    }
}

int Entity::ChildCount()
{
    if (HasComponent<Transform>() == false)
        return 0;

    auto& transform = GetComponent<Transform>();

    return transform.children.size();
}

Entity Entity::GetChild(int _index)
{
    if (HasComponent<Transform>() == false)
        return Entity(scene);
    
    auto& transform = GetComponent<Transform>();

    if (transform.children.size() >= _index || _index < 0)
        return Entity(scene);
    
    return Entity(GetComponent<Transform>().children[_index], scene);
}

void Entity::SetPosition(glm::vec3 _postion)
{
    if (HasComponent<Transform>())
    {
        Transform& transform = GetComponent<Transform>();
        SetTransformPosition(transform, _postion);
    }
}

void Entity::MovePosition(glm::vec3 _delta)
{
    if (HasComponent<Transform>())
    {
        Transform& transform = GetComponent<Transform>();
        MoveTransformPosition(transform, _delta);
    }
}

void Entity::SetRotation(glm::vec3 _radians)
{
    if (HasComponent<Transform>())
    {
        Transform& transform = GetComponent<Transform>();
        SetTransformRotation(transform, _radians);
    }
}

void Entity::Rotate(glm::vec3 _radians)
{
    if (HasComponent<Transform>())
    {
        Transform& transform = GetComponent<Transform>();
        Canis::Rotate(transform, _radians);
    }
}

void Entity::SetScale(glm::vec3 _scale)
{
    if (HasComponent<Transform>())
    {
        Transform& transform = GetComponent<Transform>();
        transform.scale = _scale;
        UpdateModelMatrix(transform);
    }
}

glm::vec3 Entity::GetGlobalPosition()
{
    if (HasComponent<Transform>())
    {
        Transform& transform = GetComponent<Transform>();

        if (transform.isDirty) // it would be nice to remove this
            UpdateModelMatrix(transform);
        
        return glm::vec3(transform.modelMatrix[3]);
    }

    return glm::vec3(0.0f);
}
} // end of Canis namespace