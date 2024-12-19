#pragma once
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    extern void EncodeTransform(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeColorComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeSphereColliderComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeRectTransform(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeUIImageComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeTextComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeButtonComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeTagComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeMeshComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeTagComponent(YAML::Emitter &_out, Canis::Entity &_entity);

    extern void EncodeMeshComponent(YAML::Emitter &_out, Canis::Entity &_entity);
}