#include <Canis/ManagedScripting.hpp>

#include <Canis/App.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Debug.hpp>

#include <SDL3/SDL_timer.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <cstring>

#if defined(_WIN32)
#include <SDL3/SDL_loadso.h>
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace Canis
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* kManagedScriptsRoot = "csharp/managed";
        constexpr const char* kLegacyManagedScriptsRoot = "assets/managed";
        constexpr const char* kManagedHostAssemblyPath = "csharp/managed/Canis.ManagedHost.dll";
        constexpr const char* kLegacyManagedHostAssemblyPath = "assets/managed/Canis.ManagedHost.dll";
        constexpr const char* kManagedHostRuntimeConfigPath = "csharp/managed/Canis.ManagedHost.runtimeconfig.json";
        constexpr const char* kLegacyManagedHostRuntimeConfigPath = "assets/managed/Canis.ManagedHost.runtimeconfig.json";

        std::string ToLowerCopy(std::string _value)
        {
            std::transform(_value.begin(), _value.end(), _value.begin(), [](unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            });
            return _value;
        }

        ManagedPropertyValue DefaultManagedValueForType(const ManagedPropertyType _type)
        {
            switch (_type)
            {
                case ManagedPropertyType::Bool:   return false;
                case ManagedPropertyType::Int:    return 0;
                case ManagedPropertyType::Float:  return 0.0f;
                case ManagedPropertyType::String: return std::string();
                case ManagedPropertyType::Vec2:   return Vector2(0.0f);
                case ManagedPropertyType::Vec3:   return Vector3(0.0f);
                case ManagedPropertyType::Vec4:   return Vector4(0.0f);
                case ManagedPropertyType::Color:  return Vector4(1.0f);
                default:                          return {};
            }
        }

        std::string ResolveManagedAssetPath(const std::string& _path)
        {
            if (_path.empty())
                return "";

            fs::path path(_path);
            if (path.is_absolute())
                return path.lexically_normal().generic_string();

            return (fs::current_path() / path).lexically_normal().generic_string();
        }

        std::string ResolvePreferredManagedPath(const std::string& _primaryPath, const std::string& _legacyPath)
        {
            const std::string resolvedPrimary = ResolveManagedAssetPath(_primaryPath);
            if (!resolvedPrimary.empty() && fs::exists(resolvedPrimary))
                return resolvedPrimary;

            const std::string resolvedLegacy = ResolveManagedAssetPath(_legacyPath);
            if (!resolvedLegacy.empty() && fs::exists(resolvedLegacy))
                return resolvedLegacy;

            return resolvedPrimary;
        }

        std::string ResolveManagedRuntimePath(const std::string& _path)
        {
            if (_path.empty())
                return "";

            const std::string resolved = ResolveManagedAssetPath(_path);
            if (fs::exists(resolved))
                return resolved;

            constexpr std::string_view legacyPrefix = "assets/managed/";
            constexpr std::string_view primaryPrefix = "csharp/managed/";
            if (_path.rfind(legacyPrefix.data(), 0) == 0)
            {
                std::string remapped = std::string(primaryPrefix) + _path.substr(legacyPrefix.size());
                const std::string resolvedRemapped = ResolveManagedAssetPath(remapped);
                if (fs::exists(resolvedRemapped))
                    return resolvedRemapped;
            }

            return resolved;
        }

        const std::string& EmptyStringRef()
        {
            static const std::string value = "";
            return value;
        }

        bool ReadUtf8File(const fs::path& _path, std::string& _outContents)
        {
            std::ifstream input(_path, std::ios::in | std::ios::binary);
            if (!input.is_open())
                return false;

            std::ostringstream stream;
            stream << input.rdbuf();
            _outContents = stream.str();
            return true;
        }

        template <typename ValueType>
        bool TryGetManagedValue(const ManagedPropertyValue& _value, ValueType& _outValue)
        {
            if (const ValueType* casted = std::get_if<ValueType>(&_value))
            {
                _outValue = *casted;
                return true;
            }

            return false;
        }

        Vector4 ManagedColorVectorFromValue(const ManagedPropertyValue& _value, const Vector4& _fallback = Vector4(1.0f))
        {
            if (const Vector4* typedValue = std::get_if<Vector4>(&_value))
                return *typedValue;

            return _fallback;
        }

        struct ManagedNativeCallbacks
        {
            using LogFn = void (*)(const char*);
            using HasTransformFn = int (*)(uint64_t);
            using GetTransformVec3Fn = int (*)(uint64_t, float*, float*, float*);
            using SetTransformVec3Fn = int (*)(uint64_t, float, float, float);
            using HasComponentFn = int (*)(uint64_t, const char*);
            using GetComponentBoolFn = int (*)(uint64_t, const char*, const char*, int*);
            using SetComponentBoolFn = int (*)(uint64_t, const char*, const char*, int);
            using GetComponentIntFn = int (*)(uint64_t, const char*, const char*, int*);
            using SetComponentIntFn = int (*)(uint64_t, const char*, const char*, int);
            using GetComponentFloatFn = int (*)(uint64_t, const char*, const char*, float*);
            using SetComponentFloatFn = int (*)(uint64_t, const char*, const char*, float);
            using GetComponentStringFn = int (*)(uint64_t, const char*, const char*, char*, int);
            using SetComponentStringFn = int (*)(uint64_t, const char*, const char*, const char*);
            using GetComponentVec2Fn = int (*)(uint64_t, const char*, const char*, float*, float*);
            using SetComponentVec2Fn = int (*)(uint64_t, const char*, const char*, float, float);
            using GetComponentVec3Fn = int (*)(uint64_t, const char*, const char*, float*, float*, float*);
            using SetComponentVec3Fn = int (*)(uint64_t, const char*, const char*, float, float, float);
            using GetComponentVec4Fn = int (*)(uint64_t, const char*, const char*, float*, float*, float*, float*);
            using SetComponentVec4Fn = int (*)(uint64_t, const char*, const char*, float, float, float, float);

            LogFn logInfo = nullptr;
            HasTransformFn hasTransform = nullptr;
            GetTransformVec3Fn getTransformPosition = nullptr;
            GetTransformVec3Fn getTransformRotation = nullptr;
            GetTransformVec3Fn getTransformScale = nullptr;
            SetTransformVec3Fn setTransformPosition = nullptr;
            SetTransformVec3Fn setTransformRotation = nullptr;
            SetTransformVec3Fn setTransformScale = nullptr;
            HasComponentFn hasComponent = nullptr;
            GetComponentBoolFn getComponentBool = nullptr;
            SetComponentBoolFn setComponentBool = nullptr;
            GetComponentIntFn getComponentInt = nullptr;
            SetComponentIntFn setComponentInt = nullptr;
            GetComponentFloatFn getComponentFloat = nullptr;
            SetComponentFloatFn setComponentFloat = nullptr;
            GetComponentStringFn getComponentString = nullptr;
            SetComponentStringFn setComponentString = nullptr;
            GetComponentVec2Fn getComponentVec2 = nullptr;
            SetComponentVec2Fn setComponentVec2 = nullptr;
            GetComponentVec3Fn getComponentVec3 = nullptr;
            SetComponentVec3Fn setComponentVec3 = nullptr;
            GetComponentVec4Fn getComponentVec4 = nullptr;
            SetComponentVec4Fn setComponentVec4 = nullptr;
        };

        App* g_managedCallbackApp = nullptr;

        struct ManagedComponentAccess
        {
            Entity* entity = nullptr;
            ComponentConf* conf = nullptr;
            void* componentPtr = nullptr;
        };

        Entity* ResolveManagedCallbackEntity(uint64_t _entityUuid)
        {
            if (g_managedCallbackApp == nullptr)
                return nullptr;

            return g_managedCallbackApp->scene.GetEntityWithUUID(static_cast<UUID>(_entityUuid));
        }

        bool ResolveManagedComponentAccess(uint64_t _entityUuid, const char* _componentName, ManagedComponentAccess& _outAccess)
        {
            _outAccess = {};
            if (_componentName == nullptr || _componentName[0] == '\0' || g_managedCallbackApp == nullptr)
                return false;

            Entity* entity = ResolveManagedCallbackEntity(_entityUuid);
            if (entity == nullptr)
                return false;

            ComponentConf* conf = g_managedCallbackApp->GetComponentConf(_componentName);
            if (conf == nullptr || conf->Has == nullptr || conf->Get == nullptr || !conf->Has(*entity))
                return false;

            void* componentPtr = conf->Get(*entity);
            if (componentPtr == nullptr)
                return false;

            _outAccess.entity = entity;
            _outAccess.conf = conf;
            _outAccess.componentPtr = componentPtr;
            return true;
        }

        bool TryGetManagedComponentPropertyNode(const ManagedComponentAccess& _access, const char* _propertyName, YAML::Node& _outNode)
        {
            if (_propertyName == nullptr || _propertyName[0] == '\0' || _access.conf == nullptr)
                return false;

            auto getterIt = _access.conf->registry.getters.find(_propertyName);
            if (getterIt == _access.conf->registry.getters.end())
                return false;

            _outNode = getterIt->second(_access.componentPtr);
            return _outNode.IsDefined();
        }

        bool TrySetManagedComponentPropertyNode(const ManagedComponentAccess& _access, const char* _propertyName, YAML::Node _node)
        {
            if (_propertyName == nullptr || _propertyName[0] == '\0' || _access.conf == nullptr)
                return false;

            auto setterIt = _access.conf->registry.setters.find(_propertyName);
            if (setterIt == _access.conf->registry.setters.end())
                return false;

            setterIt->second(_node, _access.componentPtr);
            return true;
        }

        int CopyManagedStringResult(const std::string& _value, char* _buffer, int _bufferSize)
        {
            const int required = static_cast<int>(_value.size());
            if (_buffer == nullptr || _bufferSize <= 0)
                return required;

            const int copyCount = std::min(required, _bufferSize - 1);
            if (copyCount > 0)
                std::memcpy(_buffer, _value.data(), static_cast<size_t>(copyCount));

            _buffer[copyCount] = '\0';
            return copyCount;
        }

        void ManagedCallbackLogInfo(const char* _message)
        {
            if (_message == nullptr)
                return;

            Debug::Log("[Managed] %s", _message);
        }

        int ManagedCallbackHasTransform(uint64_t _entityUuid)
        {
            if (Entity* entity = ResolveManagedCallbackEntity(_entityUuid))
                return entity->HasComponent<Transform>() ? 1 : 0;

            return 0;
        }

        int ManagedCallbackGetTransformPosition(uint64_t _entityUuid, float* _x, float* _y, float* _z)
        {
            if (_x == nullptr || _y == nullptr || _z == nullptr)
                return 0;

            Entity* entity = ResolveManagedCallbackEntity(_entityUuid);
            if (entity == nullptr || !entity->HasComponent<Transform>())
                return 0;

            const Vector3 value = entity->GetComponent<Transform>().position;
            *_x = value.x;
            *_y = value.y;
            *_z = value.z;
            return 1;
        }

        int ManagedCallbackGetTransformRotation(uint64_t _entityUuid, float* _x, float* _y, float* _z)
        {
            if (_x == nullptr || _y == nullptr || _z == nullptr)
                return 0;

            Entity* entity = ResolveManagedCallbackEntity(_entityUuid);
            if (entity == nullptr || !entity->HasComponent<Transform>())
                return 0;

            const Vector3 value = entity->GetComponent<Transform>().rotation;
            *_x = value.x;
            *_y = value.y;
            *_z = value.z;
            return 1;
        }

        int ManagedCallbackGetTransformScale(uint64_t _entityUuid, float* _x, float* _y, float* _z)
        {
            if (_x == nullptr || _y == nullptr || _z == nullptr)
                return 0;

            Entity* entity = ResolveManagedCallbackEntity(_entityUuid);
            if (entity == nullptr || !entity->HasComponent<Transform>())
                return 0;

            const Vector3 value = entity->GetComponent<Transform>().scale;
            *_x = value.x;
            *_y = value.y;
            *_z = value.z;
            return 1;
        }

        int ManagedCallbackSetTransformPosition(uint64_t _entityUuid, float _x, float _y, float _z)
        {
            Entity* entity = ResolveManagedCallbackEntity(_entityUuid);
            if (entity == nullptr || !entity->HasComponent<Transform>())
                return 0;

            entity->GetComponent<Transform>().position = Vector3(_x, _y, _z);
            return 1;
        }

        int ManagedCallbackSetTransformRotation(uint64_t _entityUuid, float _x, float _y, float _z)
        {
            Entity* entity = ResolveManagedCallbackEntity(_entityUuid);
            if (entity == nullptr || !entity->HasComponent<Transform>())
                return 0;

            entity->GetComponent<Transform>().rotation = Vector3(_x, _y, _z);
            return 1;
        }

        int ManagedCallbackSetTransformScale(uint64_t _entityUuid, float _x, float _y, float _z)
        {
            Entity* entity = ResolveManagedCallbackEntity(_entityUuid);
            if (entity == nullptr || !entity->HasComponent<Transform>())
                return 0;

            entity->GetComponent<Transform>().scale = Vector3(_x, _y, _z);
            return 1;
        }

        int ManagedCallbackHasComponent(uint64_t _entityUuid, const char* _componentName)
        {
            ManagedComponentAccess access = {};
            return ResolveManagedComponentAccess(_entityUuid, _componentName, access) ? 1 : 0;
        }

        int ManagedCallbackGetComponentBool(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, int* _value)
        {
            if (_value == nullptr)
                return 0;

            ManagedComponentAccess access = {};
            YAML::Node node = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access) ||
                !TryGetManagedComponentPropertyNode(access, _propertyName, node))
                return 0;

            try
            {
                *_value = node.as<bool>(false) ? 1 : 0;
                return 1;
            }
            catch (const YAML::Exception&)
            {
                return 0;
            }
        }

        int ManagedCallbackSetComponentBool(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, int _value)
        {
            ManagedComponentAccess access = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access))
                return 0;

            YAML::Node node(_value != 0);
            return TrySetManagedComponentPropertyNode(access, _propertyName, node) ? 1 : 0;
        }

        int ManagedCallbackGetComponentInt(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, int* _value)
        {
            if (_value == nullptr)
                return 0;

            ManagedComponentAccess access = {};
            YAML::Node node = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access) ||
                !TryGetManagedComponentPropertyNode(access, _propertyName, node))
                return 0;

            try
            {
                *_value = node.as<int>(0);
                return 1;
            }
            catch (const YAML::Exception&)
            {
                return 0;
            }
        }

        int ManagedCallbackSetComponentInt(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, int _value)
        {
            ManagedComponentAccess access = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access))
                return 0;

            YAML::Node node(_value);
            return TrySetManagedComponentPropertyNode(access, _propertyName, node) ? 1 : 0;
        }

        int ManagedCallbackGetComponentFloat(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, float* _value)
        {
            if (_value == nullptr)
                return 0;

            ManagedComponentAccess access = {};
            YAML::Node node = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access) ||
                !TryGetManagedComponentPropertyNode(access, _propertyName, node))
                return 0;

            try
            {
                *_value = node.as<float>(0.0f);
                return 1;
            }
            catch (const YAML::Exception&)
            {
                return 0;
            }
        }

        int ManagedCallbackSetComponentFloat(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, float _value)
        {
            ManagedComponentAccess access = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access))
                return 0;

            YAML::Node node(_value);
            return TrySetManagedComponentPropertyNode(access, _propertyName, node) ? 1 : 0;
        }

        int ManagedCallbackGetComponentString(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, char* _buffer, int _bufferSize)
        {
            ManagedComponentAccess access = {};
            YAML::Node node = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access) ||
                !TryGetManagedComponentPropertyNode(access, _propertyName, node))
                return -1;

            try
            {
                return CopyManagedStringResult(node.as<std::string>(""), _buffer, _bufferSize);
            }
            catch (const YAML::Exception&)
            {
                return -1;
            }
        }

        int ManagedCallbackSetComponentString(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, const char* _value)
        {
            ManagedComponentAccess access = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access))
                return 0;

            YAML::Node node(_value != nullptr ? std::string(_value) : std::string());
            return TrySetManagedComponentPropertyNode(access, _propertyName, node) ? 1 : 0;
        }

        int ManagedCallbackGetComponentVec2(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, float* _x, float* _y)
        {
            if (_x == nullptr || _y == nullptr)
                return 0;

            ManagedComponentAccess access = {};
            YAML::Node node = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access) ||
                !TryGetManagedComponentPropertyNode(access, _propertyName, node))
                return 0;

            try
            {
                const Vector2 value = node.as<Vector2>(Vector2(0.0f));
                *_x = value.x;
                *_y = value.y;
                return 1;
            }
            catch (const YAML::Exception&)
            {
                return 0;
            }
        }

        int ManagedCallbackSetComponentVec2(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, float _x, float _y)
        {
            ManagedComponentAccess access = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access))
                return 0;

            YAML::Node node(Vector2(_x, _y));
            return TrySetManagedComponentPropertyNode(access, _propertyName, node) ? 1 : 0;
        }

        int ManagedCallbackGetComponentVec3(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, float* _x, float* _y, float* _z)
        {
            if (_x == nullptr || _y == nullptr || _z == nullptr)
                return 0;

            ManagedComponentAccess access = {};
            YAML::Node node = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access) ||
                !TryGetManagedComponentPropertyNode(access, _propertyName, node))
                return 0;

            try
            {
                const Vector3 value = node.as<Vector3>(Vector3(0.0f));
                *_x = value.x;
                *_y = value.y;
                *_z = value.z;
                return 1;
            }
            catch (const YAML::Exception&)
            {
                return 0;
            }
        }

        int ManagedCallbackSetComponentVec3(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, float _x, float _y, float _z)
        {
            ManagedComponentAccess access = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access))
                return 0;

            YAML::Node node(Vector3(_x, _y, _z));
            return TrySetManagedComponentPropertyNode(access, _propertyName, node) ? 1 : 0;
        }

        int ManagedCallbackGetComponentVec4(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, float* _x, float* _y, float* _z, float* _w)
        {
            if (_x == nullptr || _y == nullptr || _z == nullptr || _w == nullptr)
                return 0;

            ManagedComponentAccess access = {};
            YAML::Node node = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access) ||
                !TryGetManagedComponentPropertyNode(access, _propertyName, node))
                return 0;

            try
            {
                const Vector4 value = node.as<Vector4>(Vector4(0.0f));
                *_x = value.x;
                *_y = value.y;
                *_z = value.z;
                *_w = value.w;
                return 1;
            }
            catch (const YAML::Exception&)
            {
                return 0;
            }
        }

        int ManagedCallbackSetComponentVec4(uint64_t _entityUuid, const char* _componentName, const char* _propertyName, float _x, float _y, float _z, float _w)
        {
            ManagedComponentAccess access = {};
            if (!ResolveManagedComponentAccess(_entityUuid, _componentName, access))
                return 0;

            YAML::Node node(Vector4(_x, _y, _z, _w));
            return TrySetManagedComponentPropertyNode(access, _propertyName, node) ? 1 : 0;
        }

        ManagedNativeCallbacks BuildManagedNativeCallbacks()
        {
            ManagedNativeCallbacks callbacks = {};
            callbacks.logInfo = &ManagedCallbackLogInfo;
            callbacks.hasTransform = &ManagedCallbackHasTransform;
            callbacks.getTransformPosition = &ManagedCallbackGetTransformPosition;
            callbacks.getTransformRotation = &ManagedCallbackGetTransformRotation;
            callbacks.getTransformScale = &ManagedCallbackGetTransformScale;
            callbacks.setTransformPosition = &ManagedCallbackSetTransformPosition;
            callbacks.setTransformRotation = &ManagedCallbackSetTransformRotation;
            callbacks.setTransformScale = &ManagedCallbackSetTransformScale;
            callbacks.hasComponent = &ManagedCallbackHasComponent;
            callbacks.getComponentBool = &ManagedCallbackGetComponentBool;
            callbacks.setComponentBool = &ManagedCallbackSetComponentBool;
            callbacks.getComponentInt = &ManagedCallbackGetComponentInt;
            callbacks.setComponentInt = &ManagedCallbackSetComponentInt;
            callbacks.getComponentFloat = &ManagedCallbackGetComponentFloat;
            callbacks.setComponentFloat = &ManagedCallbackSetComponentFloat;
            callbacks.getComponentString = &ManagedCallbackGetComponentString;
            callbacks.setComponentString = &ManagedCallbackSetComponentString;
            callbacks.getComponentVec2 = &ManagedCallbackGetComponentVec2;
            callbacks.setComponentVec2 = &ManagedCallbackSetComponentVec2;
            callbacks.getComponentVec3 = &ManagedCallbackGetComponentVec3;
            callbacks.setComponentVec3 = &ManagedCallbackSetComponentVec3;
            callbacks.getComponentVec4 = &ManagedCallbackGetComponentVec4;
            callbacks.setComponentVec4 = &ManagedCallbackSetComponentVec4;
            return callbacks;
        }

        void DrawManagedPropertyField(Editor& _editor, ManagedScriptComponent& _component, const ManagedPropertyDefinition& _definition, const std::string& _idSuffix)
        {
            ManagedPropertyValue* value = _component.GetPropertyValue(_definition.name);
            if (value == nullptr)
                return;

            const ManagedPropertyValue beforeValue = *value;

            switch (_definition.type)
            {
                case ManagedPropertyType::Bool:
                {
                    bool typedValue = false;
                    (void)TryGetManagedValue(*value, typedValue);
                    DrawInspectorField<bool>(_editor, _definition.name.c_str(), _idSuffix.c_str(), typedValue);
                    *value = typedValue;
                    break;
                }
                case ManagedPropertyType::Int:
                {
                    int typedValue = 0;
                    (void)TryGetManagedValue(*value, typedValue);
                    DrawInspectorField<int>(_editor, _definition.name.c_str(), _idSuffix.c_str(), typedValue);
                    *value = typedValue;
                    break;
                }
                case ManagedPropertyType::Float:
                {
                    float typedValue = 0.0f;
                    (void)TryGetManagedValue(*value, typedValue);
                    DrawInspectorField<float>(_editor, _definition.name.c_str(), _idSuffix.c_str(), typedValue);
                    *value = typedValue;
                    break;
                }
                case ManagedPropertyType::String:
                {
                    std::string typedValue = "";
                    (void)TryGetManagedValue(*value, typedValue);
                    DrawInspectorField<std::string>(_editor, _definition.name.c_str(), _idSuffix.c_str(), typedValue);
                    *value = typedValue;
                    break;
                }
                case ManagedPropertyType::Vec2:
                {
                    Vector2 typedValue = Vector2(0.0f);
                    (void)TryGetManagedValue(*value, typedValue);
                    DrawInspectorField<Vector2>(_editor, _definition.name.c_str(), _idSuffix.c_str(), typedValue);
                    *value = typedValue;
                    break;
                }
                case ManagedPropertyType::Vec3:
                {
                    Vector3 typedValue = Vector3(0.0f);
                    (void)TryGetManagedValue(*value, typedValue);
                    DrawInspectorField<Vector3>(_editor, _definition.name.c_str(), _idSuffix.c_str(), typedValue);
                    *value = typedValue;
                    break;
                }
                case ManagedPropertyType::Vec4:
                {
                    Vector4 typedValue = Vector4(0.0f);
                    (void)TryGetManagedValue(*value, typedValue);
                    DrawInspectorField<Vector4>(_editor, _definition.name.c_str(), _idSuffix.c_str(), typedValue);
                    *value = typedValue;
                    break;
                }
                case ManagedPropertyType::Color:
                {
                    const Vector4 vectorValue = ManagedColorVectorFromValue(*value);
                    Color typedValue = Color(vectorValue.x, vectorValue.y, vectorValue.z, vectorValue.w);
                    DrawInspectorField<Color>(_editor, _definition.name.c_str(), _idSuffix.c_str(), typedValue);
                    *value = Vector4(typedValue.r, typedValue.g, typedValue.b, typedValue.a);
                    break;
                }
                case ManagedPropertyType::None:
                default:
                    ImGui::TextDisabled("%s (unsupported managed property)", _definition.name.c_str());
                    break;
            }

            if (ManagedPropertyValueSupportsAnimation(_definition.type) &&
                ManagedPropertyValuesDiffer(beforeValue, *value, _definition.type))
            {
                _editor.NotifyAnimationPropertyEdited(
                    _component.entity,
                    _idSuffix,
                    _definition.name,
                    ManagedPropertyAnimationType(_definition.type),
                    ManagedPropertyAnimationInterpolation(_definition.type),
                    ManagedPropertyToAnimationValue(*value, _definition.type));
            }
        }

        void RegisterManagedProperty(ScriptConf& _conf, const ManagedPropertyDefinition& _definition)
        {
            const std::string propertyName = _definition.name;
            const ManagedPropertyType propertyType = _definition.type;

            _conf.registry.setters[propertyName] =
                [propertyName](YAML::Node& _node, void* _componentPtr)
                {
                    auto* component = static_cast<ManagedScriptComponent*>(_componentPtr);
                    (void)component->SetPropertyFromYaml(propertyName, _node);
                };

            _conf.registry.getters[propertyName] =
                [propertyName](void* _componentPtr) -> YAML::Node
                {
                    auto* component = static_cast<ManagedScriptComponent*>(_componentPtr);
                    return component->GetPropertyAsYaml(propertyName);
                };

            _conf.registry.drawers[propertyName] =
                [definition = _definition](Editor& _editor, const std::string&, void* _componentPtr, const std::string& _idSuffix)
                {
                    auto* component = static_cast<ManagedScriptComponent*>(_componentPtr);
                    DrawManagedPropertyField(_editor, *component, definition, _idSuffix);
                };

            if (ManagedPropertyValueSupportsAnimation(propertyType))
            {
                _conf.registry.animationTypes[propertyName] = ManagedPropertyAnimationType(propertyType);
                _conf.registry.animationInterpolations[propertyName] = ManagedPropertyAnimationInterpolation(propertyType);
                _conf.registry.animationGetters[propertyName] =
                    [propertyName, propertyType](void* _componentPtr) -> AnimationValue
                    {
                        auto* component = static_cast<ManagedScriptComponent*>(_componentPtr);
                        if (const ManagedPropertyValue* value = component->GetPropertyValue(propertyName))
                            return ManagedPropertyToAnimationValue(*value, propertyType);

                        return {};
                    };
                _conf.registry.animationSetters[propertyName] =
                    [propertyName, propertyType](void* _componentPtr, const AnimationValue& _value)
                    {
                        auto* component = static_cast<ManagedScriptComponent*>(_componentPtr);
                        ManagedPropertyValue updatedValue = DefaultManagedValueForType(propertyType);
                        if (const ManagedPropertyValue* currentValue = component->GetPropertyValue(propertyName))
                            updatedValue = *currentValue;

                        if (ManagedPropertyFromAnimationValue(_value, propertyType, updatedValue))
                            (void)component->SetPropertyValue(propertyName, updatedValue);
                    };
            }

            _conf.registry.propertyOrder.push_back(propertyName);
        }

        bool HasManagedManifestExtension(const fs::path& _path)
        {
            const std::string filename = ToLowerCopy(_path.filename().string());
            return filename.size() >= 12 &&
                (filename.ends_with(".managed.yml") || filename.ends_with(".managed.yaml"));
        }

        std::vector<fs::path> FindManagedManifestFiles()
        {
            std::vector<fs::path> manifests = {};
            std::vector<fs::path> managedRoots =
            {
                fs::path(kManagedScriptsRoot),
                fs::path(kLegacyManagedScriptsRoot)
            };

            std::error_code ec;
            for (const fs::path& managedRoot : managedRoots)
            {
                ec.clear();
                if (!fs::exists(managedRoot, ec) || !fs::is_directory(managedRoot, ec))
                    continue;

                for (const fs::directory_entry& entry : fs::recursive_directory_iterator(managedRoot, ec))
                {
                    if (ec)
                        break;

                    if (!entry.is_regular_file())
                        continue;

                    if (HasManagedManifestExtension(entry.path()))
                        manifests.push_back(entry.path());
                }
            }

            std::sort(manifests.begin(), manifests.end());
            manifests.erase(std::unique(manifests.begin(), manifests.end()), manifests.end());
            return manifests;
        }

        std::shared_ptr<const ManagedScriptDefinition> ParseManagedScriptDefinition(const YAML::Node& _scriptNode, const std::string& _defaultAssemblyPath, const fs::path& _manifestPath)
        {
            if (!_scriptNode || !_scriptNode.IsMap())
                return {};

            std::shared_ptr<ManagedScriptDefinition> definition = std::make_shared<ManagedScriptDefinition>();
            definition->scriptName = _scriptNode["ScriptName"].as<std::string>(_scriptNode["scriptName"].as<std::string>(""));
            definition->typeName = _scriptNode["TypeName"].as<std::string>(_scriptNode["typeName"].as<std::string>(definition->scriptName));
            definition->assemblyPath = _scriptNode["AssemblyPath"].as<std::string>(_scriptNode["assemblyPath"].as<std::string>(_defaultAssemblyPath));
            definition->sourcePath = _scriptNode["SourcePath"].as<std::string>(_scriptNode["sourcePath"].as<std::string>(""));

            if (definition->scriptName.empty())
                definition->scriptName = definition->typeName;

            if (definition->typeName.empty())
                return {};

            if (!definition->assemblyPath.empty())
            {
                fs::path assemblyPath(definition->assemblyPath);
                if (assemblyPath.is_relative())
                    definition->assemblyPath = ResolveManagedRuntimePath(assemblyPath.generic_string());
            }

            if (!definition->sourcePath.empty())
            {
                fs::path sourcePath(definition->sourcePath);
                if (sourcePath.is_relative())
                    definition->sourcePath = (fs::current_path() / sourcePath).lexically_normal().generic_string();
            }

            YAML::Node propertiesNode = _scriptNode["Properties"];
            if (!propertiesNode)
                propertiesNode = _scriptNode["properties"];

            if (propertiesNode && propertiesNode.IsSequence())
            {
                for (const YAML::Node& propertyNode : propertiesNode)
                {
                    if (!propertyNode || !propertyNode.IsMap())
                        continue;

                    ManagedPropertyDefinition property = {};
                    property.name = propertyNode["Name"].as<std::string>(propertyNode["name"].as<std::string>(""));
                    property.type = ManagedPropertyTypeFromString(propertyNode["Type"].as<std::string>(propertyNode["type"].as<std::string>("")));
                    property.defaultValue = DefaultManagedValueForType(property.type);

                    YAML::Node defaultNode = propertyNode["Default"];
                    if (!defaultNode)
                        defaultNode = propertyNode["default"];

                    if (property.name.empty() || property.type == ManagedPropertyType::None)
                        continue;

                    ManagedPropertyValue parsedDefault = property.defaultValue;
                    if (defaultNode && ManagedPropertyValueFromYaml(property.type, defaultNode, parsedDefault))
                        property.defaultValue = parsedDefault;

                    definition->properties.push_back(property);
                }
            }

            return definition;
        }

        std::vector<std::shared_ptr<const ManagedScriptDefinition>> ParseManagedManifestFile(const fs::path& _manifestPath)
        {
            std::vector<std::shared_ptr<const ManagedScriptDefinition>> definitions = {};

            try
            {
                YAML::Node root = YAML::LoadFile(_manifestPath.string());
                if (!root || !root.IsMap())
                    return definitions;

                std::string assemblyPath = root["AssemblyPath"].as<std::string>(root["assemblyPath"].as<std::string>(""));
                if (!assemblyPath.empty())
                    assemblyPath = ResolveManagedRuntimePath(assemblyPath);

                YAML::Node scriptsNode = root["Scripts"];
                if (!scriptsNode)
                    scriptsNode = root["scripts"];

                if (!scriptsNode || !scriptsNode.IsSequence())
                    return definitions;

                for (const YAML::Node& scriptNode : scriptsNode)
                {
                    if (std::shared_ptr<const ManagedScriptDefinition> definition = ParseManagedScriptDefinition(scriptNode, assemblyPath, _manifestPath))
                        definitions.push_back(definition);
                }
            }
            catch (const YAML::Exception& exception)
            {
                Debug::Warning("Failed to parse managed script manifest '%s': %s", _manifestPath.string().c_str(), exception.what());
            }

            return definitions;
        }

