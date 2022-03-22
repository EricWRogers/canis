#pragma once
#include <vector>
#include <tuple>
#include <glm/glm.hpp>


struct BaseECSComponent;
typedef void* EntityHandle;
typedef glm::uint32 (*ECSComponentCreateFunction)(std::vector<glm::uint8>& memory, EntityHandle entity, BaseECSComponent* comp);
typedef void (*ECSComponentFreeFunction)(BaseECSComponent* comp);
#define NULL_ENTITY_HANDLE nullptr

struct BaseECSComponent
{
public:
	static glm::uint32 RegisterComponentType(ECSComponentCreateFunction createfn,
			ECSComponentFreeFunction freefn, size_t size);
	EntityHandle entity = NULL_ENTITY_HANDLE;

	inline static ECSComponentCreateFunction GetTypeCreateFunction(glm::uint32 id)
	{
		return std::get<0>((*componentTypes)[id]);
	}

	inline static ECSComponentFreeFunction GetTypeFreeFunction(glm::uint32 id)
	{
		return std::get<1>((*componentTypes)[id]);
	}

	inline static size_t GetTypeSize(glm::uint32 id)
	{
		return std::get<2>((*componentTypes)[id]);
	}

	inline static bool IsTypeValid(glm::uint32 id)
	{
		return id < componentTypes->size();
	}
private:
	static std::vector<std::tuple<ECSComponentCreateFunction, ECSComponentFreeFunction, size_t> >* componentTypes;
};

template<typename T>
struct ECSComponent : public BaseECSComponent
{
	static const ECSComponentCreateFunction CREATE_FUNCTION;
	static const ECSComponentFreeFunction FREE_FUNCTION;
	static const glm::uint32 ID;
	static const size_t SIZE; 
};

template<typename Component>
glm::uint32 ECSComponentCreate(std::vector<glm::uint8>& memory, EntityHandle entity, BaseECSComponent* comp)
{
	glm::uint32 index = memory.size();
	memory.resize(index+Component::SIZE);
	Component* component = new(&memory[index])Component(*(Component*)comp);
	component->entity = entity;
	return index;
}

template<typename Component>
void ECSComponentFree(BaseECSComponent* comp)
{
	Component* component = (Component*)comp;
	component->~Component();
}

template<typename T>
const glm::uint32 ECSComponent<T>::ID(BaseECSComponent::RegisterComponentType(ECSComponentCreate<T>, ECSComponentFree<T>, sizeof(T)));

template<typename T>
const size_t ECSComponent<T>::SIZE(sizeof(T));

template<typename T>
const ECSComponentCreateFunction ECSComponent<T>::CREATE_FUNCTION(ECSComponentCreate<T>);

template<typename T>
const ECSComponentFreeFunction ECSComponent<T>::FREE_FUNCTION(ECSComponentFree<T>);
