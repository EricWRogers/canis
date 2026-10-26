#pragma once

#include <Canis/Entity.hpp>
#include <Canis/External/GetNameOfType.hpp>
#include <functional>
#include <string>
#include <typeindex>
#include <type_traits>
#include <variant>
#include <vector>

namespace YAML { class Node; }

namespace Canis
{
    class Editor;
    using SignalConstant = std::variant<std::monostate, float, int, bool, std::string>;
    enum class SignalArgumentMode { Dynamic, Constant };

    // Only stable names, entity references and constants are saved. Script
    // instances and native function pointers are resolved when the signal fires.
    struct SignalListener
    {
        Entity target = nullptr;
        std::string script;
        std::string method;
        bool enabled = true;
        SignalArgumentMode argumentMode = SignalArgumentMode::Dynamic;
        SignalConstant constant;
    };

    struct SignalAction
    {
        std::type_index argumentType = typeid(void);
        std::string argumentName;
        std::function<void(ScriptableEntity&, const void*)> invoke;
    };

    template<class T> std::string SignalArgumentName()
    {
        if constexpr (std::is_void_v<T>) return "";
        else if constexpr (std::is_same_v<T, float>) return "float";
        else if constexpr (std::is_same_v<T, int>) return "int";
        else if constexpr (std::is_same_v<T, bool>) return "bool";
        else if constexpr (std::is_same_v<T, std::string>) return "string";
        else return std::string(type_name<T>());
    }

    class SignalBase
    {
    public:
        std::vector<SignalListener> listeners;
        std::type_index ArgumentType() const { return m_argumentType; }
        const std::string& ArgumentName() const { return m_argumentName; }
        bool CanInvoke(const SignalListener& listener, const SignalAction& action) const;
        // False means unassigned, missing or incompatible. Never invokes a
        // different script as a fallback for an authored binding.
        bool IsListenerValid(const SignalListener& listener) const;
    protected:
        SignalBase(std::type_index type, std::string name) : m_argumentType(type), m_argumentName(std::move(name)) {}
        static void EmitPersistent(const std::vector<SignalListener>& listeners,
                                   std::type_index argumentType, const void* value);
    private:
        std::type_index m_argumentType;
        std::string m_argumentName;
    };

    void InvokeSignalAction(ScriptableEntity& script, const std::function<void()>& action);

    // Zero or one typed payload. Structured C++ payloads work without converting
    // them to strings/YAML; only persistent listener bindings are serialized.
    template<class T = void> class Signal : public SignalBase
    {
        static_assert(std::is_same_v<T, std::remove_cvref_t<T>>);
    public:
        using Callback = std::function<void(const T&)>;
        using Connection = uint64_t;
        Signal() : SignalBase(typeid(T), SignalArgumentName<T>()) {}
        Connection Connect(Callback callback)
        {
            const auto id = ++m_nextConnection;
            m_callbacks.emplace_back(id, std::move(callback));
            return id;
        }
        template<class Receiver> Connection Connect(ScriptHandle<Receiver> receiver, void(Receiver::*method)(const T&))
        {
            return Connect([receiver, method](const T& value) { if (auto* instance = receiver.TryGet()) InvokeSignalAction(*instance, [&] { (instance->*method)(value); }); });
        }
        bool Disconnect(Connection id) { return std::erase_if(m_callbacks, [id](const auto& item) { return item.first == id; }) != 0; }
        void Emit(T value) const
        {
            // Snapshot before invoking: listeners may edit the list or destroy
            // the emitter. Changes to connections take effect on the next Emit.
            const auto persistent = listeners;
            const auto callbacks = m_callbacks;
            EmitPersistent(persistent, typeid(T), &value);
            for (const auto& [id, callback] : callbacks) if (callback) callback(value);
        }
    private:
        Connection m_nextConnection = 0;
        std::vector<std::pair<Connection, Callback>> m_callbacks;
    };

    template<> class Signal<void> : public SignalBase
    {
    public:
        using Callback = std::function<void()>;
        using Connection = uint64_t;
        Signal() : SignalBase(typeid(void), "") {}
        Connection Connect(Callback callback)
        {
            const auto id = ++m_nextConnection;
            m_callbacks.emplace_back(id, std::move(callback));
            return id;
        }
        template<class Receiver> Connection Connect(ScriptHandle<Receiver> receiver, void(Receiver::*method)())
        {
            return Connect([receiver, method] { if (auto* instance = receiver.TryGet()) InvokeSignalAction(*instance, [&] { (instance->*method)(); }); });
        }
        bool Disconnect(Connection id) { return std::erase_if(m_callbacks, [id](const auto& item) { return item.first == id; }) != 0; }
        void Emit() const
        {
            const auto persistent = listeners;
            const auto callbacks = m_callbacks;
            EmitPersistent(persistent, typeid(void), nullptr);
            for (const auto& [id, callback] : callbacks) if (callback) callback();
        }
    private:
        Connection m_nextConnection = 0;
        std::vector<std::pair<Connection, Callback>> m_callbacks;
    };

    YAML::Node EncodeSignal(const SignalBase& signal);
    bool DecodeSignal(const YAML::Node& node, Scene& scene, SignalBase& signal);

    void DrawSignalInspector(Editor* editor, const char* label, const char* suffix, SignalBase& signal);
}
