#include <Canis/ECS/Encode.hpp>
#include <Canis/Yaml.hpp>

#include <Canis/AssetManager.hpp>

#include <Canis/ECS/Components/Transform.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/SphereColliderComponent.hpp>
#include <Canis/ECS/Components/RectTransform.hpp>
#include <Canis/ECS/Components/UIImageComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/ButtonComponent.hpp>
#include <Canis/ECS/Components/TagComponent.hpp>
#include <Canis/ECS/Components/MeshComponent.hpp>

namespace Canis
{
    void EncodeTransform(YAML::Emitter &_out, Canis::Entity &_entity)
    {
        if (_entity.HasComponent<Transform>())
        {
            Transform& tc = _entity.GetComponent<Transform>();

            _out << YAML::Key << "Canis::Transform";
            _out << YAML::BeginMap;

            _out << YAML::Key << "active" << YAML::Value << tc.active;
            _out << YAML::Key << "position" << YAML::Value << tc.position;
            _out << YAML::Key << "rotation" << YAML::Value << glm::degrees(glm::eulerAngles(tc.rotation));
            _out << YAML::Key << "scale" << YAML::Value << tc.scale;

            _out << YAML::EndMap;
        }
    }

	void EncodeColorComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<ColorComponent>())
        {
            ColorComponent& cc = _entity.GetComponent<ColorComponent>();

            _out << YAML::Key << "Canis::ColorComponent";
            _out << YAML::BeginMap;

            _out << YAML::Key << "color" << YAML::Value << cc.color;
            _out << YAML::Key << "emission" << YAML::Value << cc.emission;
            _out << YAML::Key << "emissionUsingAlbedoIntesity" << YAML::Value << cc.emissionUsingAlbedoIntesity;

            _out << YAML::EndMap;
        }
	}

	void EncodeSphereColliderComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<SphereColliderComponent>())
        {
            SphereColliderComponent& scc = _entity.GetComponent<SphereColliderComponent>();

            _out << YAML::Key << "Canis::SphereColliderComponent";
            _out << YAML::BeginMap;

            _out << YAML::Key << "center" << YAML::Value << scc.center;
            _out << YAML::Key << "radius" << YAML::Value << scc.radius;
            _out << YAML::Key << "layer" << YAML::Value << scc.layer;
			_out << YAML::Key << "mask" << YAML::Value << scc.mask;

            _out << YAML::EndMap;
        }
	}

	void EncodeRectTransform(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<RectTransform>())
        {
            RectTransform& rtc = _entity.GetComponent<RectTransform>();

            _out << YAML::Key << "Canis::RectTransform";
            _out << YAML::BeginMap;

            _out << YAML::Key << "active" << YAML::Value << rtc.active;
			_out << YAML::Key << "anchor" << YAML::Value << rtc.anchor;
			_out << YAML::Key << "position" << YAML::Value << rtc.position;
			_out << YAML::Key << "size" << YAML::Value << rtc.size;
			_out << YAML::Key << "originOffset" << YAML::Value << rtc.originOffset;
			_out << YAML::Key << "rotation" << YAML::Value << rtc.rotation;
			_out << YAML::Key << "scale" << YAML::Value << rtc.scale;
			_out << YAML::Key << "depth" << YAML::Value << rtc.depth;
			_out << YAML::Key << "scaleWithScreen" << YAML::Value << (unsigned int)rtc.scaleWithScreen;

            _out << YAML::EndMap;
        }
	}

	void EncodeUIImageComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<UIImageComponent>())
        {
            auto& ic = _entity.GetComponent<UIImageComponent>();

            _out << YAML::Key << "Canis::UIImageComponent";
            _out << YAML::BeginMap;

			_out << YAML::Key << "TextureAsset";
            _out << YAML::BeginMap;

			_out << YAML::Key << "path" << YAML::Value << AssetManager::Get<TextureAsset>(ic.textureHandle.id)->GetPath();

			_out << YAML::EndMap; // close asset map

			_out << YAML::Key << "uv" << YAML::Value << ic.uv;

            _out << YAML::EndMap; // close text component map
        }
	}
	
	void EncodeTextComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<TextComponent>())
        {
            TextComponent& tc = _entity.GetComponent<TextComponent>();

            _out << YAML::Key << "Canis::TextComponent";
            _out << YAML::BeginMap;

			_out << YAML::Key << "text" << YAML::Value << tc.text;
			_out << YAML::Key << "alignment" << YAML::Value << tc.alignment;

			_out << YAML::Key << "TextAsset";
            _out << YAML::BeginMap;

			_out << YAML::Key << "path" << YAML::Value << AssetManager::Get<TextAsset>(tc.assetId)->GetPath();
			_out << YAML::Key << "size" << YAML::Value << AssetManager::Get<TextAsset>(tc.assetId)->GetFontSize();

			_out << YAML::EndMap; // close asset map

            _out << YAML::EndMap; // close text component map
        }
	}

	void EncodeButtonComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<ButtonComponent>())
		{
			ButtonComponent& bc = _entity.GetComponent<ButtonComponent>();
			ButtonComponent defaultButton;

			_out << YAML::Key << "Canis::ButtonComponent";
            _out << YAML::BeginMap;

			if (bc.eventName != defaultButton.eventName)
				_out << YAML::Key << "eventName" << YAML::Value << bc.eventName;

			if (bc.baseColor != defaultButton.baseColor)
				_out << YAML::Key << "baseColor" << YAML::Value << bc.baseColor;

			if (bc.hoverColor != defaultButton.hoverColor)
				_out << YAML::Key << "hoverColor" << YAML::Value << bc.hoverColor;

			if (bc.action != defaultButton.action)
				_out << YAML::Key << "action" << YAML::Value << bc.action;

			if (bc.mouseOver != defaultButton.mouseOver)
				_out << YAML::Key << "mouseOver" << YAML::Value << bc.mouseOver;

			if (bc.scale != defaultButton.scale)
				_out << YAML::Key << "scale" << YAML::Value << bc.scale;

			if (bc.hoverScale != defaultButton.hoverScale)
				_out << YAML::Key << "hoverScale" << YAML::Value << bc.hoverScale;

			if (bc.up)
				if (bc.up.HasComponent<IDComponent>())
					_out << YAML::Key << "up" << YAML::Value << (uint64_t)bc.up.GetComponent<IDComponent>().ID;
			
			if (bc.down)
				if (bc.down.HasComponent<IDComponent>())
					_out << YAML::Key << "down" << YAML::Value << (uint64_t)bc.down.GetComponent<IDComponent>().ID;
			
			if (bc.left)
				if (bc.left.HasComponent<IDComponent>())
					_out << YAML::Key << "left" << YAML::Value << (uint64_t)bc.left.GetComponent<IDComponent>().ID;
			
			if (bc.right)
				if (bc.right.HasComponent<IDComponent>())
					_out << YAML::Key << "right" << YAML::Value << (uint64_t)bc.right.GetComponent<IDComponent>().ID;

			if (bc.defaultSelected != defaultButton.defaultSelected)
				_out << YAML::Key << "defaultSelected" << YAML::Value << bc.defaultSelected;

			_out << YAML::EndMap; // close asset map
		}
	}

	void EncodeTagComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<TagComponent>())
		{
			TagComponent& tc = _entity.GetComponent<TagComponent>();

            _out << YAML::Key << "Canis::TagComponent" << YAML::Value << std::string(tc.tag);
		}
	}

	void EncodeMeshComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<MeshComponent>())
		{
			MeshComponent& mc = _entity.GetComponent<MeshComponent>();

			_out << YAML::Key << "Canis::MeshComponent";
            _out << YAML::BeginMap;

			_out << YAML::Key << "modelPath" << YAML::Value << AssetManager::GetPath(mc.modelHandle.id);
			_out << YAML::Key << "materialPath" << YAML::Value << AssetManager::GetPath(mc.material);
            _out << YAML::Key << "castShadow" << YAML::Value << mc.castShadow;
			_out << YAML::Key << "useInstance" << YAML::Value << mc.useInstance;
			_out << YAML::Key << "castDepth" << YAML::Value << mc.castDepth;
			

            _out << YAML::EndMap;
		}
	}
}