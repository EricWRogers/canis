#pragma once
#include <vector>
#include <tuple>


struct BaseECSComponent;
typedef void* EntityHandle;
typedef u_int32_t (*ECSComponentCreateFunction)(std::vector<u_int8_t>& memory, EntityHandle entity, BaseECSComponent* comp);
typedef void (*ECSComponentFreeFunction)(BaseECSComponent* comp);
#define NULL_ENTITY_HANDLE nullptr

struct BaseECSComponent
{
public:
	static u_int32_t RegisterComponentType(ECSComponentCreateFunction createfn,
			ECSComponentFreeFunction freefn, size_t size);
	EntityHandle entity = NULL_ENTITY_HANDLE;

	inline static ECSComponentCreateFunction GetTypeCreateFunction(u_int32_t id)
	{
		return std::get<0>((*componentTypes)[id]);
	}

	inline static ECSComponentFreeFunction GetTypeFreeFunction(u_int32_t id)
	{
		return std::get<1>((*componentTypes)[id]);
	}

	inline static size_t GetTypeSize(u_int32_t id)
	{
		return std::get<2>((*componentTypes)[id]);
	}

	inline static bool IsTypeValid(u_int32_t id)
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
	static const u_int32_t ID;
	static const size_t SIZE; 
};

template<typename Component>
u_int32_t ECSComponentCreate(std::vector<u_int8_t>& memory, EntityHandle entity, BaseECSComponent* comp)
{
	u_int32_t index = memory.size();
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
const u_int32_t ECSComponent<T>::ID(BaseECSComponent::RegisterComponentType(ECSComponentCreate<T>, ECSComponentFree<T>, sizeof(T)));

template<typename T>
const size_t ECSComponent<T>::SIZE(sizeof(T));

template<typename T>
const ECSComponentCreateFunction ECSComponent<T>::CREATE_FUNCTION(ECSComponentCreate<T>);

template<typename T>
const ECSComponentFreeFunction ECSComponent<T>::FREE_FUNCTION(ECSComponentFree<T>);
