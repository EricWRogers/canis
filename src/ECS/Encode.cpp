#include <Canis/ECS/Encode.hpp>

#include <Canis/AssetManager.hpp>

#include <Canis/ECS/Components/TransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/SphereColliderComponent.hpp>
#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>
#include <Canis/ECS/Components/TagComponent.hpp>
#include <Canis/ECS/Components/MeshComponent.hpp>

namespace Canis
{
    YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec2& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec3& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
		return out;
	}

	YAML::Emitter& operator<<(YAML::Emitter& out, const glm::vec4& v)
	{
		out << YAML::Flow;
		out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
		return out;
	}

    void EncodeTransformComponent(YAML::Emitter &_out, Canis::Entity &_entity)
    {
        if (_entity.HasComponent<TransformComponent>())
        {
            TransformComponent& tc = _entity.GetComponent<TransformComponent>();

            _out << YAML::Key << "Canis::TransformComponent";
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

	void EncodeRectTransformComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<RectTransformComponent>())
        {
            RectTransformComponent& rtc = _entity.GetComponent<RectTransformComponent>();

            _out << YAML::Key << "Canis::RectTransformComponent";
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

	void EncodeTextComponent(YAML::Emitter &_out, Canis::Entity &_entity)
	{
		if (_entity.HasComponent<TextComponent>())
        {
            TextComponent& tc = _entity.GetComponent<TextComponent>();

            _out << YAML::Key << "Canis::TextComponent";
            _out << YAML::BeginMap;

			_out << YAML::Key << "assetId";
            _out << YAML::BeginMap;

			_out << YAML::Key << "path" << YAML::Value << AssetManager::GetPath(tc.assetId);
			_out << YAML::Key << "size" << YAML::Value << AssetManager::Get<TextAsset>(tc.assetId)->GetFontSize();

			_out << YAML::EndMap;

            _out << YAML::Key << "active" << YAML::Value << tc.assetId;
			_out << YAML::Key << "text" << YAML::Value << tc.text;
			_out << YAML::Key << "alignment" << YAML::Value << tc.alignment;

            _out << YAML::EndMap;
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

			_out << YAML::Key << "modelPath" << YAML::Value << AssetManager::GetPath(mc.id);
			_out << YAML::Key << "materialPath" << YAML::Value << AssetManager::GetPath(mc.material);
            _out << YAML::Key << "castShadow" << YAML::Value << mc.castShadow;
			_out << YAML::Key << "useInstance" << YAML::Value << mc.useInstance;
			_out << YAML::Key << "castDepth" << YAML::Value << mc.castDepth;
			

            _out << YAML::EndMap;
		}
	}
}