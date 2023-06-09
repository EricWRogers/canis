#include <Canis/ECS/Decode.hpp>
#include <Canis/SceneManager.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/SpriteAnimationComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>
#include <Canis/ECS/Components/Camera2DComponent.hpp>
#include <Canis/ECS/Components/CircleColliderComponent.hpp>

namespace Canis
{

    void DecodeTagComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto tagComponent = _n["Canis::TagComponent"])
        {
            auto &tc = _entity.AddComponent<Canis::TagComponent>();
            std::string fileTag = tagComponent.as<std::string>();
            char tag[20] = "";

            int i = 0;
            while (i < 20 - 1 && i < fileTag.size())
            {
                tag[i] = fileTag[i];
                i++;
            }
            tag[i] = '\0';

            strcpy(tc.tag, tag);
        }
    }

    void DecodeCamera2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto camera2dComponent = _n["Canis::Camera2DComponent"])
        {
            auto &c2dc = _entity.AddComponent<Canis::Camera2DComponent>();
            c2dc.position = camera2dComponent["position"].as<glm::vec2>();
            c2dc.scale = camera2dComponent["scale"].as<float>();
        }
    }

    void DecodeRectTransformComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto rectTransform = _n["Canis::RectTransformComponent"])
        {
            auto &rt = _entity.AddComponent<Canis::RectTransformComponent>();
            rt.active = rectTransform["active"].as<bool>();
            rt.anchor = (Canis::RectAnchor)rectTransform["anchor"].as<int>();
            rt.position = rectTransform["position"].as<glm::vec2>();
            rt.size = rectTransform["size"].as<glm::vec2>();
            rt.originOffset = rectTransform["originOffset"].as<glm::vec2>();
            rt.rotation = rectTransform["rotation"].as<float>();
            rt.scale = rectTransform["scale"].as<float>();
            rt.depth = rectTransform["depth"].as<float>();
        }
    }

    void DecodeColorComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto colorComponent = _n["Canis::ColorComponent"])
        {
            auto &cc = _entity.AddComponent<Canis::ColorComponent>();
            cc.color = colorComponent["color"].as<glm::vec4>();
        }
    }

    void DecodeTextComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto textComponent = _n["Canis::TextComponent"])
        {
            auto &tc = _entity.AddComponent<Canis::TextComponent>();
            auto asset = textComponent["assetId"];
            if (asset)
            {
                tc.assetId = _sceneManager->assetManager->LoadText(
                    asset["path"].as<std::string>(),
                    asset["size"].as<unsigned int>());
            }
            tc.text = new std::string;
            (*tc.text) = textComponent["text"].as<std::string>();
            tc.alignment = textComponent["alignment"].as<unsigned int>();
        }
    }

    void DecodeSprite2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto sprite2DComponent = _n["Canis::Sprite2DComponent"])
        {
            auto &s2dc = _entity.AddComponent<Canis::Sprite2DComponent>();
            s2dc.uv = sprite2DComponent["uv"].as<glm::vec4>();
            s2dc.texture = _sceneManager->assetManager->Get<Canis::TextureAsset>(
                                           _sceneManager->assetManager->LoadTexture(
                                               sprite2DComponent["texture"].as<std::string>()))
                               ->GetTexture();
        }
    }

    void DecodeUIImageComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto uiImageComponent = _n["Canis::UIImageComponent"])
        {
            auto &uiic = _entity.AddComponent<Canis::UIImageComponent>();
            uiic.uv = uiImageComponent["uv"].as<glm::vec4>();
            uiic.texture = _sceneManager->assetManager->Get<Canis::TextureAsset>(
                                           _sceneManager->assetManager->LoadTexture(
                                               uiImageComponent["texture"].as<std::string>()))
                               ->GetTexture();
        }
    }

    void DecodeUISliderComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto uiSliderComponent = _n["Canis::UISliderComponent"])
        {
            auto &uisc = _entity.AddComponent<Canis::UISliderComponent>();
            uisc.maxWidth = uiSliderComponent["maxWidth"].as<float>();
            uisc.minUVX = uiSliderComponent["minUVX"].as<float>();
            uisc.maxUVX = uiSliderComponent["maxUVX"].as<float>();
            uisc.value = uiSliderComponent["value"].as<float>();
        }
    }

    void DecodeSpriteAnimationComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto spriteAnimationComponent = _n["Canis::SpriteAnimationComponent"])
        {
            auto &sac = _entity.AddComponent<Canis::SpriteAnimationComponent>();
            sac.animationId = _sceneManager->assetManager->LoadSpriteAnimation(
                spriteAnimationComponent["animationId"].as<std::string>());
            sac.countDown = spriteAnimationComponent["countDown"].as<float>();
            sac.index = spriteAnimationComponent["index"].as<unsigned short int>();
            sac.flipX = spriteAnimationComponent["flipX"].as<bool>();
            sac.flipY = spriteAnimationComponent["flipY"].as<bool>();
            sac.speed = spriteAnimationComponent["speed"].as<float>();
        }
    }

    void DecodeCircleColliderComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto circleColliderComponent = _n["Canis::CircleColliderComponent"])
        {
            auto &ccc = _entity.AddComponent<Canis::CircleColliderComponent>();
            ccc.center = circleColliderComponent["center"].as<glm::vec2>();
            ccc.radius = circleColliderComponent["radius"].as<float>();
            ccc.layer = (Canis::BIT)circleColliderComponent["layer"].as<unsigned int>();
            ccc.mask = (Canis::BIT)circleColliderComponent["mask"].as<unsigned int>();
        }
    }
};