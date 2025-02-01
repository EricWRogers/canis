#pragma once
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    class SceneManager;
    
    extern void DecodeTagComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeCamera2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeRectTransform(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeButtonComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeColor(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeTextComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeSprite2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeUIImageComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeUISliderComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeSpriteAnimationComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeCircleColliderComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeTransform(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeMesh(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeSphereCollider(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern void DecodeDirectionalLight(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern bool DecodeRenderMeshSystem(const std::string &_name, Canis::Scene *_scene);
    extern bool DecodeButtonSystem(const std::string &_name, Canis::Scene *_scene);
    extern bool DecodeRenderHUDSystem(const std::string &_name, Canis::Scene *_scene);
    extern bool DecodeRenderTextSystem(const std::string &_name, Canis::Scene *_scene);
    extern bool DecodeSpriteRenderer2DSystem(const std::string &_name, Canis::Scene *_scene);
};