#if defined(_WIN32)
        using HostChar = wchar_t;
        using HostLibraryHandle = HMODULE;
        constexpr const wchar_t* kHostFxrLibraryName = L"hostfxr.dll";

        const wchar_t* GetUnmanagedCallersOnlyMethodMarker()
        {
            return reinterpret_cast<const wchar_t*>(static_cast<intptr_t>(-1));
        }

        std::wstring ToHostString(const std::string& _value)
        {
            if (_value.empty())
                return {};

            const int required = MultiByteToWideChar(CP_UTF8, 0, _value.c_str(), -1, nullptr, 0);
            std::wstring wideValue(static_cast<size_t>(std::max(required, 1)) - 1, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, _value.c_str(), -1, wideValue.data(), required);
            return wideValue;
        }

        std::string ToUtf8String(const std::wstring& _value)
        {
            if (_value.empty())
                return {};

            const int required = WideCharToMultiByte(CP_UTF8, 0, _value.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string utf8(static_cast<size_t>(std::max(required, 1)) - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, _value.c_str(), -1, utf8.data(), required, nullptr, nullptr);
            return utf8;
        }

        HostLibraryHandle OpenHostLibrary(const std::wstring& _path)
        {
            return LoadLibraryW(_path.c_str());
        }

        void* LoadHostSymbol(HostLibraryHandle _library, const char* _symbol)
        {
            return reinterpret_cast<void*>(GetProcAddress(_library, _symbol));
        }

        void CloseHostLibrary(HostLibraryHandle _library)
        {
            if (_library != nullptr)
                FreeLibrary(_library);
        }

        std::vector<std::wstring> GetHostFxrRootCandidates()
        {
            std::vector<std::wstring> candidates = {};

            if (const char* dotnetRoot = std::getenv("DOTNET_ROOT"))
                candidates.push_back(ToHostString(dotnetRoot));
            if (const char* dotnetRootX64 = std::getenv("DOTNET_ROOT_X64"))
                candidates.push_back(ToHostString(dotnetRootX64));

            wchar_t programFilesPath[MAX_PATH] = {};
            if (GetEnvironmentVariableW(L"ProgramFiles", programFilesPath, MAX_PATH) > 0)
                candidates.push_back(std::wstring(programFilesPath) + L"\\dotnet");

            return candidates;
        }
#elif defined(__EMSCRIPTEN__)
        using HostChar = char;
        using HostLibraryHandle = void*;
        constexpr const char* kHostFxrLibraryName = "";

        std::string ToHostString(const std::string& _value)
        {
            return _value;
        }

        std::string ToUtf8String(const std::string& _value)
        {
            return _value;
        }

        const char* GetUnmanagedCallersOnlyMethodMarker()
        {
            return reinterpret_cast<const char*>(static_cast<intptr_t>(-1));
        }

        std::vector<std::string> GetHostFxrRootCandidates()
        {
            return {};
        }

        extern "C"
        {
            __attribute__((weak)) int CanisManagedWebInitializeHost(const ManagedNativeCallbacks*, const char*);
            __attribute__((weak)) void CanisManagedWebShutdownHost();
            __attribute__((weak)) int CanisManagedWebInstantiateScript(uint64_t, const char*, const char*, const char*);
            __attribute__((weak)) void CanisManagedWebInvokeCreate(uint64_t, const char*);
            __attribute__((weak)) void CanisManagedWebInvokeReady(uint64_t, const char*);
            __attribute__((weak)) void CanisManagedWebInvokeDestroy(uint64_t, const char*);
            __attribute__((weak)) void CanisManagedWebUpdateScript(uint64_t, const char*, float);
            __attribute__((weak)) void CanisManagedWebReleaseScript(uint64_t, const char*);
            __attribute__((weak)) void CanisManagedWebSetBool(uint64_t, const char*, const char*, int);
            __attribute__((weak)) int CanisManagedWebGetBool(uint64_t, const char*, const char*);
            __attribute__((weak)) void CanisManagedWebSetInt(uint64_t, const char*, const char*, int);
            __attribute__((weak)) int CanisManagedWebGetInt(uint64_t, const char*, const char*);
            __attribute__((weak)) void CanisManagedWebSetFloat(uint64_t, const char*, const char*, float);
            __attribute__((weak)) float CanisManagedWebGetFloat(uint64_t, const char*, const char*);
            __attribute__((weak)) void CanisManagedWebSetString(uint64_t, const char*, const char*, const char*);
            __attribute__((weak)) int CanisManagedWebGetString(uint64_t, const char*, const char*, char*, int);
            __attribute__((weak)) void CanisManagedWebSetVec2(uint64_t, const char*, const char*, float, float);
            __attribute__((weak)) void CanisManagedWebGetVec2(uint64_t, const char*, const char*, float*, float*);
            __attribute__((weak)) void CanisManagedWebSetVec3(uint64_t, const char*, const char*, float, float, float);
            __attribute__((weak)) void CanisManagedWebGetVec3(uint64_t, const char*, const char*, float*, float*, float*);
            __attribute__((weak)) void CanisManagedWebSetVec4(uint64_t, const char*, const char*, float, float, float, float);
            __attribute__((weak)) void CanisManagedWebGetVec4(uint64_t, const char*, const char*, float*, float*, float*, float*);
        }
#else
        using HostChar = char;
        using HostLibraryHandle = void*;
        constexpr const char* kHostFxrLibraryName =
#if defined(__APPLE__)
            "libhostfxr.dylib";
#else
            "libhostfxr.so";
#endif

        const char* GetUnmanagedCallersOnlyMethodMarker()
        {
            return reinterpret_cast<const char*>(static_cast<intptr_t>(-1));
        }

        std::string ToHostString(const std::string& _value)
        {
            return _value;
        }

        std::string ToUtf8String(const std::string& _value)
        {
            return _value;
        }

        HostLibraryHandle OpenHostLibrary(const std::string& _path)
        {
            return dlopen(_path.c_str(), RTLD_NOW | RTLD_LOCAL);
        }

        void* LoadHostSymbol(HostLibraryHandle _library, const char* _symbol)
        {
            return dlsym(_library, _symbol);
        }

        void CloseHostLibrary(HostLibraryHandle _library)
        {
            if (_library != nullptr)
                dlclose(_library);
        }

        std::vector<std::string> GetHostFxrRootCandidates()
        {
            std::vector<std::string> candidates = {};

            if (const char* dotnetRoot = std::getenv("DOTNET_ROOT"))
                candidates.push_back(dotnetRoot);

            if (const char* home = std::getenv("HOME"))
                candidates.push_back(std::string(home) + "/.dotnet");

            candidates.push_back("/usr/share/dotnet");
            candidates.push_back("/usr/local/share/dotnet");
            return candidates;
        }
#endif

        template <typename PathString>
        std::optional<PathString> FindLatestHostFxrPath()
        {
            using PathType = fs::path;
            std::vector<PathType> candidateFiles = {};

            for (const auto& rootCandidate : GetHostFxrRootCandidates())
            {
                if (rootCandidate.empty())
                    continue;

                const PathType fxrRoot = PathType(rootCandidate) / "host" / "fxr";
                std::error_code ec;
                if (!fs::exists(fxrRoot, ec) || !fs::is_directory(fxrRoot, ec))
                    continue;

                for (const fs::directory_entry& entry : fs::directory_iterator(fxrRoot, ec))
                {
                    if (ec || !entry.is_directory())
                        continue;

                    const PathType libraryPath = entry.path() / kHostFxrLibraryName;
                    if (fs::exists(libraryPath, ec))
                        candidateFiles.push_back(libraryPath);
                }
            }

            if (candidateFiles.empty())
                return std::nullopt;

            std::sort(candidateFiles.begin(), candidateFiles.end());
            return ToHostString(candidateFiles.back().lexically_normal().generic_string());
        }

        struct ManagedHostFunctionTable
        {
            using hostfxr_handle = void*;
            using hostfxr_initialize_for_runtime_config_fn = int (*)(const HostChar*, const void*, hostfxr_handle*);
            using hostfxr_get_runtime_delegate_fn = int (*)(hostfxr_handle, int, void**);
            using hostfxr_close_fn = int (*)(hostfxr_handle);
            using load_assembly_and_get_function_pointer_fn =
                int (*)(const HostChar*, const HostChar*, const HostChar*, const HostChar*, void*, void**);

            using InitializeManagedHostFn = int (*)(const ManagedNativeCallbacks*, const char*);
            using ShutdownManagedHostFn = void (*)();
            using InstantiateScriptFn = int (*)(uint64_t, const char*, const char*, const char*);
            using InvokeScriptFn = void (*)(uint64_t, const char*);
            using UpdateScriptFn = void (*)(uint64_t, const char*, float);
            using ReleaseScriptFn = void (*)(uint64_t, const char*);
            using SetBoolFn = void (*)(uint64_t, const char*, const char*, int);
            using GetBoolFn = int (*)(uint64_t, const char*, const char*);
            using SetIntFn = void (*)(uint64_t, const char*, const char*, int);
            using GetIntFn = int (*)(uint64_t, const char*, const char*);
            using SetFloatFn = void (*)(uint64_t, const char*, const char*, float);
            using GetFloatFn = float (*)(uint64_t, const char*, const char*);
            using SetStringFn = void (*)(uint64_t, const char*, const char*, const char*);
            using GetStringFn = int (*)(uint64_t, const char*, const char*, char*, int);
            using SetVec2Fn = void (*)(uint64_t, const char*, const char*, float, float);
            using GetVec2Fn = void (*)(uint64_t, const char*, const char*, float*, float*);
            using SetVec3Fn = void (*)(uint64_t, const char*, const char*, float, float, float);
            using GetVec3Fn = void (*)(uint64_t, const char*, const char*, float*, float*, float*);
            using SetVec4Fn = void (*)(uint64_t, const char*, const char*, float, float, float, float);
            using GetVec4Fn = void (*)(uint64_t, const char*, const char*, float*, float*, float*, float*);

            HostLibraryHandle library = nullptr;
            load_assembly_and_get_function_pointer_fn loadAssemblyAndGetFunctionPointer = nullptr;

            InitializeManagedHostFn initializeManagedHost = nullptr;
            ShutdownManagedHostFn shutdownManagedHost = nullptr;
            InstantiateScriptFn instantiateScript = nullptr;
            InvokeScriptFn invokeCreate = nullptr;
            InvokeScriptFn invokeReady = nullptr;
            InvokeScriptFn invokeDestroy = nullptr;
            UpdateScriptFn updateScript = nullptr;
            ReleaseScriptFn releaseScript = nullptr;
            SetBoolFn setBool = nullptr;
            GetBoolFn getBool = nullptr;
            SetIntFn setInt = nullptr;
            GetIntFn getInt = nullptr;
            SetFloatFn setFloat = nullptr;
            GetFloatFn getFloat = nullptr;
            SetStringFn setString = nullptr;
            GetStringFn getString = nullptr;
            SetVec2Fn setVec2 = nullptr;
            GetVec2Fn getVec2 = nullptr;
            SetVec3Fn setVec3 = nullptr;
            GetVec3Fn getVec3 = nullptr;
            SetVec4Fn setVec4 = nullptr;
            GetVec4Fn getVec4 = nullptr;
        };

        bool BuildManagedHostFunctionTable(const std::string& _runtimeConfigPath, const std::string& _assemblyPath, ManagedHostFunctionTable& _outTable);

        ManagedHostFunctionTable* AcquireProcessManagedHostFunctionTable(
            const std::string& _runtimeConfigPath,
            const std::string& _assemblyPath)
        {
            static ManagedHostFunctionTable table = {};
            static bool loaded = false;

            if (loaded)
                return &table;

            ManagedHostFunctionTable loadedTable = {};
            if (!BuildManagedHostFunctionTable(_runtimeConfigPath, _assemblyPath, loadedTable))
                return nullptr;

            table = loadedTable;
            loaded = true;
            return &table;
        }

        bool LoadManagedHostEntrypoint(
            ManagedHostFunctionTable& _table,
            const std::string& _assemblyPath,
            const std::string& _typeName,
            const std::string& _methodName,
            void** _outFunction)
        {
            if (_table.loadAssemblyAndGetFunctionPointer == nullptr || _outFunction == nullptr)
                return false;

            const auto assemblyPath = ToHostString(_assemblyPath);
            const auto typeName = ToHostString(_typeName);
            const auto methodName = ToHostString(_methodName);

            void* functionPtr = nullptr;
            const int rc = _table.loadAssemblyAndGetFunctionPointer(
                assemblyPath.c_str(),
                typeName.c_str(),
                methodName.c_str(),
                GetUnmanagedCallersOnlyMethodMarker(),
                nullptr,
                &functionPtr);
            if (rc != 0 || functionPtr == nullptr)
                return false;

            *_outFunction = functionPtr;
            return true;
        }

        bool BuildManagedHostFunctionTable(const std::string& _runtimeConfigPath, const std::string& _assemblyPath, ManagedHostFunctionTable& _outTable)
        {
#if defined(__EMSCRIPTEN__)
            (void)_runtimeConfigPath;
            (void)_assemblyPath;

            ManagedHostFunctionTable table = {};
            table.initializeManagedHost = CanisManagedWebInitializeHost;
            table.shutdownManagedHost = CanisManagedWebShutdownHost;
            table.instantiateScript = CanisManagedWebInstantiateScript;
            table.invokeCreate = CanisManagedWebInvokeCreate;
            table.invokeReady = CanisManagedWebInvokeReady;
            table.invokeDestroy = CanisManagedWebInvokeDestroy;
            table.updateScript = CanisManagedWebUpdateScript;
            table.releaseScript = CanisManagedWebReleaseScript;
            table.setBool = CanisManagedWebSetBool;
            table.getBool = CanisManagedWebGetBool;
            table.setInt = CanisManagedWebSetInt;
            table.getInt = CanisManagedWebGetInt;
            table.setFloat = CanisManagedWebSetFloat;
            table.getFloat = CanisManagedWebGetFloat;
            table.setString = CanisManagedWebSetString;
            table.getString = CanisManagedWebGetString;
            table.setVec2 = CanisManagedWebSetVec2;
            table.getVec2 = CanisManagedWebGetVec2;
            table.setVec3 = CanisManagedWebSetVec3;
            table.getVec3 = CanisManagedWebGetVec3;
            table.setVec4 = CanisManagedWebSetVec4;
            table.getVec4 = CanisManagedWebGetVec4;

            const bool isComplete =
                table.initializeManagedHost != nullptr &&
                table.shutdownManagedHost != nullptr &&
                table.instantiateScript != nullptr &&
                table.invokeCreate != nullptr &&
                table.invokeReady != nullptr &&
                table.invokeDestroy != nullptr &&
                table.updateScript != nullptr &&
                table.releaseScript != nullptr &&
                table.setBool != nullptr &&
                table.getBool != nullptr &&
                table.setInt != nullptr &&
                table.getInt != nullptr &&
                table.setFloat != nullptr &&
                table.getFloat != nullptr &&
                table.setString != nullptr &&
                table.getString != nullptr &&
                table.setVec2 != nullptr &&
                table.getVec2 != nullptr &&
                table.setVec3 != nullptr &&
                table.getVec3 != nullptr &&
                table.setVec4 != nullptr &&
                table.getVec4 != nullptr;
            if (!isComplete)
                return false;

            _outTable = table;
            return true;
#else
            using hostfxr_handle = ManagedHostFunctionTable::hostfxr_handle;
            using hostfxr_initialize_for_runtime_config_fn = ManagedHostFunctionTable::hostfxr_initialize_for_runtime_config_fn;
            using hostfxr_get_runtime_delegate_fn = ManagedHostFunctionTable::hostfxr_get_runtime_delegate_fn;
            using hostfxr_close_fn = ManagedHostFunctionTable::hostfxr_close_fn;

            constexpr int hdt_load_assembly_and_get_function_pointer = 5;
            constexpr const char* kManagedEntryType = "Canis.ManagedHost.EntryPoints, Canis.ManagedHost";

            const auto hostFxrPath = FindLatestHostFxrPath<decltype(ToHostString(std::string()))>();
            if (!hostFxrPath.has_value())
                return false;

            ManagedHostFunctionTable table = {};
            table.library = OpenHostLibrary(hostFxrPath.value());
            if (table.library == nullptr)
                return false;

            auto* initializeForRuntimeConfig = reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>(
                LoadHostSymbol(table.library, "hostfxr_initialize_for_runtime_config"));
            auto* getRuntimeDelegate = reinterpret_cast<hostfxr_get_runtime_delegate_fn>(
                LoadHostSymbol(table.library, "hostfxr_get_runtime_delegate"));
            auto* closeHostContext = reinterpret_cast<hostfxr_close_fn>(
                LoadHostSymbol(table.library, "hostfxr_close"));

            if (initializeForRuntimeConfig == nullptr || getRuntimeDelegate == nullptr || closeHostContext == nullptr)
            {
                CloseHostLibrary(table.library);
                return false;
            }

            hostfxr_handle context = nullptr;
            const auto runtimeConfigPath = ToHostString(_runtimeConfigPath);
            if (initializeForRuntimeConfig(runtimeConfigPath.c_str(), nullptr, &context) != 0 || context == nullptr)
            {
                CloseHostLibrary(table.library);
                return false;
            }

            void* loadAssemblyFunction = nullptr;
            const int delegateResult = getRuntimeDelegate(
                context,
                hdt_load_assembly_and_get_function_pointer,
                &loadAssemblyFunction);
            closeHostContext(context);

            if (delegateResult != 0 || loadAssemblyFunction == nullptr)
            {
                CloseHostLibrary(table.library);
                return false;
            }

            table.loadAssemblyAndGetFunctionPointer =
                reinterpret_cast<ManagedHostFunctionTable::load_assembly_and_get_function_pointer_fn>(loadAssemblyFunction);

            auto requireEntrypoint = [&](const char* _methodName, void** _slot) -> bool
            {
                return LoadManagedHostEntrypoint(table, _assemblyPath, kManagedEntryType, _methodName, _slot);
            };

            if (!requireEntrypoint("InitializeManagedHost", reinterpret_cast<void**>(&table.initializeManagedHost)) ||
                !requireEntrypoint("ShutdownManagedHost", reinterpret_cast<void**>(&table.shutdownManagedHost)) ||
                !requireEntrypoint("InstantiateScript", reinterpret_cast<void**>(&table.instantiateScript)) ||
                !requireEntrypoint("InvokeCreate", reinterpret_cast<void**>(&table.invokeCreate)) ||
                !requireEntrypoint("InvokeReady", reinterpret_cast<void**>(&table.invokeReady)) ||
                !requireEntrypoint("InvokeDestroy", reinterpret_cast<void**>(&table.invokeDestroy)) ||
                !requireEntrypoint("UpdateScript", reinterpret_cast<void**>(&table.updateScript)) ||
                !requireEntrypoint("ReleaseScript", reinterpret_cast<void**>(&table.releaseScript)) ||
                !requireEntrypoint("SetBool", reinterpret_cast<void**>(&table.setBool)) ||
                !requireEntrypoint("GetBool", reinterpret_cast<void**>(&table.getBool)) ||
                !requireEntrypoint("SetInt", reinterpret_cast<void**>(&table.setInt)) ||
                !requireEntrypoint("GetInt", reinterpret_cast<void**>(&table.getInt)) ||
                !requireEntrypoint("SetFloat", reinterpret_cast<void**>(&table.setFloat)) ||
                !requireEntrypoint("GetFloat", reinterpret_cast<void**>(&table.getFloat)) ||
                !requireEntrypoint("SetString", reinterpret_cast<void**>(&table.setString)) ||
                !requireEntrypoint("GetString", reinterpret_cast<void**>(&table.getString)) ||
                !requireEntrypoint("SetVec2", reinterpret_cast<void**>(&table.setVec2)) ||
                !requireEntrypoint("GetVec2", reinterpret_cast<void**>(&table.getVec2)) ||
                !requireEntrypoint("SetVec3", reinterpret_cast<void**>(&table.setVec3)) ||
                !requireEntrypoint("GetVec3", reinterpret_cast<void**>(&table.getVec3)) ||
                !requireEntrypoint("SetVec4", reinterpret_cast<void**>(&table.setVec4)) ||
                !requireEntrypoint("GetVec4", reinterpret_cast<void**>(&table.getVec4)))
            {
                CloseHostLibrary(table.library);
                return false;
            }

            _outTable = table;
            return true;
#endif
        }

        void ReleaseManagedHostFunctionTable(ManagedHostFunctionTable& _table)
        {
            if (_table.shutdownManagedHost != nullptr)
                _table.shutdownManagedHost();

            _table = {};
        }
    } // namespace

    struct ManagedScriptRuntime::ManagedHostRuntimeHandle
    {
        ManagedHostFunctionTable* table = nullptr;
        bool sessionInitialized = false;
    };

    const char* ManagedPropertyTypeLabel(const ManagedPropertyType _type)
    {
        switch (_type)
        {
            case ManagedPropertyType::Bool:   return "Bool";
            case ManagedPropertyType::Int:    return "Int";
            case ManagedPropertyType::Float:  return "Float";
            case ManagedPropertyType::String: return "String";
            case ManagedPropertyType::Vec2:   return "Vector2";
            case ManagedPropertyType::Vec3:   return "Vector3";
            case ManagedPropertyType::Vec4:   return "Vector4";
            case ManagedPropertyType::Color:  return "Color";
            default:                          return "None";
        }
    }

    ManagedPropertyType ManagedPropertyTypeFromString(const std::string& _typeName)
    {
        const std::string normalized = ToLowerCopy(_typeName);
        if (normalized == "bool" || normalized == "boolean")
            return ManagedPropertyType::Bool;
        if (normalized == "int" || normalized == "integer")
            return ManagedPropertyType::Int;
        if (normalized == "float" || normalized == "single")
            return ManagedPropertyType::Float;
        if (normalized == "string")
            return ManagedPropertyType::String;
        if (normalized == "vec2" || normalized == "vector2")
            return ManagedPropertyType::Vec2;
        if (normalized == "vec3" || normalized == "vector3")
            return ManagedPropertyType::Vec3;
        if (normalized == "vec4" || normalized == "vector4")
            return ManagedPropertyType::Vec4;
        if (normalized == "color")
            return ManagedPropertyType::Color;

        return ManagedPropertyType::None;
    }

    bool ManagedPropertyValueFromYaml(const ManagedPropertyType _type, const YAML::Node& _node, ManagedPropertyValue& _outValue)
    {
        try
        {
            switch (_type)
            {
                case ManagedPropertyType::Bool:   _outValue = _node.as<bool>(false); return true;
                case ManagedPropertyType::Int:    _outValue = _node.as<int>(0); return true;
                case ManagedPropertyType::Float:  _outValue = _node.as<float>(0.0f); return true;
                case ManagedPropertyType::String: _outValue = _node.as<std::string>(""); return true;
                case ManagedPropertyType::Vec2:   _outValue = _node.as<Vector2>(Vector2(0.0f)); return true;
                case ManagedPropertyType::Vec3:   _outValue = _node.as<Vector3>(Vector3(0.0f)); return true;
                case ManagedPropertyType::Vec4:   _outValue = _node.as<Vector4>(Vector4(0.0f)); return true;
                case ManagedPropertyType::Color:
                {
                    const Color color = _node.as<Color>(Color(1.0f));
                    _outValue = Vector4(color.r, color.g, color.b, color.a);
                    return true;
                }
                case ManagedPropertyType::None:
                default:
                    break;
            }
        }
        catch (const YAML::Exception&)
        {
        }

        return false;
    }

    YAML::Node ManagedPropertyValueToYaml(const ManagedPropertyValue& _value)
    {
        if (const bool* typedValue = std::get_if<bool>(&_value))
            return YAML::Node(*typedValue);
        if (const int* typedValue = std::get_if<int>(&_value))
            return YAML::Node(*typedValue);
        if (const float* typedValue = std::get_if<float>(&_value))
            return YAML::Node(*typedValue);
        if (const std::string* typedValue = std::get_if<std::string>(&_value))
            return YAML::Node(*typedValue);
        if (const Vector2* typedValue = std::get_if<Vector2>(&_value))
            return YAML::Node(*typedValue);
        if (const Vector3* typedValue = std::get_if<Vector3>(&_value))
            return YAML::Node(*typedValue);
        if (const Vector4* typedValue = std::get_if<Vector4>(&_value))
            return YAML::Node(*typedValue);

        return {};
    }

    bool ManagedPropertyValueSupportsAnimation(const ManagedPropertyType _type)
    {
        return _type == ManagedPropertyType::Bool ||
            _type == ManagedPropertyType::Int ||
            _type == ManagedPropertyType::Float ||
            _type == ManagedPropertyType::Vec2 ||
            _type == ManagedPropertyType::Vec3 ||
            _type == ManagedPropertyType::Vec4 ||
            _type == ManagedPropertyType::Color;
    }

    AnimationValueType ManagedPropertyAnimationType(const ManagedPropertyType _type)
    {
        switch (_type)
        {
            case ManagedPropertyType::Bool:  return AnimationValueType::BOOL;
            case ManagedPropertyType::Int:   return AnimationValueType::INT;
            case ManagedPropertyType::Float: return AnimationValueType::FLOAT;
            case ManagedPropertyType::Vec2:  return AnimationValueType::VEC2;
            case ManagedPropertyType::Vec3:  return AnimationValueType::VEC3;
            case ManagedPropertyType::Vec4:  return AnimationValueType::VEC4;
            case ManagedPropertyType::Color: return AnimationValueType::VEC4;
            default:                         return AnimationValueType::NONE;
        }
    }

    AnimationInterpolation ManagedPropertyAnimationInterpolation(const ManagedPropertyType _type)
    {
        return GetDefaultAnimationInterpolation(ManagedPropertyAnimationType(_type));
    }

    AnimationValue ManagedPropertyToAnimationValue(const ManagedPropertyValue& _value, const ManagedPropertyType _type)
    {
        switch (_type)
        {
            case ManagedPropertyType::Bool:
                return AnimationValue::Bool(std::get<bool>(_value));
            case ManagedPropertyType::Int:
                return AnimationValue::Int(std::get<int>(_value));
            case ManagedPropertyType::Float:
                return AnimationValue::Float(std::get<float>(_value));
            case ManagedPropertyType::Vec2:
                return AnimationValue::Vec2(std::get<Vector2>(_value));
            case ManagedPropertyType::Vec3:
                return AnimationValue::Vec3(std::get<Vector3>(_value));
            case ManagedPropertyType::Vec4:
                return AnimationValue::Vec4(std::get<Vector4>(_value));
            case ManagedPropertyType::Color:
            {
                return AnimationValue::Vec4(ManagedColorVectorFromValue(_value));
            }
            default:
                return {};
        }
    }

    bool ManagedPropertyFromAnimationValue(const AnimationValue& _value, const ManagedPropertyType _type, ManagedPropertyValue& _inOutValue)
    {
        switch (_type)
        {
            case ManagedPropertyType::Bool:
                _inOutValue = _value.AsBool(std::get_if<bool>(&_inOutValue) ? std::get<bool>(_inOutValue) : false);
                return true;
            case ManagedPropertyType::Int:
                _inOutValue = _value.AsInt(std::get_if<int>(&_inOutValue) ? std::get<int>(_inOutValue) : 0);
                return true;
            case ManagedPropertyType::Float:
                _inOutValue = _value.AsFloat(std::get_if<float>(&_inOutValue) ? std::get<float>(_inOutValue) : 0.0f);
                return true;
            case ManagedPropertyType::Vec2:
                _inOutValue = _value.AsVec2(std::get_if<Vector2>(&_inOutValue) ? std::get<Vector2>(_inOutValue) : Vector2(0.0f));
                return true;
            case ManagedPropertyType::Vec3:
                _inOutValue = _value.AsVec3(std::get_if<Vector3>(&_inOutValue) ? std::get<Vector3>(_inOutValue) : Vector3(0.0f));
                return true;
            case ManagedPropertyType::Vec4:
                _inOutValue = _value.AsVec4(std::get_if<Vector4>(&_inOutValue) ? std::get<Vector4>(_inOutValue) : Vector4(0.0f));
                return true;
            case ManagedPropertyType::Color:
            {
                const Vector4 vec4Value = _value.AsVec4(Vector4(1.0f));
                _inOutValue = vec4Value;
                return true;
            }
            default:
                return false;
        }
    }

    bool ManagedPropertyValuesDiffer(const ManagedPropertyValue& _left, const ManagedPropertyValue& _right, const ManagedPropertyType _type)
    {
        switch (_type)
        {
            case ManagedPropertyType::Bool:
                return std::get<bool>(_left) != std::get<bool>(_right);
            case ManagedPropertyType::Int:
                return std::get<int>(_left) != std::get<int>(_right);
            case ManagedPropertyType::Float:
                return std::fabs(std::get<float>(_left) - std::get<float>(_right)) > 0.00001f;
            case ManagedPropertyType::String:
                return std::get<std::string>(_left) != std::get<std::string>(_right);
            case ManagedPropertyType::Vec2:
            {
                const Vector2 left = std::get<Vector2>(_left);
                const Vector2 right = std::get<Vector2>(_right);
                return std::fabs(left.x - right.x) > 0.00001f || std::fabs(left.y - right.y) > 0.00001f;
            }
            case ManagedPropertyType::Vec3:
            {
                const Vector3 left = std::get<Vector3>(_left);
                const Vector3 right = std::get<Vector3>(_right);
                return std::fabs(left.x - right.x) > 0.00001f ||
                    std::fabs(left.y - right.y) > 0.00001f ||
                    std::fabs(left.z - right.z) > 0.00001f;
            }
            case ManagedPropertyType::Vec4:
            {
                const Vector4 left = std::get<Vector4>(_left);
                const Vector4 right = std::get<Vector4>(_right);
                return std::fabs(left.x - right.x) > 0.00001f ||
                    std::fabs(left.y - right.y) > 0.00001f ||
                    std::fabs(left.z - right.z) > 0.00001f ||
                    std::fabs(left.w - right.w) > 0.00001f;
            }
            case ManagedPropertyType::Color:
            {
                const Vector4 left = ManagedColorVectorFromValue(_left);
                const Vector4 right = ManagedColorVectorFromValue(_right);
                return std::fabs(left.x - right.x) > 0.00001f ||
                    std::fabs(left.y - right.y) > 0.00001f ||
                    std::fabs(left.z - right.z) > 0.00001f ||
                    std::fabs(left.w - right.w) > 0.00001f;
            }
            default:
                return false;
        }
    }

    ManagedScriptComponent::ManagedScriptComponent(Entity& _entity, std::shared_ptr<const ManagedScriptDefinition> _definition)
        : ScriptableEntity(_entity), m_definition(std::move(_definition))
    {
        ResetToDefaults();
    }

    const std::string& ManagedScriptComponent::GetScriptName() const
    {
        return m_definition != nullptr ? m_definition->scriptName : EmptyStringRef();
    }

    const std::string& ManagedScriptComponent::GetManagedTypeName() const
    {
        return m_definition != nullptr ? m_definition->typeName : EmptyStringRef();
    }

    const std::string& ManagedScriptComponent::GetManagedAssemblyPath() const
    {
        return m_definition != nullptr ? m_definition->assemblyPath : EmptyStringRef();
    }

    void ManagedScriptComponent::ResetToDefaults()
    {
        m_propertyValues.clear();
        if (m_definition == nullptr)
            return;

        for (const ManagedPropertyDefinition& property : m_definition->properties)
            m_propertyValues[property.name] = property.defaultValue;
    }

    bool ManagedScriptComponent::HasProperty(const std::string& _name) const
    {
        return m_propertyValues.contains(_name);
    }

    ManagedPropertyValue* ManagedScriptComponent::GetPropertyValue(const std::string& _name)
    {
        auto it = m_propertyValues.find(_name);
        return it == m_propertyValues.end() ? nullptr : &it->second;
    }

    const ManagedPropertyValue* ManagedScriptComponent::GetPropertyValue(const std::string& _name) const
    {
        auto it = m_propertyValues.find(_name);
        return it == m_propertyValues.end() ? nullptr : &it->second;
    }

    bool ManagedScriptComponent::SetPropertyValue(const std::string& _name, const ManagedPropertyValue& _value)
    {
        auto it = m_propertyValues.find(_name);
        if (it == m_propertyValues.end())
            return false;

        it->second = _value;
        return true;
    }

    YAML::Node ManagedScriptComponent::GetPropertyAsYaml(const std::string& _name) const
    {
        if (const ManagedPropertyValue* value = GetPropertyValue(_name))
            return ManagedPropertyValueToYaml(*value);

        return {};
    }

    bool ManagedScriptComponent::SetPropertyFromYaml(const std::string& _name, const YAML::Node& _node)
    {
        if (m_definition == nullptr)
            return false;

        auto definitionIt = std::find_if(m_definition->properties.begin(), m_definition->properties.end(),
            [&_name](const ManagedPropertyDefinition& property)
            {
                return property.name == _name;
            });
        if (definitionIt == m_definition->properties.end())
            return false;

        ManagedPropertyValue value = definitionIt->defaultValue;
        if (!ManagedPropertyValueFromYaml(definitionIt->type, _node, value))
            return false;

        m_propertyValues[_name] = value;
        return true;
    }

    void ManagedScriptComponent::SyncPropertiesToManaged()
    {
        if (!m_managedRuntimeLive || entity.scene.app == nullptr || m_definition == nullptr)
            return;

        if (ManagedScriptRuntime* runtime = entity.scene.app->TryGetManagedScriptRuntime())
        {
            for (const ManagedPropertyDefinition& property : m_definition->properties)
            {
                if (const ManagedPropertyValue* value = GetPropertyValue(property.name))
                    (void)runtime->SyncPropertyToManaged(*this, property, *value);
            }
        }
    }

    void ManagedScriptComponent::SyncPropertiesFromManaged()
    {
        if (!m_managedRuntimeLive || entity.scene.app == nullptr || m_definition == nullptr)
            return;

        if (ManagedScriptRuntime* runtime = entity.scene.app->TryGetManagedScriptRuntime())
        {
            for (const ManagedPropertyDefinition& property : m_definition->properties)
            {
                if (ManagedPropertyValue* value = GetPropertyValue(property.name))
                    (void)runtime->SyncPropertyFromManaged(*this, property, *value);
            }
        }
    }

    void ManagedScriptComponent::Create()
    {
        if (entity.scene.app == nullptr)
            return;

        if (ManagedScriptRuntime* runtime = entity.scene.app->TryGetManagedScriptRuntime())
        {
            m_managedRuntimeLive = runtime->CreateInstance(*this);
            if (!m_managedRuntimeLive)
                return;

            SyncPropertiesToManaged();
            runtime->InvokeCreate(*this);
            SyncPropertiesFromManaged();
        }
    }

    void ManagedScriptComponent::Ready()
    {
        if (!m_managedRuntimeLive || entity.scene.app == nullptr)
            return;

        if (ManagedScriptRuntime* runtime = entity.scene.app->TryGetManagedScriptRuntime())
        {
            SyncPropertiesToManaged();
            runtime->ReadyInstance(*this);
            SyncPropertiesFromManaged();
        }
    }

    void ManagedScriptComponent::Destroy()
    {
        if (!m_managedRuntimeLive || entity.scene.app == nullptr)
            return;

        if (ManagedScriptRuntime* runtime = entity.scene.app->TryGetManagedScriptRuntime())
        {
            runtime->DestroyInstance(*this);
            m_managedRuntimeLive = false;
        }
    }

    void ManagedScriptComponent::Update(float _dt)
    {
        if (!m_managedRuntimeLive || entity.scene.app == nullptr)
            return;

        if (ManagedScriptRuntime* runtime = entity.scene.app->TryGetManagedScriptRuntime())
        {
            SyncPropertiesToManaged();
            runtime->UpdateInstance(*this, _dt);
            SyncPropertiesFromManaged();
        }
    }

    ManagedScriptRuntime::~ManagedScriptRuntime()
    {
        Shutdown();
    }

    void ManagedScriptRuntime::Initialize(App& _app)
    {
        if (m_app != nullptr)
            return;

        m_app = &_app;
        m_managedRootPath = ResolvePreferredManagedPath(kManagedScriptsRoot, kLegacyManagedScriptsRoot);
        m_hostAssemblyPath = ResolvePreferredManagedPath(kManagedHostAssemblyPath, kLegacyManagedHostAssemblyPath);
        m_runtimeConfigPath = ResolvePreferredManagedPath(kManagedHostRuntimeConfigPath, kLegacyManagedHostRuntimeConfigPath);

        DiscoverScriptDefinitions();
        RegisterDiscoveredScripts();
        TryInitializeManagedHost();
    }

    void ManagedScriptRuntime::Shutdown()
    {
        if (m_app != nullptr)
        {
            for (ScriptConf& conf : m_registeredScriptConfs)
                m_app->UnregisterScript(conf);
        }

        if (m_host != nullptr)
        {
            if (m_host->sessionInitialized && m_host->table != nullptr && m_host->table->shutdownManagedHost != nullptr)
                m_host->table->shutdownManagedHost();
            delete m_host;
            m_host = nullptr;
        }

        m_registeredScriptConfs.clear();
        m_definitions.clear();
        m_app = nullptr;
        m_hostAvailable = false;
        m_hostInitialized = false;
        m_loggedUnavailable = false;
        g_managedCallbackApp = nullptr;
    }

    bool ManagedScriptRuntime::CreateInstance(ManagedScriptComponent& _script)
    {
        if (!m_hostAvailable || m_host == nullptr || _script.GetDefinition() == nullptr)
        {
            if (!m_loggedUnavailable)
            {
                Debug::Warning("Managed scripting is registered, but the .NET managed host is unavailable. Expected '%s' and '%s'.",
                    m_hostAssemblyPath.c_str(),
                    m_runtimeConfigPath.c_str());
                m_loggedUnavailable = true;
            }
            return false;
        }

        const ManagedScriptDefinition& definition = *_script.GetDefinition();
        return m_host->table->instantiateScript(
            static_cast<uint64_t>(_script.entity.uuid),
            definition.scriptName.c_str(),
            definition.typeName.c_str(),
            definition.assemblyPath.c_str()) != 0;
    }

    void ManagedScriptRuntime::InvokeCreate(ManagedScriptComponent& _script)
    {
        if (m_hostAvailable && m_host != nullptr && _script.GetDefinition() != nullptr)
            m_host->table->invokeCreate(static_cast<uint64_t>(_script.entity.uuid), _script.GetScriptName().c_str());
    }

    void ManagedScriptRuntime::ReadyInstance(ManagedScriptComponent& _script)
    {
        if (m_hostAvailable && m_host != nullptr && _script.GetDefinition() != nullptr)
            m_host->table->invokeReady(static_cast<uint64_t>(_script.entity.uuid), _script.GetScriptName().c_str());
    }

    void ManagedScriptRuntime::DestroyInstance(ManagedScriptComponent& _script)
    {
        if (!m_hostAvailable || m_host == nullptr || _script.GetDefinition() == nullptr)
            return;

        m_host->table->invokeDestroy(static_cast<uint64_t>(_script.entity.uuid), _script.GetScriptName().c_str());
        m_host->table->releaseScript(static_cast<uint64_t>(_script.entity.uuid), _script.GetScriptName().c_str());
    }

    void ManagedScriptRuntime::UpdateInstance(ManagedScriptComponent& _script, float _dt)
    {
        if (m_hostAvailable && m_host != nullptr && _script.GetDefinition() != nullptr)
            m_host->table->updateScript(static_cast<uint64_t>(_script.entity.uuid), _script.GetScriptName().c_str(), _dt);
    }

    bool ManagedScriptRuntime::SyncPropertyToManaged(ManagedScriptComponent& _script, const ManagedPropertyDefinition& _definition, const ManagedPropertyValue& _value)
    {
        if (!m_hostAvailable || m_host == nullptr)
            return false;

        const uint64_t entityUuid = static_cast<uint64_t>(_script.entity.uuid);
        const char* scriptName = _script.GetScriptName().c_str();
        const char* propertyName = _definition.name.c_str();

        switch (_definition.type)
        {
            case ManagedPropertyType::Bool:
                m_host->table->setBool(entityUuid, scriptName, propertyName, std::get<bool>(_value) ? 1 : 0);
                return true;
            case ManagedPropertyType::Int:
                m_host->table->setInt(entityUuid, scriptName, propertyName, std::get<int>(_value));
                return true;
            case ManagedPropertyType::Float:
                m_host->table->setFloat(entityUuid, scriptName, propertyName, std::get<float>(_value));
                return true;
            case ManagedPropertyType::String:
                m_host->table->setString(entityUuid, scriptName, propertyName, std::get<std::string>(_value).c_str());
                return true;
            case ManagedPropertyType::Vec2:
            {
                const Vector2 value = std::get<Vector2>(_value);
                m_host->table->setVec2(entityUuid, scriptName, propertyName, value.x, value.y);
                return true;
            }
            case ManagedPropertyType::Vec3:
            {
                const Vector3 value = std::get<Vector3>(_value);
                m_host->table->setVec3(entityUuid, scriptName, propertyName, value.x, value.y, value.z);
                return true;
            }
            case ManagedPropertyType::Vec4:
            {
                const Vector4 value = std::get<Vector4>(_value);
                m_host->table->setVec4(entityUuid, scriptName, propertyName, value.x, value.y, value.z, value.w);
                return true;
            }
            case ManagedPropertyType::Color:
            {
                const Vector4 value = ManagedColorVectorFromValue(_value);
                m_host->table->setVec4(entityUuid, scriptName, propertyName, value.x, value.y, value.z, value.w);
                return true;
            }
            default:
                return false;
        }
    }

    bool ManagedScriptRuntime::SyncPropertyFromManaged(ManagedScriptComponent& _script, const ManagedPropertyDefinition& _definition, ManagedPropertyValue& _value)
    {
        if (!m_hostAvailable || m_host == nullptr)
            return false;

        const uint64_t entityUuid = static_cast<uint64_t>(_script.entity.uuid);
        const char* scriptName = _script.GetScriptName().c_str();
        const char* propertyName = _definition.name.c_str();

        switch (_definition.type)
        {
            case ManagedPropertyType::Bool:
                _value = m_host->table->getBool(entityUuid, scriptName, propertyName) != 0;
                return true;
            case ManagedPropertyType::Int:
                _value = m_host->table->getInt(entityUuid, scriptName, propertyName);
                return true;
            case ManagedPropertyType::Float:
                _value = m_host->table->getFloat(entityUuid, scriptName, propertyName);
                return true;
            case ManagedPropertyType::String:
            {
                std::array<char, 2048> buffer = {};
                const int copied = m_host->table->getString(entityUuid, scriptName, propertyName, buffer.data(), static_cast<int>(buffer.size()));
                if (copied >= 0)
                    _value = std::string(buffer.data());
                return copied >= 0;
            }
            case ManagedPropertyType::Vec2:
            {
                float x = 0.0f;
                float y = 0.0f;
                m_host->table->getVec2(entityUuid, scriptName, propertyName, &x, &y);
                _value = Vector2(x, y);
                return true;
            }
            case ManagedPropertyType::Vec3:
            {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                m_host->table->getVec3(entityUuid, scriptName, propertyName, &x, &y, &z);
                _value = Vector3(x, y, z);
                return true;
            }
            case ManagedPropertyType::Vec4:
            {
                float x = 0.0f;
                float y = 0.0f;
                float z = 0.0f;
                float w = 0.0f;
                m_host->table->getVec4(entityUuid, scriptName, propertyName, &x, &y, &z, &w);
                _value = Vector4(x, y, z, w);
                return true;
            }
            case ManagedPropertyType::Color:
            {
                float x = 1.0f;
                float y = 1.0f;
                float z = 1.0f;
                float w = 1.0f;
                m_host->table->getVec4(entityUuid, scriptName, propertyName, &x, &y, &z, &w);
                _value = Vector4(x, y, z, w);
                return true;
            }
            default:
                return false;
        }
    }

    void ManagedScriptRuntime::DiscoverScriptDefinitions()
    {
        m_definitions.clear();

        for (const fs::path& manifestPath : FindManagedManifestFiles())
        {
            for (const std::shared_ptr<const ManagedScriptDefinition>& definition : ParseManagedManifestFile(manifestPath))
            {
                if (definition == nullptr || definition->scriptName.empty())
                    continue;

                const bool duplicate = std::any_of(m_definitions.begin(), m_definitions.end(),
                    [&definition](const std::shared_ptr<const ManagedScriptDefinition>& existing)
                    {
                        return existing != nullptr && existing->scriptName == definition->scriptName;
                    });
                if (duplicate)
                {
                    Debug::Warning("Ignoring duplicate managed script definition '%s'.", definition->scriptName.c_str());
                    continue;
                }

                m_definitions.push_back(definition);
            }
        }
    }

    void ManagedScriptRuntime::RegisterDiscoveredScripts()
    {
        if (m_app == nullptr)
            return;

        m_registeredScriptConfs.clear();
        for (const std::shared_ptr<const ManagedScriptDefinition>& definition : m_definitions)
            RegisterScriptDefinition(definition);

        for (ScriptConf& conf : m_registeredScriptConfs)
            m_app->RegisterScript(conf);
    }

    void ManagedScriptRuntime::TryInitializeManagedHost()
    {
        m_hostAvailable = false;
        m_hostInitialized = false;
        g_managedCallbackApp = m_app;

        if (!fs::exists(m_runtimeConfigPath) || !fs::exists(m_hostAssemblyPath))
            return;

        ManagedHostFunctionTable* processTable = AcquireProcessManagedHostFunctionTable(m_runtimeConfigPath, m_hostAssemblyPath);
        if (processTable == nullptr)
        {
            Debug::Warning("Failed to initialize the .NET managed host from '%s'.", m_hostAssemblyPath.c_str());
            return;
        }

        ManagedHostRuntimeHandle* runtime = new ManagedHostRuntimeHandle();
        runtime->table = processTable;
        const ManagedNativeCallbacks callbacks = BuildManagedNativeCallbacks();
        if (runtime->table->initializeManagedHost(&callbacks, m_managedRootPath.c_str()) == 0)
        {
            delete runtime;
            return;
        }

        runtime->sessionInitialized = true;
        m_host = runtime;
        m_hostAvailable = true;
        m_hostInitialized = true;
        m_loggedUnavailable = false;
        Debug::Log("Managed scripting host initialized from '%s'.", m_hostAssemblyPath.c_str());
    }

    std::shared_ptr<const ManagedScriptDefinition> ManagedScriptRuntime::FindDefinition(const std::string& _scriptName) const
    {
        for (const std::shared_ptr<const ManagedScriptDefinition>& definition : m_definitions)
        {
            if (definition != nullptr && definition->scriptName == _scriptName)
                return definition;
        }

        return {};
    }

    void ManagedScriptRuntime::RegisterScriptDefinition(const std::shared_ptr<const ManagedScriptDefinition>& _definition)
    {
        if (m_app == nullptr || _definition == nullptr)
            return;

        ScriptConf conf = {};
        conf.kind = RegistryEntryKind::Script;
        conf.name = _definition->scriptName;
        conf.Construct =
            [definition = _definition](Entity& _entity, bool _callCreate) -> ScriptableEntity*
            {
                return _entity.AttachScript(definition->scriptName, new ManagedScriptComponent(_entity, definition), _callCreate);
            };
        conf.Add =
            [definition = _definition](Entity& _entity) -> void
            {
                (void)_entity.AttachScript(definition->scriptName, new ManagedScriptComponent(_entity, definition), true);
            };
        conf.Has =
            [scriptName = _definition->scriptName](Entity& _entity) -> bool
            {
                return _entity.HasScriptByName(scriptName);
            };
        conf.Get =
            [scriptName = _definition->scriptName](Entity& _entity) -> void*
            {
                return static_cast<void*>(_entity.GetScriptByName(scriptName));
            };
        conf.Remove =
            [scriptName = _definition->scriptName](Entity& _entity) -> void
            {
                _entity.RemoveScriptByName(scriptName);
            };
        conf.Encode =
            [definition = _definition](YAML::Node& _node, Entity& _entity) -> void
            {
                if (ManagedScriptComponent* component = dynamic_cast<ManagedScriptComponent*>(_entity.GetScriptByName(definition->scriptName)))
                {
                    YAML::Node componentNode(YAML::NodeType::Map);
                    for (const ManagedPropertyDefinition& property : definition->properties)
                        componentNode[property.name] = component->GetPropertyAsYaml(property.name);

                    _node[definition->scriptName] = componentNode;
                }
            };
        conf.Decode =
            [definition = _definition](YAML::Node& _node, Entity& _entity, bool _callCreate) -> void
            {
                YAML::Node componentNode = _node[definition->scriptName];
                if (!componentNode || !componentNode.IsMap())
                    return;

                auto* component = dynamic_cast<ManagedScriptComponent*>(
                    _entity.AttachScript(definition->scriptName, new ManagedScriptComponent(_entity, definition), false));
                if (component == nullptr)
                    return;

                for (const ManagedPropertyDefinition& property : definition->properties)
                {
                    if (YAML::Node propertyNode = componentNode[property.name])
                        (void)component->SetPropertyFromYaml(property.name, propertyNode);
                }

                if (_callCreate)
                    component->Create();
            };
        conf.DrawInspector =
            [this, definition = _definition](Editor& _editor, Entity& _entity, const ScriptConf& _conf) -> void
            {
                if (ManagedScriptComponent* component = dynamic_cast<ManagedScriptComponent*>(_entity.GetScriptByName(definition->scriptName)))
                {
#if CANIS_EDITOR
                    ImGui::TextUnformatted(definition->typeName.c_str());
                    if (!definition->assemblyPath.empty())
                    {
                        ImGui::TextDisabled("%s", definition->assemblyPath.c_str());
                    }
                    if (!this->IsManagedHostAvailable())
                    {
                        ImGui::TextColored(ImVec4(0.95f, 0.74f, 0.3f, 1.0f), "Managed host unavailable: inspector/scene values still work, runtime execution is disabled.");
                    }
#endif
                    DrawRegisteredProperties(_editor, _conf.registry, component, _conf.name);
                }
            };

        for (const ManagedPropertyDefinition& property : _definition->properties)
            RegisterManagedProperty(conf, property);

        m_registeredScriptConfs.push_back(conf);
    }
}
