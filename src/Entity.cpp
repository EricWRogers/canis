#include <Canis/Entity.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Math.hpp>

#include <Canis/ScriptableEntity.hpp>
#include <Canis/ECS/Components/ScriptComponent.hpp>
#include <Canis/ECS/Components/TransformComponent.hpp>

namespace Canis
{
void Entity::Destroy()
{
    if (scene != nullptr)
    {
        if (scene->entityRegistry.valid(entityHandle))
        {
            scene->entityRegistry.destroy(entityHandle);
        }
    }
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

void Entity::SetParent(entt::entity _parent)
{
    if (HasComponent<TransformComponent>())
    {
        if (scene->entityRegistry.all_of<TransformComponent>(_parent))
        {
            TransformComponent& transform = GetComponent<TransformComponent>();

            if (transform.parent != entt::null)
            {
                TransformComponent& parentTransform = scene->entityRegistry.get<TransformComponent>(transform.parent);

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

            TransformComponent& parentTransform = scene->entityRegistry.get<TransformComponent>(transform.parent);

            parentTransform.children.push_back(entityHandle);

            UpdateModelMatrix(transform);
        }
    }
}

void Entity::AddChild(entt::entity _child)
{
    if (HasComponent<TransformComponent>())
    {
        SetParent(_child);
    }
}

void Entity::SetPosition(glm::vec3 _postion)
{
    if (HasComponent<TransformComponent>())
    {
        TransformComponent& transform = GetComponent<TransformComponent>();
        SetTransformPosition(transform, _postion);
    }
}

void Entity::MovePosition(glm::vec3 _delta)
{
    if (HasComponent<TransformComponent>())
    {
        TransformComponent& transform = GetComponent<TransformComponent>();
        MoveTransformPosition(transform, _delta);
    }
}

void Entity::SetRotation(glm::vec3 _radians)
{
    if (HasComponent<TransformComponent>())
    {
        TransformComponent& transform = GetComponent<TransformComponent>();
        SetTransformRotation(transform, _radians);
    }
}

void Entity::Rotate(glm::vec3 _radians)
{
    if (HasComponent<TransformComponent>())
    {
        TransformComponent& transform = GetComponent<TransformComponent>();
        Canis::Rotate(transform, _radians);
    }
}

void Entity::SetScale(glm::vec3 _scale)
{
    if (HasComponent<TransformComponent>())
    {
        TransformComponent& transform = GetComponent<TransformComponent>();
        transform.scale = _scale;
        UpdateModelMatrix(transform);
    }
}

glm::vec3 Entity::GetGlobalPosition()
{
    if (HasComponent<TransformComponent>())
    {
        TransformComponent& transform = GetComponent<TransformComponent>();
        return glm::vec3(transform.modelMatrix[3]);
    }

    return glm::vec3(0.0f);
}
} // end of Canis namespace