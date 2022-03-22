#include "ECS.hpp"

ECS::~ECS()
{
	for(std::map<u_int32_t, std::vector<u_int8_t>>::iterator it = components.begin(); it != components.end(); ++it) {
		size_t typeSize = BaseECSComponent::GetTypeSize(it->first);
		ECSComponentFreeFunction freefn = BaseECSComponent::GetTypeFreeFunction(it->first);
		for(u_int32_t i = 0; i < it->second.size(); i += typeSize) {
			freefn((BaseECSComponent*)&it->second[i]);
		}
	}

	for(u_int32_t i = 0; i < entities.size(); i++) {
		delete entities[i];
	}
}

EntityHandle ECS::MakeEntity(BaseECSComponent** entityComponents, const u_int32_t* componentIDs, size_t numComponents)
{
	std::pair<u_int32_t, std::vector<std::pair<u_int32_t, u_int32_t> > >* newEntity = new std::pair<u_int32_t, std::vector<std::pair<u_int32_t, u_int32_t> > >();
	EntityHandle handle = (EntityHandle)newEntity;
	for(u_int32_t i = 0; i < numComponents; i++) {
		if(!BaseECSComponent::IsTypeValid(componentIDs[i])) {
			//DEBUG_LOG("ECS", LOG_ERROR, "'%u' is not a valid component type.", componentIDs[i]);
			delete newEntity;
			return NULL_ENTITY_HANDLE;
		}

		AddComponentInternal(handle, newEntity->second, componentIDs[i], entityComponents[i]);
	}

	newEntity->first = entities.size();
	entities.push_back(newEntity);

	for(u_int32_t i = 0; i < listeners.size(); i++) {
		bool isValid = true;
		if(listeners[i]->ShouldNotifyOnAllEntityOperations()) {
			listeners[i]->OnMakeEntity(handle);
		} else {
			for(u_int32_t j = 0; j < listeners[i]->GetComponentIDs().size(); j++) {
				bool hasComponent = false;
				for(u_int32_t k = 0; k < numComponents; k++) {
					if(listeners[i]->GetComponentIDs()[j] == componentIDs[k]) {
						hasComponent = true;
						break;
					}
				}
				if(!hasComponent) {
					isValid = false;
					break;
				}
			}
			if(isValid) {
				listeners[i]->OnMakeEntity(handle);
			}
		}
	}

	return handle;
}

void ECS::RemoveEntity(EntityHandle handle)
{
	std::vector<std::pair<u_int32_t, u_int32_t> >& entity = HandleToEntity(handle);

	for(u_int32_t i = 0; i < listeners.size(); i++) {
		const std::vector<u_int32_t>& componentIDs = listeners[i]->GetComponentIDs();
		bool isValid = true;
		if(listeners[i]->ShouldNotifyOnAllEntityOperations()) {
			listeners[i]->OnRemoveEntity(handle);
		} else {
			for(u_int32_t j = 0; j < componentIDs.size(); j++) {
				bool hasComponent = false;
				for(u_int32_t k = 0; k < entity.size(); k++) {
					if(componentIDs[j] == entity[k].first) {
						hasComponent = true;
						break;
					}
				}
				if(!hasComponent) {
					isValid = false;
					break;
				}
			}
			if(isValid) {
				listeners[i]->OnRemoveEntity(handle);
			}
		}
	}
	
	for(u_int32_t i = 0; i < entity.size(); i++) {
		DeleteComponent(entity[i].first, entity[i].second);
	}

	u_int32_t destIndex = HandleToEntityIndex(handle);
	u_int32_t srcIndex = entities.size() - 1;
	delete entities[destIndex];
	entities[destIndex] = entities[srcIndex];
	entities[destIndex]->first = destIndex;
	entities.pop_back();
}

void ECS::AddComponentInternal(EntityHandle handle, std::vector<std::pair<u_int32_t, u_int32_t> >& entity, u_int32_t componentID, BaseECSComponent* component)
{
	ECSComponentCreateFunction createfn = BaseECSComponent::GetTypeCreateFunction(componentID);
	std::pair<u_int32_t, u_int32_t> newPair;
	newPair.first = componentID;
	newPair.second = createfn(components[componentID], handle, component);
	entity.push_back(newPair);
}

void ECS::DeleteComponent(u_int32_t componentID, u_int32_t index)
{
	std::vector<u_int8_t>& array = components[componentID];
	ECSComponentFreeFunction freefn = BaseECSComponent::GetTypeFreeFunction(componentID);
	size_t typeSize = BaseECSComponent::GetTypeSize(componentID);
	u_int32_t srcIndex = array.size() - typeSize;

	BaseECSComponent* destComponent = (BaseECSComponent*)&array[index];
	BaseECSComponent* srcComponent = (BaseECSComponent*)&array[srcIndex];
	freefn(destComponent);

	if(index == srcIndex) {
		array.resize(srcIndex);
		return;
	}
	std::memcpy(destComponent, srcComponent, typeSize);

	std::vector<std::pair<u_int32_t, u_int32_t> >& srcComponents = HandleToEntity(srcComponent->entity);
	for(u_int32_t i = 0; i < srcComponents.size(); i++) {
		if(componentID == srcComponents[i].first && srcIndex == srcComponents[i].second) {
			srcComponents[i].second = index;
			break;
		}
	}

	array.resize(srcIndex);
}

