#include <Canis/Yaml.hpp>
#include <Canis/SceneManager.hpp>

void Canis::AddEntityAndUUIDToSceneManager(void *_entity, Canis::UUID _uuid, void *_sceneManager)
{
    SceneManager* sceneManager = (SceneManager*)_sceneManager;

    EntityAndUUID entityAndUUID;
    entityAndUUID.entity = (Entity*)_entity;
    entityAndUUID.uuid = _uuid;

    sceneManager->entityAndUUIDToConnect.push_back(entityAndUUID);
}

namespace YAML
{
    Emitter &operator<<(Emitter &out, const glm::vec2 &v)
    {
        out << Flow;
        out << BeginSeq << v.x << v.y << EndSeq;
        return out;
    }

    Emitter &operator<<(Emitter &out, const glm::vec3 &v)
    {
        out << Flow;
        out << BeginSeq << v.x << v.y << v.z << EndSeq;
        return out;
    }

    Emitter &operator<<(Emitter &out, const glm::vec4 &v)
    {
        out << Flow;
        out << BeginSeq << v.x << v.y << v.z << v.w << EndSeq;
        return out;
    }

    Emitter &operator<<(Emitter &out, const EntityData &e)
    {
        Canis::Entity nonConstE;
        nonConstE.entityHandle = e.entityHandle;
        nonConstE.scene = (Canis::Scene*)e.scene;
        
        if (nonConstE)
        {
            if (nonConstE.HasComponent<Canis::IDComponent>())
            {
                Canis::Log("HI");
                out << nonConstE.GetUUID();
                return out;
            }
        }

        out << 0;
        return out;
    }

    uint64_t GetUUIDFromEntityData(const EntityData &_entityData)
    {
        Canis::Entity nonConstE;
        nonConstE.entityHandle = _entityData.entityHandle;
        nonConstE.scene = (Canis::Scene*)_entityData.scene;
        
        if (nonConstE)
        {
            if (nonConstE.HasComponent<Canis::IDComponent>())
            {
                return (uint64_t)nonConstE.GetUUID();
            }
        }

        return 0lu;
    }
} // end of YAML namespace