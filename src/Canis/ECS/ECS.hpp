#pragma once

#define NULL_COPY_AND_ASSIGN(T) \
    T(const T& other) {(void)other;} \
    void operator=(const T& other) { (void)other; }

#include <map>
#include <vector>
#include <cstring>
#include <glm/glm.hpp>

#include "ECSComponent.hpp"
#include "ECSSystem.hpp"

class ECSListener
{
public:
	virtual void OnMakeEntity(EntityHandle handle) {}
	virtual void OnRemoveEntity(EntityHandle handle) {}
	virtual void OnAddComponent(EntityHandle handle, glm::uint32 id) {}
	virtual void OnRemoveComponent(EntityHandle handle, glm::uint32 id) {}

	const std::vector<glm::uint32>& GetComponentIDs() { 
		return componentIDs;
	}

	inline bool ShouldNotifyOnAllComponentOperations() {
		return notifyOnAllComponentOperations;
	}
	inline bool ShouldNotifyOnAllEntityOperations() {
		return notifyOnAllEntityOperations;
	}

protected:
	void SetNotificationSettings(
			bool ShouldNotifyOnAllComponentOperations,
			bool shouldNotifyOnAllEntityOperations) {
		notifyOnAllComponentOperations = ShouldNotifyOnAllComponentOperations;
		notifyOnAllEntityOperations = shouldNotifyOnAllEntityOperations;
	}
	void AddComponentID(glm::uint32 id) {
		componentIDs.push_back(id);
	}
private:
	std::vector<glm::uint32> componentIDs;
	bool notifyOnAllComponentOperations = false;
	bool notifyOnAllEntityOperations = false;
};

class ECS
{
public:
	ECS() {}
	~ECS();

	// ECSListener methods
	inline void AddListener(ECSListener* listener) {
		listeners.push_back(listener);
	}

	// Entity methods
	EntityHandle MakeEntity(BaseECSComponent** components, const glm::uint32* componentIDs, size_t numComponents);
	void RemoveEntity(EntityHandle handle);

	template<class A>
	EntityHandle MakeEntity(A& c1)
	{
		BaseECSComponent* components[] = { &c1 };
		glm::uint32 componentIDs[] = {A::ID};
		return MakeEntity(components, componentIDs, 1);
	}

	template<class A, class B>
	EntityHandle MakeEntity(A& c1, B& c2)
	{
		BaseECSComponent* components[] = { &c1, &c2 };
		glm::uint32 componentIDs[] = {A::ID, B::ID};
		return MakeEntity(components, componentIDs, 2);
	}

