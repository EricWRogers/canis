#include "ECS.hpp"

ECS::~ECS()
{
	for(std::map<glm::uint32, std::vector<glm::uint8>>::iterator it = components.begin(); it != components.end(); ++it) {
		size_t typeSize = BaseECSComponent::GetTypeSize(it->first);
		ECSComponentFreeFunction freefn = BaseECSComponent::GetTypeFreeFunction(it->first);
		for(glm::uint32 i = 0; i < it->second.size(); i += typeSize) {
			freefn((BaseECSComponent*)&it->second[i]);
		}
	}

	for(glm::uint32 i = 0; i < entities.size(); i++) {
		delete entities[i];
	}
}

EntityHandle ECS::MakeEntity(BaseECSComponent** entityComponents, const glm::uint32* componentIDs, size_t numComponents)
{
	std::pair<glm::uint32, std::vector<std::pair<glm::uint32, glm::uint32> > >* newEntity = new std::pair<glm::uint32, std::vector<std::pair<glm::uint32, glm::uint32> > >();
	EntityHandle handle = (EntityHandle)newEntity;
	for(glm::uint32 i = 0; i < numComponents; i++) {
		if(!BaseECSComponent::IsTypeValid(componentIDs[i])) {
			//DEBUG_LOG("ECS", LOG_ERROR, "'%u' is not a valid component type.", componentIDs[i]);
			delete newEntity;
			return NULL_ENTITY_HANDLE;
		}

		AddComponentInternal(handle, newEntity->second, componentIDs[i], entityComponents[i]);
	}

	newEntity->first = entities.size();
	entities.push_back(newEntity);

	for(glm::uint32 i = 0; i < listeners.size(); i++) {
		bool isValid = true;
		if(listeners[i]->ShouldNotifyOnAllEntityOperations()) {
			listeners[i]->OnMakeEntity(handle);
		} else {
			for(glm::uint32 j = 0; j < listeners[i]->GetComponentIDs().size(); j++) {
				bool hasComponent = false;
				for(glm::uint32 k = 0; k < numComponents; k++) {
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
	std::vector<std::pair<glm::uint32, glm::uint32> >& entity = HandleToEntity(handle);

	for(glm::uint32 i = 0; i < listeners.size(); i++) {
		const std::vector<glm::uint32>& componentIDs = listeners[i]->GetComponentIDs();
		bool isValid = true;
		if(listeners[i]->ShouldNotifyOnAllEntityOperations()) {
			listeners[i]->OnRemoveEntity(handle);
		} else {
			for(glm::uint32 j = 0; j < componentIDs.size(); j++) {
				bool hasComponent = false;
				for(glm::uint32 k = 0; k < entity.size(); k++) {
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
	
	for(glm::uint32 i = 0; i < entity.size(); i++) {
		DeleteComponent(entity[i].first, entity[i].second);
	}

	glm::uint32 destIndex = HandleToEntityIndex(handle);
	glm::uint32 srcIndex = entities.size() - 1;
	delete entities[destIndex];
	entities[destIndex] = entities[srcIndex];
	entities[destIndex]->first = destIndex;
	entities.pop_back();
}

void ECS::AddComponentInternal(EntityHandle handle, std::vector<std::pair<glm::uint32, glm::uint32> >& entity, glm::uint32 componentID, BaseECSComponent* component)
{
	ECSComponentCreateFunction createfn = BaseECSComponent::GetTypeCreateFunction(componentID);
	std::pair<glm::uint32, glm::uint32> newPair;
	newPair.first = componentID;
	newPair.second = createfn(components[componentID], handle, component);
	entity.push_back(newPair);
}

void ECS::DeleteComponent(glm::uint32 componentID, glm::uint32 index)
{
	std::vector<glm::uint8>& array = components[componentID];
	ECSComponentFreeFunction freefn = BaseECSComponent::GetTypeFreeFunction(componentID);
	size_t typeSize = BaseECSComponent::GetTypeSize(componentID);
	glm::uint32 srcIndex = array.size() - typeSize;

	BaseECSComponent* destComponent = (BaseECSComponent*)&array[index];
	BaseECSComponent* srcComponent = (BaseECSComponent*)&array[srcIndex];
	freefn(destComponent);

	if(index == srcIndex) {
		array.resize(srcIndex);
		return;
	}
	std::memcpy(destComponent, srcComponent, typeSize);

	std::vector<std::pair<glm::uint32, glm::uint32> >& srcComponents = HandleToEntity(srcComponent->entity);
	for(glm::uint32 i = 0; i < srcComponents.size(); i++) {
		if(componentID == srcComponents[i].first && srcIndex == srcComponents[i].second) {
			srcComponents[i].second = index;
			break;
		}
	}

	array.resize(srcIndex);
}

bool ECS::RemoveComponentInternal(EntityHandle handle, glm::uint32 componentID)
{
	std::vector<std::pair<glm::uint32, glm::uint32> >& entityComponents = HandleToEntity(handle);
	for(glm::uint32 i = 0; i < entityComponents.size(); i++) {
		if(componentID == entityComponents[i].first) {
			DeleteComponent(entityComponents[i].first, entityComponents[i].second);
			glm::uint32 srcIndex = entityComponents.size()-1;
			glm::uint32 destIndex = i;
			entityComponents[destIndex] = entityComponents[srcIndex];
			entityComponents.pop_back();
			return true;
		}
	}
	return false;
}

BaseECSComponent* ECS::GetComponentInternal(std::vector<std::pair<glm::uint32, glm::uint32> >& entityComponents, std::vector<glm::uint8>& array, glm::uint32 componentID)
{
	for(glm::uint32 i = 0; i < entityComponents.size(); i++) {
		if(componentID == entityComponents[i].first) {
			return (BaseECSComponent*)&array[entityComponents[i].second];
		}
	}
	return nullptr;
}

void ECS::UpdateSystems(ECSSystemList& systems, float delta)
{
	std::vector<BaseECSComponent*> componentParam;
	std::vector<std::vector<glm::uint8>*> componentArrays;
	for(glm::uint32 i = 0; i < systems.size(); i++) {
		const std::vector<glm::uint32>& componentTypes = systems[i]->GetComponentTypes();
		if(componentTypes.size() == 1) {
			size_t typeSize = BaseECSComponent::GetTypeSize(componentTypes[0]);
			std::vector<glm::uint8>& array = components[componentTypes[0]];
			systems[i]->BeginUpdateComponents();
			for(glm::uint32 j = 0; j < array.size(); j += typeSize) {
				BaseECSComponent* component = (BaseECSComponent*)&array[j];
				systems[i]->UpdateComponents(delta, &component);
			}
			systems[i]->EndUpdateComponents();
		} else {
			UpdateSystemWithMultipleComponents(i, systems, delta, componentTypes, componentParam, componentArrays);
		}
	}
}

glm::uint32 ECS::FindLeastCommonComponent(const std::vector<glm::uint32>& componentTypes, const std::vector<glm::uint32>& componentFlags)
{
	glm::uint32 minSize = (glm::uint32)-1;
	glm::uint32 minIndex = (glm::uint32)-1;
	for(glm::uint32 i = 0; i < componentTypes.size(); i++) {
		if((componentFlags[i] & BaseECSSystem::FLAG_OPTIONAL) != 0) {
			continue;
		}
		size_t typeSize = BaseECSComponent::GetTypeSize(componentTypes[i]);
		glm::uint32 size = components[componentTypes[i]].size()/typeSize;
		if(size <= minSize) {
			minSize = size;
			minIndex = i;
		}
	}

	return minIndex;
}

void ECS::UpdateSystemWithMultipleComponents(glm::uint32 index, ECSSystemList& systems, float delta,
		const std::vector<glm::uint32>& componentTypes, std::vector<BaseECSComponent*>& componentParam,
		std::vector<std::vector<glm::uint8>*>& componentArrays)
{
	systems[index]->BeginUpdateComponents();
	const std::vector<glm::uint32>& componentFlags = systems[index]->GetComponentFlags();

	componentParam.resize(glm::max(componentParam.size(), componentTypes.size()));
	componentArrays.resize(glm::max(componentArrays.size(), componentTypes.size()));
	for(glm::uint32 i = 0; i < componentTypes.size(); i++) {
		componentArrays[i] = &components[componentTypes[i]];
	}
	glm::uint32 minSizeIndex = FindLeastCommonComponent(componentTypes, componentFlags);

	size_t typeSize = BaseECSComponent::GetTypeSize(componentTypes[minSizeIndex]);
	std::vector<glm::uint8>& array = *componentArrays[minSizeIndex];
	for(glm::uint32 i = 0; i < array.size(); i += typeSize) {
		componentParam[minSizeIndex] = (BaseECSComponent*)&array[i];
		std::vector<std::pair<glm::uint32, glm::uint32> >& entityComponents =
			HandleToEntity(componentParam[minSizeIndex]->entity);

		bool isValid = true;
		for(glm::uint32 j = 0; j < componentTypes.size(); j++) {
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
	systems[index]->EndUpdateComponents();
}