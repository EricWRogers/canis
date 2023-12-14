#pragma once
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    class SceneManager;
    
    extern void DecodeTagComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeCamera2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeRectTransformComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeColorComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeTextComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeSprite2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeUIImageComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeUISliderComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeSpriteAnimationComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeCircleColliderComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeTransformComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern void DecodeDirectionalLightComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
};