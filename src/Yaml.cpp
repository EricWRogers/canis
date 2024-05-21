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
} // end of YAML namespace