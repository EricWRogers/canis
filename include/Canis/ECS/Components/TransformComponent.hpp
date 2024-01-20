#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace Canis
{
	struct TransformComponent
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
	};
} // end of Canis namespace