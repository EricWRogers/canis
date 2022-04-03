#pragma once

#include <vector>
#include <glm/glm.hpp>

#include "ECSComponent.hpp"

class BaseECSSystem
{
public:
	enum
	{
		FLAG_OPTIONAL = 1,
	};
	BaseECSSystem() {}
	virtual void BeginUpdateComponents() {}
	virtual void UpdateComponents(float delta, BaseECSComponent** components) {}
	virtual void EndUpdateComponents() {}
	const std::vector<glm::uint32>& GetComponentTypes()
	{
		return componentTypes;
	}
	const std::vector<glm::uint32>& GetComponentFlags()
	{
		return componentFlags;
	}
	bool IsValid();
protected:
	void AddComponentType(glm::uint32 componentType, glm::uint32 componentFlag = 0)
	{
		componentTypes.push_back(componentType);
		componentFlags.push_back(componentFlag);
	}
private:
	std::vector<glm::uint32> componentTypes;
	std::vector<glm::uint32> componentFlags;
};

class ECSSystemList
{
public:
	inline bool AddSystem(BaseECSSystem& system)
	{
		if(!system.IsValid()) {
			return false;
		}
		systems.push_back(&system);
		return true;
	}
	inline size_t size() {
		return systems.size();
	}
	inline BaseECSSystem* operator[](glm::uint32 index) {
		return systems[index];
	}
	bool RemoveSystem(BaseECSSystem& system);
private:
	std::vector<BaseECSSystem*> systems;
};