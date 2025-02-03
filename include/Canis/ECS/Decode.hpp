#pragma once
#include <Canis/Entity.hpp>
#include <Canis/Yaml.hpp>

namespace Canis
{
    class SceneManager;
    
    extern bool DecodeTagComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeCamera2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeRectTransform(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeButtonComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeColor(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeTextComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeSprite2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeUIImageComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeUISliderComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeSpriteAnimationComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeCircleColliderComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeTransform(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeMesh(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeSphereCollider(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);
    extern bool DecodeDirectionalLight(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager);

    extern bool DecodeRenderMeshSystem(const std::string &_name, Canis::Scene *_scene);
    extern bool DecodeButtonSystem(const std::string &_name, Canis::Scene *_scene);
    extern bool DecodeRenderHUDSystem(const std::string &_name, Canis::Scene *_scene);
    extern bool DecodeRenderTextSystem(const std::string &_name, Canis::Scene *_scene);
    extern bool DecodeSpriteRenderer2DSystem(const std::string &_name, Canis::Scene *_scene);
};