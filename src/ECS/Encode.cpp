#include <Canis/ECS/Encode.hpp>

#include <Canis/ECS/Components/TransformComponent.hpp>

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
            _out << YAML::Key << "rotation" << YAML::Value << tc.rotation;
            _out << YAML::Key << "scale" << YAML::Value << tc.scale;

            _out << YAML::EndMap;
        }
    }
}