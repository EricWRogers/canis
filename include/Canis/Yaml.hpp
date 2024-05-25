#pragma once
#include <glm/glm.hpp>
#include <fstream>
#include <yaml-cpp/yaml.h>
#include <Canis/Data/Bit.hpp>
#include <Canis/UUID.hpp>
#include <Canis/External/entt.hpp>

#include <map>
#include <unordered_map>
#include <variant>
#include <type_traits>
#include <functional>
#include <string>
#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

namespace Canis {
	class Entity;
}

struct EntityData { 
	entt::entity entityHandle{ entt::null };
	void *scene = nullptr;
};

// Define PropertySetter as a function that takes a YAML node and a void pointer to the component
using PropertySetter = std::function<void(YAML::Node&, void*, void*)>;

// Define PropertyGetter as a function that takes a void pointer to the component and returns a YAML node
using PropertyGetter = std::function<YAML::Node(void*)>;

using AddComponentFunc = std::function<void(Canis::Entity&)>;
using RemoveComponentFunc = std::function<void(Canis::Entity&)>;
using HasComponentFunc = std::function<bool(Canis::Entity&)>;

using AddScriptableComponent = std::function<void(Canis::Entity&)>;
using RemoveScriptableComponent = std::function<void(Canis::Entity&)>;
using HasScriptableComponent = std::function<bool(Canis::Entity&)>;

// PropertyRegistry struct to hold the setters and getters for each property
struct PropertyRegistry {
    std::map<std::string, PropertySetter> setters;
    std::map<std::string, PropertyGetter> getters;
	std::vector<std::string> propertyOrder;
};

struct SystemRegistry {
	std::vector<std::string> updateSystems;
	std::vector<std::string> renderSystems;
};

struct ComponentRegistry
{
	std::vector<std::string> names;
	std::map<std::string, AddComponentFunc> addComponentFuncs;
    std::map<std::string, RemoveComponentFunc> removeComponentFuncs;
	std::map<std::string, HasComponentFunc> hasComponentFuncs;
};

struct ScriptableComponentRegistry
{
	std::vector<std::string> names;
	std::map<std::string, AddScriptableComponent> addScriptableComponent;
    std::map<std::string, RemoveScriptableComponent> removeScriptableComponent;
	std::map<std::string, HasScriptableComponent> hasScriptableComponent;
};

// Template declaration for GetPropertyRegistry
template <typename T>
PropertyRegistry& GetPropertyRegistry()
{
    static PropertyRegistry registry;
    return registry;
}

extern SystemRegistry& GetSystemRegistry();
extern ComponentRegistry& GetComponent();
extern ScriptableComponentRegistry& GetScriptableComponentRegistry();

namespace Canis
{
	void AddEntityAndUUIDToSceneManager(void *_entity, Canis::UUID _uuid, void *_sceneManager);
}

#define REGISTER_PROPERTY(component, property, type)                                                 						\
{                                                                                                    						\
	GetPropertyRegistry<component>().setters[#property] = [](YAML::Node &node, void *componentPtr, void *_sceneManager) { 	\
		if constexpr (!std::is_same_v<type, Canis::Entity>) {   															\
			static_cast<component *>(componentPtr)->property = node.as<type>();                  							\
		} 																													\
		else 																												\
		{ 																													\
			Canis::AddEntityAndUUIDToSceneManager( 																			\
				(void*)(&static_cast<component *>(componentPtr)->property), 												\
				node.as<Canis::UUID>(), 																					\
				_sceneManager); 																							\
		}  																													\
	};                                                                                       								\
    GetPropertyRegistry<component>().getters[#property] = [](void *componentPtr) -> YAML::Node {       						\
        if constexpr (!std::is_same_v<type, Canis::Entity>) {                                          						\
            return YAML::Node(static_cast<component *>(componentPtr)->property);                        					\
        } else {                                                                                       						\
            /* Assuming Canis::UUID has an encode method or operator<< defined */                         					\
            return YAML::Node(*(EntityData*)(void*)&(static_cast<component *>(componentPtr)->property));                    \
        }                                                                                              						\
    };                                                                                                						\
	GetPropertyRegistry<component>().propertyOrder.push_back(#property);  \
}



//#define REGISTER_PROPERTY_DEFAULT(component, property, type, defaultValue)                           \
//    GetPropertyRegistry<component>().setters[#property] = [](YAML::Node &node, void *componentPtr, void *_sceneManager) { \
//        static_cast<component *>(componentPtr)->property = node.as<type>(defaultValue);              \
//    };

namespace YAML
{
	Emitter &operator<<(Emitter &out, const glm::vec2 &v);

	Emitter &operator<<(Emitter &out, const glm::vec3 &v);

	Emitter &operator<<(Emitter &out, const glm::vec4 &v);

	Emitter &operator<<(Emitter &out, const EntityData &e);

	template <>
	struct convert<glm::vec2>
	{
		static Node encode(const glm::vec2 &rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node &node, glm::vec2 &rhs)
		{
			if (!node.IsSequence() || node.size() != 2)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			return true;
		}
	};

	template <>
	struct convert<glm::vec3>
	{
		static Node encode(const glm::vec3 &rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node &node, glm::vec3 &rhs)
		{
			if (!node.IsSequence() || node.size() != 3)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			return true;
		}
	};

	template <>
	struct convert<glm::vec4>
	{
		static Node encode(const glm::vec4 &rhs)
		{
			Node node;
			node.push_back(rhs.x);
			node.push_back(rhs.y);
			node.push_back(rhs.z);
			node.push_back(rhs.w);
			node.SetStyle(EmitterStyle::Flow);
			return node;
		}

		static bool decode(const Node &node, glm::vec4 &rhs)
		{
			if (!node.IsSequence() || node.size() != 4)
				return false;

			rhs.x = node[0].as<float>();
			rhs.y = node[1].as<float>();
			rhs.z = node[2].as<float>();
			rhs.w = node[3].as<float>();
			return true;
		}
	};

	template <>
	struct convert<Canis::UUID>
	{
		static Node encode(const Canis::UUID &_uuid)
		{
			Node node;
			node.push_back((uint64_t)_uuid);
			return node;
		}

		static bool decode(const Node &_node, Canis::UUID &_uuid)
		{
			_uuid = _node.as<uint64_t>();
			return true;
		}
	};

	extern uint64_t GetUUIDFromEntityData(const EntityData &_entityData);

	template <>
	struct convert<EntityData>
	{
		static Node encode(const EntityData &e)
		{
			Node node;
			node = std::to_string(GetUUIDFromEntityData(e));
			return node;
		}

		static bool decode(const Node &_node, EntityData &e)
		{
			e.entityHandle = (entt::entity)_node.as<uint32_t>();
			e.scene = (void*)_node.as<uint64_t>();
			return true;
		}
	};
} // end of YAML namespace