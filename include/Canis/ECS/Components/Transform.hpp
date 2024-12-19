#pragma once
#include <vector>
#include <Canis/Yaml.hpp>

namespace Canis
{
	struct Transform
	{
		entt::registry* registry = nullptr;
		bool active = true;
		glm::vec3 position = glm::vec3(0.0f);
		glm::quat rotation = glm::quat(glm::vec3(0.0f, 0.0f, 0.0f));
		glm::vec3 scale = glm::vec3(1.0f);
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		bool isDirty = true;
		entt::entity parent = entt::null;
		std::vector<entt::entity> children;

		static void RegisterProperties()
		{
			REGISTER_PROPERTY(Transform, active, bool);
			REGISTER_PROPERTY(Transform, position, glm::vec3);
			REGISTER_PROPERTY(Transform, rotation, glm::quat);
			REGISTER_PROPERTY(Transform, scale, glm::vec3);
		}
	};
} // end of Canis namespace