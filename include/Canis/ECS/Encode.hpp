#pragma once
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    extern void EncodeTransformComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeColorComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeSphereColliderComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeRectTransformComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeTextComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeTagComponent(YAML::Emitter &_out, Canis::Entity &_entity);
}