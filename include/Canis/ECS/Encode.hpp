#pragma once
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    extern void EncodeTransformComponent(YAML::Emitter &_out, Canis::Entity &_entity);
}