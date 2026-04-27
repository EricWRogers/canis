#pragma once

#include <Canis/ConfigData.hpp>
#include <Canis/Entity.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace Canis
{
    class App;

    enum class ManagedPropertyType
    {
        None = 0,
        Bool,
        Int,
        Float,
        String,
        Vec2,
        Vec3,
        Vec4,
        Color,
    };

    using ManagedPropertyValue = std::variant<std::monostate, bool, int, float, std::string, Vector2, Vector3, Vector4>;

    struct ManagedPropertyDefinition
    {
        std::string name = "";
        ManagedPropertyType type = ManagedPropertyType::None;
        ManagedPropertyValue defaultValue = {};
    };

    struct ManagedScriptDefinition
    {
        std::string scriptName = "";
        std::string typeName = "";
        std::string assemblyPath = "";
        std::string sourcePath = "";
        std::vector<ManagedPropertyDefinition> properties = {};
    };

    class ManagedScriptComponent : public ScriptableEntity
    {
    public:
        explicit ManagedScriptComponent(Entity &_entity, std::shared_ptr<const ManagedScriptDefinition> _definition);

        const ManagedScriptDefinition* GetDefinition() const { return m_definition.get(); }
        const std::string& GetScriptName() const;
        const std::string& GetManagedTypeName() const;
        const std::string& GetManagedAssemblyPath() const;
        bool IsManagedRuntimeLive() const { return m_managedRuntimeLive; }

        void ResetToDefaults();
        bool HasProperty(const std::string &_name) const;
        ManagedPropertyValue* GetPropertyValue(const std::string &_name);
        const ManagedPropertyValue* GetPropertyValue(const std::string &_name) const;
        bool SetPropertyValue(const std::string &_name, const ManagedPropertyValue &_value);

        YAML::Node GetPropertyAsYaml(const std::string &_name) const;
        bool SetPropertyFromYaml(const std::string &_name, const YAML::Node &_node);

        void Create() override;
        void Ready() override;
        void Destroy() override;
        void Update(float _dt) override;

    private:
        std::shared_ptr<const ManagedScriptDefinition> m_definition = {};
        std::unordered_map<std::string, ManagedPropertyValue> m_propertyValues = {};
        bool m_managedRuntimeLive = false;

        void SyncPropertiesToManaged();
        void SyncPropertiesFromManaged();
    };

    class ManagedScriptRuntime
    {
    public:
        ManagedScriptRuntime() = default;
        ~ManagedScriptRuntime();

        void Initialize(App &_app);
        void Shutdown();

        bool IsManagedHostAvailable() const { return m_hostAvailable; }
        const std::vector<std::shared_ptr<const ManagedScriptDefinition>>& GetDefinitions() const { return m_definitions; }

        bool CreateInstance(ManagedScriptComponent &_script);
        void InvokeCreate(ManagedScriptComponent &_script);
        void ReadyInstance(ManagedScriptComponent &_script);
        void DestroyInstance(ManagedScriptComponent &_script);
        void UpdateInstance(ManagedScriptComponent &_script, float _dt);
        bool SyncPropertyToManaged(ManagedScriptComponent &_script, const ManagedPropertyDefinition &_definition, const ManagedPropertyValue &_value);
        bool SyncPropertyFromManaged(ManagedScriptComponent &_script, const ManagedPropertyDefinition &_definition, ManagedPropertyValue &_value);

    private:
        App* m_app = nullptr;
        std::vector<std::shared_ptr<const ManagedScriptDefinition>> m_definitions = {};
        std::vector<ScriptConf> m_registeredScriptConfs = {};
        bool m_hostAvailable = false;
        bool m_hostInitialized = false;
        bool m_loggedUnavailable = false;
        std::string m_managedRootPath = "";
        std::string m_hostAssemblyPath = "";
        std::string m_runtimeConfigPath = "";

        struct ManagedHostRuntimeHandle;
        ManagedHostRuntimeHandle* m_host = nullptr;

        void DiscoverScriptDefinitions();
        void RegisterDiscoveredScripts();
        void TryInitializeManagedHost();
        std::shared_ptr<const ManagedScriptDefinition> FindDefinition(const std::string &_scriptName) const;
        void RegisterScriptDefinition(const std::shared_ptr<const ManagedScriptDefinition> &_definition);
    };

    const char* ManagedPropertyTypeLabel(ManagedPropertyType _type);
    ManagedPropertyType ManagedPropertyTypeFromString(const std::string &_typeName);
    bool ManagedPropertyValueFromYaml(ManagedPropertyType _type, const YAML::Node &_node, ManagedPropertyValue &_outValue);
    YAML::Node ManagedPropertyValueToYaml(const ManagedPropertyValue &_value);
    bool ManagedPropertyValueSupportsAnimation(ManagedPropertyType _type);
    AnimationValueType ManagedPropertyAnimationType(ManagedPropertyType _type);
    AnimationInterpolation ManagedPropertyAnimationInterpolation(ManagedPropertyType _type);
    AnimationValue ManagedPropertyToAnimationValue(const ManagedPropertyValue &_value, ManagedPropertyType _type);
    bool ManagedPropertyFromAnimationValue(const AnimationValue &_value, ManagedPropertyType _type, ManagedPropertyValue &_inOutValue);
    bool ManagedPropertyValuesDiffer(const ManagedPropertyValue &_left, const ManagedPropertyValue &_right, ManagedPropertyType _type);
}