	template<class A, class B, class C>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3 };
		glm::uint32 componentIDs[] = {A::ID, B::ID, C::ID};
		return MakeEntity(components, componentIDs, 3);
	}

	template<class A, class B, class C, class D>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4 };
		glm::uint32 componentIDs[] = {A::ID, B::ID, C::ID, D::ID};
		return MakeEntity(components, componentIDs, 4);
	}

	template<class A, class B, class C, class D, class E>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5 };
		glm::uint32 componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID};
		return MakeEntity(components, componentIDs, 5);
	}

	template<class A, class B, class C, class D, class E, class F>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6};
		glm::uint32 componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID};
		return MakeEntity(components, componentIDs, 6);
	}

	template<class A, class B, class C, class D, class E, class F, class G>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6, G& c7)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6, &c7};
		glm::uint32 componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID, G::ID};
		return MakeEntity(components, componentIDs, 7);
	}

	template<class A, class B, class C, class D, class E, class F, class G, class H>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6, G& c7, H& c8)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8};
		glm::uint32 componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID, G::ID, H::ID};
		return MakeEntity(components, componentIDs, 8);
	}

	template<class A, class B, class C, class D, class E, class F, class G, class H, class I>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6, G& c7, H& c8, I& c9)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8, &c9 };
		glm::uint32 componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID, G::ID, H::ID, I::ID};
		return MakeEntity(components, componentIDs, 9);
	}

	template<class A, class B, class C, class D, class E, class F, class G, class H, class I, class J>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6, G& c7, H& c8, I& c9, J& c10)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8, &c9, &c10 };
		glm::uint32 componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID, G::ID, H::ID, I::ID, J::ID };
		return MakeEntity(components, componentIDs, 10);
	}



	// Component methods
	template<class Component>
	inline void AddComponent(EntityHandle entity, Component* component)
	{
		AddComponentInternal(entity, HandleToEntity(entity), Component::ID, component);
		for(glm::uint32 i = 0; i < listeners.size(); i++) {
			const std::vector<glm::uint32>& componentIDs = listeners[i]->GetComponentIDs();
			if(listeners[i]->ShouldNotifyOnAllComponentOperations()) {
				listeners[i]->OnAddComponent(entity, Component::ID);
			} else {
				for(glm::uint32 j = 0; j < componentIDs.size(); j++) {
					if(componentIDs[j] == Component::ID) {
						listeners[i]->OnAddComponent(entity, Component::ID);
						break;
					}
				}
			}
		}
	}

	template<class Component>
	bool RemoveComponent(EntityHandle entity)
	{
		for(glm::uint32 i = 0; i < listeners.size(); i++) {
			const std::vector<glm::uint32>& componentIDs = listeners[i]->GetComponentIDs();
			for(glm::uint32 j = 0; j < componentIDs.size(); j++) {
				if(listeners[i]->ShouldNotifyOnAllComponentOperations()) {
					listeners[i]->OnRemoveComponent(entity, Component::ID);
				} else {
					if(componentIDs[j] == Component::ID) {
						listeners[i]->OnRemoveComponent(entity, Component::ID);
						break;
					}
				}
			}
		}
		return RemoveComponentInternal(entity, Component::ID);
	}

	template<class Component>
	Component* GetComponent(EntityHandle entity)
	{
		return (Component*)GetComponentInternal(HandleToEntity(entity), components[Component::ID], Component::ID);
	}

	BaseECSComponent* GetComponentByType(EntityHandle entity, glm::uint32 componentID)
	{
		return GetComponentInternal(HandleToEntity(entity), components[componentID], componentID);
	}

	// System methods
	void UpdateSystems(ECSSystemList& systems, float delta);
	
private:
	std::map<glm::uint32, std::vector<glm::uint8>> components;
	std::vector<std::pair<glm::uint32, std::vector<std::pair<glm::uint32, glm::uint32> > >* > entities;
	std::vector<ECSListener*> listeners;

	inline std::pair<glm::uint32, std::vector<std::pair<glm::uint32, glm::uint32> > >* HandleToRawType(EntityHandle handle)
	{
		return (std::pair<glm::uint32, std::vector<std::pair<glm::uint32, glm::uint32> > >*)handle;
	}

	inline glm::uint32 HandleToEntityIndex(EntityHandle handle)
	{
		return HandleToRawType(handle)->first;
	}

	inline std::vector<std::pair<glm::uint32, glm::uint32> >& HandleToEntity(EntityHandle handle)
	{
		return HandleToRawType(handle)->second;
	}

	void DeleteComponent(glm::uint32 componentID, glm::uint32 index);
	bool RemoveComponentInternal(EntityHandle handle, glm::uint32 componentID);
	void AddComponentInternal(EntityHandle handle, std::vector<std::pair<glm::uint32, glm::uint32> >& entity, glm::uint32 componentID, BaseECSComponent* component);
	BaseECSComponent* GetComponentInternal(std::vector<std::pair<glm::uint32, glm::uint32> >& entityComponents, std::vector<glm::uint8>& array, glm::uint32 componentID);

	void UpdateSystemWithMultipleComponents(glm::uint32 index, ECSSystemList& systems, float delta, const std::vector<glm::uint32>& componentTypes,
			std::vector<BaseECSComponent*>& componentParam, std::vector<std::vector<glm::uint8>*>& componentArrays);
	glm::uint32 FindLeastCommonComponent(const std::vector<glm::uint32>& componentTypes, const std::vector<glm::uint32>& componentFlags);

	NULL_COPY_AND_ASSIGN(ECS);
};