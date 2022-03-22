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
	virtual void OnAddComponent(EntityHandle handle, u_int32_t id) {}
	virtual void OnRemoveComponent(EntityHandle handle, u_int32_t id) {}

	const std::vector<u_int32_t>& GetComponentIDs() { 
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
	void AddComponentID(u_int32_t id) {
		componentIDs.push_back(id);
	}
private:
	std::vector<u_int32_t> componentIDs;
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
	EntityHandle MakeEntity(BaseECSComponent** components, const u_int32_t* componentIDs, size_t numComponents);
	void RemoveEntity(EntityHandle handle);

	template<class A>
	EntityHandle MakeEntity(A& c1)
	{
		BaseECSComponent* components[] = { &c1 };
		u_int32_t componentIDs[] = {A::ID};
		return MakeEntity(components, componentIDs, 1);
	}

	template<class A, class B>
	EntityHandle MakeEntity(A& c1, B& c2)
	{
		BaseECSComponent* components[] = { &c1, &c2 };
		u_int32_t componentIDs[] = {A::ID, B::ID};
		return MakeEntity(components, componentIDs, 2);
	}

	template<class A, class B, class C>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3 };
		u_int32_t componentIDs[] = {A::ID, B::ID, C::ID};
		return MakeEntity(components, componentIDs, 3);
	}

	template<class A, class B, class C, class D>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4 };
		u_int32_t componentIDs[] = {A::ID, B::ID, C::ID, D::ID};
		return MakeEntity(components, componentIDs, 4);
	}

	template<class A, class B, class C, class D, class E>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5 };
		u_int32_t componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID};
		return MakeEntity(components, componentIDs, 5);
	}

	template<class A, class B, class C, class D, class E, class F>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6};
		u_int32_t componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID};
		return MakeEntity(components, componentIDs, 6);
	}

	template<class A, class B, class C, class D, class E, class F, class G>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6, G& c7)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6, &c7};
		u_int32_t componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID, G::ID};
		return MakeEntity(components, componentIDs, 7);
	}

	template<class A, class B, class C, class D, class E, class F, class G, class H>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6, G& c7, H& c8)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8};
		u_int32_t componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID, G::ID, H::ID};
		return MakeEntity(components, componentIDs, 8);
	}

	template<class A, class B, class C, class D, class E, class F, class G, class H, class I>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6, G& c7, H& c8, I& c9)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8, &c9 };
		u_int32_t componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID, G::ID, H::ID, I::ID};
		return MakeEntity(components, componentIDs, 9);
	}

	template<class A, class B, class C, class D, class E, class F, class G, class H, class I, class J>
	EntityHandle MakeEntity(A& c1, B& c2, C& c3, D& c4, E& c5, F& c6, G& c7, H& c8, I& c9, J& c10)
	{
		BaseECSComponent* components[] = { &c1, &c2, &c3, &c4, &c5, &c6, &c7, &c8, &c9, &c10 };
		u_int32_t componentIDs[] = {A::ID, B::ID, C::ID, D::ID, E::ID, F::ID, G::ID, H::ID, I::ID, J::ID };
		return MakeEntity(components, componentIDs, 10);
	}



	// Component methods
	template<class Component>
	inline void AddComponent(EntityHandle entity, Component* component)
	{
		AddComponentInternal(entity, HandleToEntity(entity), Component::ID, component);
		for(u_int32_t i = 0; i < listeners.size(); i++) {
			const std::vector<u_int32_t>& componentIDs = listeners[i]->GetComponentIDs();
			if(listeners[i]->ShouldNotifyOnAllComponentOperations()) {
				listeners[i]->OnAddComponent(entity, Component::ID);
			} else {
				for(u_int32_t j = 0; j < componentIDs.size(); j++) {
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
		for(u_int32_t i = 0; i < listeners.size(); i++) {
			const std::vector<u_int32_t>& componentIDs = listeners[i]->GetComponentIDs();
			for(u_int32_t j = 0; j < componentIDs.size(); j++) {
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

	BaseECSComponent* GetComponentByType(EntityHandle entity, u_int32_t componentID)
	{
		return GetComponentInternal(HandleToEntity(entity), components[componentID], componentID);
	}

	// System methods
	void UpdateSystems(ECSSystemList& systems, float delta);
	
private:
	std::map<u_int32_t, std::vector<u_int8_t>> components;
	std::vector<std::pair<u_int32_t, std::vector<std::pair<u_int32_t, u_int32_t> > >* > entities;
	std::vector<ECSListener*> listeners;

	inline std::pair<u_int32_t, std::vector<std::pair<u_int32_t, u_int32_t> > >* HandleToRawType(EntityHandle handle)
	{
		return (std::pair<u_int32_t, std::vector<std::pair<u_int32_t, u_int32_t> > >*)handle;
	}

	inline u_int32_t HandleToEntityIndex(EntityHandle handle)
	{
		return HandleToRawType(handle)->first;
	}

	inline std::vector<std::pair<u_int32_t, u_int32_t> >& HandleToEntity(EntityHandle handle)
	{
		return HandleToRawType(handle)->second;
	}

	void DeleteComponent(u_int32_t componentID, u_int32_t index);
	bool RemoveComponentInternal(EntityHandle handle, u_int32_t componentID);
	void AddComponentInternal(EntityHandle handle, std::vector<std::pair<u_int32_t, u_int32_t> >& entity, u_int32_t componentID, BaseECSComponent* component);
	BaseECSComponent* GetComponentInternal(std::vector<std::pair<u_int32_t, u_int32_t> >& entityComponents, std::vector<u_int8_t>& array, u_int32_t componentID);

	void UpdateSystemWithMultipleComponents(u_int32_t index, ECSSystemList& systems, float delta, const std::vector<u_int32_t>& componentTypes,
			std::vector<BaseECSComponent*>& componentParam, std::vector<std::vector<u_int8_t>*>& componentArrays);
	u_int32_t FindLeastCommonComponent(const std::vector<u_int32_t>& componentTypes, const std::vector<u_int32_t>& componentFlags);

	NULL_COPY_AND_ASSIGN(ECS);
};