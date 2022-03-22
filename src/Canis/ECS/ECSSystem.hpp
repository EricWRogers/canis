#pragma once

#include <vector>

#include "ECSComponent.hpp"

class BaseECSSystem
{
public:
	enum
	{
		FLAG_OPTIONAL = 1,
	};
	BaseECSSystem() {}
	virtual void UpdateComponents(float delta, BaseECSComponent** components) {}
	const std::vector<u_int32_t>& GetComponentTypes()
	{
		return componentTypes;
	}
	const std::vector<u_int32_t>& GetComponentFlags()
	{
		return componentFlags;
	}
	bool IsValid();
protected:
	void AddComponentType(u_int32_t componentType, u_int32_t componentFlag = 0)
	{
		componentTypes.push_back(componentType);
		componentFlags.push_back(componentFlag);
	}
private:
	std::vector<u_int32_t> componentTypes;
	std::vector<u_int32_t> componentFlags;
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
	inline BaseECSSystem* operator[](u_int32_t index) {
		return systems[index];
	}
	bool RemoveSystem(BaseECSSystem& system);
private:
	std::vector<BaseECSSystem*> systems;
};