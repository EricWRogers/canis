#include <Canis/ECS/Decode.hpp>
#include <Canis/SceneManager.hpp>
#include <Canis/ECS/Components/Color.hpp>
#include <Canis/ECS/Components/RectTransform.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/Sprite2DComponent.hpp>
#include <Canis/ECS/Components/SpriteAnimationComponent.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/UISliderComponent.hpp>
#include <Canis/ECS/Components/Camera2DComponent.hpp>
#include <Canis/ECS/Components/CircleColliderComponent.hpp>

#include <Canis/ECS/Components/Transform.hpp>
#include <Canis/ECS/Components/Mesh.hpp>
#include <Canis/ECS/Components/SphereCollider.hpp>
#include <Canis/ECS/Components/DirectionalLight.hpp>

#include <Canis/ECS/Systems/RenderMeshSystem.hpp>
#include <Canis/ECS/Systems/ButtonSystem.hpp>
#include <Canis/ECS/Systems/RenderHUDSystem.hpp>
#include <Canis/ECS/Systems/RenderTextSystem.hpp>
#include <Canis/ECS/Systems/SpriteRenderer2DSystem.hpp>

#include <Canis/External/GetNameOfType.hpp>

using namespace glm;

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

    void DecodeRectTransform(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto rectTransform = _n["Canis::RectTransform"])
        {
            auto &rt = _entity.AddComponent<Canis::RectTransform>();
            rt.active = rectTransform["active"].as<bool>();
            rt.anchor = (Canis::RectAnchor)rectTransform["anchor"].as<int>();
            rt.position = rectTransform["position"].as<glm::vec2>();
            rt.size = rectTransform["size"].as<glm::vec2>();
            rt.originOffset = rectTransform["originOffset"].as<glm::vec2>();
            rt.rotation = rectTransform["rotation"].as<float>();
            rt.scale = rectTransform["scale"].as<float>();
            rt.depth = rectTransform["depth"].as<float>();
            rt.scaleWithScreen = (ScaleWithScreen)rectTransform["scaleWithScreen"].as<int>(0);
        }
    }

    void DecodeButtonComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto buttonComponent = _n["Canis::ButtonComponent"])
        {
            ButtonComponent defaultButton;

            auto &bc = _entity.AddComponent<ButtonComponent>();
            bc.eventName = buttonComponent["eventName"].as<std::string>();
            bc.baseColor = buttonComponent["baseColor"].as<vec4>(defaultButton.baseColor);
            bc.hoverColor = buttonComponent["hoverColor"].as<vec4>(defaultButton.hoverColor);
            bc.action = buttonComponent["action"].as<unsigned int>(defaultButton.action);
            bc.mouseOver = buttonComponent["mouseOver"].as<bool>(defaultButton.mouseOver);
            bc.scale = buttonComponent["scale"].as<float>(defaultButton.scale);
            bc.hoverScale = buttonComponent["hoverScale"].as<float>(defaultButton.hoverScale);
            //bc.up = buttonComponent["up"].as<uint64_t>(defaultButton.up);
            //bc.down = buttonComponent["down"].as<std::string>(defaultButton.down);
            //bc.left = buttonComponent["left"].as<std::string>(defaultButton.left);
            //bc.right = buttonComponent["right"].as<std::string>(defaultButton.right);
            bc.defaultSelected = buttonComponent["defaultSelected"].as<bool>(defaultButton.defaultSelected);
        }
    }

    void DecodeColor(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto colorComponent = _n["Canis::Color"])
        {
            auto &cc = _entity.AddComponent<Canis::Color>();
            cc.color = colorComponent["color"].as<glm::vec4>(cc.color);
            cc.emission = colorComponent["emission"].as<glm::vec3>(cc.emission);
            cc.emissionUsingAlbedoIntesity = colorComponent["emissionUsingAlbedoIntesity"].as<float>(cc.emissionUsingAlbedoIntesity);
        }
    }

    void DecodeTextComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto textComponent = _n["Canis::TextComponent"])
        {
            auto &tc = _entity.AddComponent<Canis::TextComponent>();
            auto asset = textComponent["TextAsset"];
            if (asset)
            {
                tc.assetId = AssetManager::LoadText(
                    asset["path"].as<std::string>(),
                    asset["size"].as<unsigned int>());
            }
            tc.text = textComponent["text"].as<std::string>("");
            tc.alignment = textComponent["alignment"].as<unsigned int>();
        }
    }

    void DecodeSprite2DComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto sprite2DComponent = _n["Canis::Sprite2DComponent"])
        {
            auto &s2dc = _entity.AddComponent<Canis::Sprite2DComponent>();
            s2dc.uv = sprite2DComponent["uv"].as<glm::vec4>();
            s2dc.textureHandle = sprite2DComponent["textureHandle"].as<TextureHandle>();//AssetManager::GetTextureHandle(sprite2DComponent["textureHandle"].as<std::string>());
        }
    }

    void DecodeUIImageComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto uiImageComponent = _n["Canis::UIImageComponent"])
        {
            auto &uiic = _entity.AddComponent<Canis::UIImageComponent>();
            uiic.uv = uiImageComponent["uv"].as<glm::vec4>();
            if (auto textureAsset = uiImageComponent["TextureAsset"])
            {
                uiic.textureHandle = AssetManager::GetTextureHandle(textureAsset["path"].as<std::string>());
            }
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
            uisc.targetValue = uisc.value;
        }
    }

    void DecodeSpriteAnimationComponent(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto spriteAnimationComponent = _n["Canis::SpriteAnimationComponent"])
        {
            auto &sac = _entity.AddComponent<Canis::SpriteAnimationComponent>();
            sac.animationId = AssetManager::LoadSpriteAnimation(
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

    void DecodeTransform(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto transformComponent = _n["Canis::Transform"])
        {
            auto &tc = _entity.AddComponent<Canis::Transform>();
            tc.active = transformComponent["active"].as<bool>(true);
            tc.position = transformComponent["position"].as<glm::vec3>(glm::vec3(0.0f));
            tc.rotation = glm::quat(glm::degrees(transformComponent["rotation"].as<glm::vec3>(glm::vec3(0.0f))));
            tc.scale = transformComponent["scale"].as<glm::vec3>(glm::vec3(1.0f));
            UpdateModelMatrix(tc);
        }
    }

    void DecodeMesh(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto meshComponent = _n["Canis::Mesh"])
        {
            auto &mc = _entity.AddComponent<Canis::Mesh>();
            mc.castShadow = meshComponent["castShadow"].as<bool>(mc.castShadow);
            mc.useInstance = meshComponent["useInstance"].as<bool>(mc.useInstance);
            mc.castDepth = meshComponent["castDepth"].as<bool>(mc.castDepth);

            std::string modelPath = meshComponent["modelPath"].as<std::string>("");
            if (modelPath == "")
                Canis::FatalError("Decode mesh component: modelPath was empty");
            
            mc.modelHandle.id = AssetManager::LoadModel(modelPath);

            std::string materialPath = meshComponent["materialPath"].as<std::string>("");
            if (materialPath == "")
                Canis::FatalError("Decode mesh component: materialPath was empty");
            
            mc.material = AssetManager::LoadMaterial(materialPath);
        }
    }

    void DecodeSphereCollider(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto sphereColliderComponent = _n["Canis::SphereCollider"])
        {
            auto &scc = _entity.AddComponent<SphereCollider>();
            scc.center = sphereColliderComponent["center"].as<glm::vec3>(scc.center);
            scc.radius = sphereColliderComponent["radius"].as<float>(scc.radius);
            scc.layer = sphereColliderComponent["layer"].as<unsigned int>(scc.layer);
            scc.mask = sphereColliderComponent["mask"].as<unsigned int>(scc.mask);
        }
    }
    
    void DecodeDirectionalLight(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)
    {
        if (auto DirectionalLight = _n["Canis::DirectionalLight"])
        {
            auto &dlc = _entity.AddComponent<Canis::DirectionalLight>();
            dlc.direction = DirectionalLight["direction"].as<glm::vec3>();
            dlc.ambient = DirectionalLight["ambient"].as<glm::vec3>();
            dlc.diffuse = DirectionalLight["diffuse"].as<glm::vec3>();
            dlc.specular = DirectionalLight["specular"].as<glm::vec3>();
        }
    }

    bool DecodeRenderMeshSystem(const std::string &_name, Canis::Scene *_scene)
	{
        if (_name == "Canis::RenderMeshSystem")
        {
            _scene->CreateRenderSystem<RenderMeshSystem>();
            return true;
        }
        return false;
    }

    bool DecodeButtonSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::ButtonSystem")
        {
            _scene->CreateSystem<ButtonSystem>();
            return true;
        }
        return false;
    }

    bool DecodeRenderHUDSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::RenderHUDSystem")
        {
            _scene->CreateRenderSystem<RenderHUDSystem>();
            return true;
        }
        return false;
    }

    bool DecodeRenderTextSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::RenderTextSystem")
        {
            _scene->CreateRenderSystem<RenderTextSystem>();
            return true;
        }
        return false;
    }

    bool DecodeSpriteRenderer2DSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::SpriteRenderer2DSystem")
        {
            _scene->CreateRenderSystem<SpriteRenderer2DSystem>();
            return true;
        }
        return false;
    }
};