bool ECS::RemoveComponentInternal(EntityHandle handle, u_int32_t componentID)
{
	std::vector<std::pair<u_int32_t, u_int32_t> >& entityComponents = HandleToEntity(handle);
	for(u_int32_t i = 0; i < entityComponents.size(); i++) {
		if(componentID == entityComponents[i].first) {
			DeleteComponent(entityComponents[i].first, entityComponents[i].second);
			u_int32_t srcIndex = entityComponents.size()-1;
			u_int32_t destIndex = i;
			entityComponents[destIndex] = entityComponents[srcIndex];
			entityComponents.pop_back();
			return true;
		}
	}
	return false;
}

BaseECSComponent* ECS::GetComponentInternal(std::vector<std::pair<u_int32_t, u_int32_t> >& entityComponents, std::vector<u_int8_t>& array, u_int32_t componentID)
{
	for(u_int32_t i = 0; i < entityComponents.size(); i++) {
		if(componentID == entityComponents[i].first) {
			return (BaseECSComponent*)&array[entityComponents[i].second];
		}
	}
	return nullptr;
}

void ECS::UpdateSystems(ECSSystemList& systems, float delta)
{
	std::vector<BaseECSComponent*> componentParam;
	std::vector<std::vector<u_int8_t>*> componentArrays;
	for(u_int32_t i = 0; i < systems.size(); i++) {
		const std::vector<u_int32_t>& componentTypes = systems[i]->GetComponentTypes();
		if(componentTypes.size() == 1) {
			size_t typeSize = BaseECSComponent::GetTypeSize(componentTypes[0]);
			std::vector<u_int8_t>& array = components[componentTypes[0]];
			for(u_int32_t j = 0; j < array.size(); j += typeSize) {
				BaseECSComponent* component = (BaseECSComponent*)&array[j];
				systems[i]->UpdateComponents(delta, &component);
			}
		} else {
			UpdateSystemWithMultipleComponents(i, systems, delta, componentTypes, componentParam, componentArrays);
		}
	}
}

u_int32_t ECS::FindLeastCommonComponent(const std::vector<u_int32_t>& componentTypes, const std::vector<u_int32_t>& componentFlags)
{
	u_int32_t minSize = (u_int32_t)-1;
	u_int32_t minIndex = (u_int32_t)-1;
	for(u_int32_t i = 0; i < componentTypes.size(); i++) {
		if((componentFlags[i] & BaseECSSystem::FLAG_OPTIONAL) != 0) {
			continue;
		}
		size_t typeSize = BaseECSComponent::GetTypeSize(componentTypes[i]);
		u_int32_t size = components[componentTypes[i]].size()/typeSize;
		if(size <= minSize) {
			minSize = size;
			minIndex = i;
		}
	}

	return minIndex;
}

void ECS::UpdateSystemWithMultipleComponents(u_int32_t index, ECSSystemList& systems, float delta,
		const std::vector<u_int32_t>& componentTypes, std::vector<BaseECSComponent*>& componentParam,
		std::vector<std::vector<u_int8_t>*>& componentArrays)
{
	const std::vector<u_int32_t>& componentFlags = systems[index]->GetComponentFlags();

	componentParam.resize(glm::max(componentParam.size(), componentTypes.size()));
	componentArrays.resize(glm::max(componentArrays.size(), componentTypes.size()));
	for(u_int32_t i = 0; i < componentTypes.size(); i++) {
		componentArrays[i] = &components[componentTypes[i]];
	}
	u_int32_t minSizeIndex = FindLeastCommonComponent(componentTypes, componentFlags);

	size_t typeSize = BaseECSComponent::GetTypeSize(componentTypes[minSizeIndex]);
	std::vector<u_int8_t>& array = *componentArrays[minSizeIndex];
	for(u_int32_t i = 0; i < array.size(); i += typeSize) {
		componentParam[minSizeIndex] = (BaseECSComponent*)&array[i];
		std::vector<std::pair<u_int32_t, u_int32_t> >& entityComponents =
			HandleToEntity(componentParam[minSizeIndex]->entity);

		bool isValid = true;
		for(u_int32_t j = 0; j < componentTypes.size(); j++) {
			if(j == minSizeIndex) {
				continue;
			}

			componentParam[j] = GetComponentInternal(entityComponents,
					*componentArrays[j], componentTypes[j]);
			if(componentParam[j] == nullptr && (componentFlags[j] & BaseECSSystem::FLAG_OPTIONAL) == 0) {
				isValid = false;
				break;
			}
		}

		if(isValid) {
			systems[index]->UpdateComponents(delta, &componentParam[0]);
		}
	}
}