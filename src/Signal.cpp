#include <Canis/Signal.hpp>
#include <Canis/App.hpp>
#include <Canis/Scene.hpp>
#include <Canis/Yaml.hpp>
#include <cmath>

namespace Canis
{
    namespace
    {
        std::type_index ConstantType(const SignalConstant& value)
        {
            return std::visit([](const auto& v) -> std::type_index
            {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) return typeid(void);
                else return typeid(T);
            }, value);
        }
        bool Compatible(const SignalListener& listener, const SignalAction& action, std::type_index type)
        {
            if (listener.argumentMode == SignalArgumentMode::Configured) return bool(action.invokeConfigured);
            return bool(action.invoke) && (action.argumentType == typeid(void) || action.argumentType ==
                (listener.argumentMode == SignalArgumentMode::Dynamic ? type : ConstantType(listener.constant)));
        }
        const SignalAction* FindAction(const SignalListener& listener, void*& component)
        {
            component = nullptr;
            if (!listener.target || !listener.target.scene.app || listener.script.empty() || listener.method.empty()) return nullptr;
            const auto* conf = listener.target.scene.app->GetScriptConf(listener.script);
            if (!conf || !conf->Get) return nullptr;
            const auto it = conf->signalActions.find(listener.method);
            if (it == conf->signalActions.end() || (!it->second.invoke && !it->second.invokeConfigured)) return nullptr;
            auto target = listener.target;
            component = conf->Get(target);
            return component ? &it->second : nullptr;
        }
    }
    void InvokeSignalAction(ScriptableEntity& script, const std::function<void()>& action)
    {
        script.entity.scene.InvokeScriptAction(script, action);
    }
    bool SignalBase::CanInvoke(const SignalListener& listener, const SignalAction& action) const
    {
        if (!Compatible(listener, action, ArgumentType())) return false;
        if (listener.argumentMode != SignalArgumentMode::Configured) return true;
        if (!listener.target || !action.prepareArguments) return false;
        auto args = YAML::Clone(listener.arguments);
        return action.prepareArguments(listener.target.scene, args);
    }
    bool SignalBase::IsListenerValid(const SignalListener& listener) const
    {
        void* component;
        const auto* action = FindAction(listener, component);
        return action && CanInvoke(listener, *action);
    }
    void SignalBase::EmitPersistent(const std::vector<SignalListener>& listeners, std::type_index type, const void* value)
    {
        for (const auto& listener : listeners)
        {
            if (!listener.enabled) continue;
            void* component;
            const auto* registered = FindAction(listener, component);
            if (!registered || !Compatible(listener, *registered, type)) continue;
            // A callback may change registrations or remove itself.
            const auto action = *registered;
            if (listener.argumentMode == SignalArgumentMode::Configured) {
                auto args = YAML::Clone(listener.arguments);
                if (!action.prepareArguments || !action.prepareArguments(listener.target.scene, args)) continue;
                listener.target.scene.InvokeComponentAction([&] { action.invokeConfigured(component, listener.target.scene, args); });
                continue;
            }
            auto call = [&](const void* argument)
            {
                listener.target.scene.InvokeComponentAction([&] { action.invoke(component, argument); });
            };
            if (action.argumentType == typeid(void)) call(nullptr);
            else if (listener.argumentMode == SignalArgumentMode::Dynamic) call(value);
            else std::visit([&](const auto& constant) { call(&constant); }, listener.constant);
        }
    }
}

namespace Canis
{
    std::vector<SignalListener> SnapshotSignalListeners(const std::vector<SignalListener>& listeners)
    {
        auto snapshot = listeners;
        for (auto& listener : snapshot)
            if (listener.arguments && !listener.arguments.IsNull()) listener.arguments.reset(YAML::Clone(listener.arguments));
        return snapshot;
    }

    SignalBase::SignalBase(const SignalBase& other)
        : listeners(SnapshotSignalListeners(other.listeners)), m_argumentType(other.m_argumentType), m_argumentName(other.m_argumentName) {}
    SignalBase& SignalBase::operator=(const SignalBase& other)
    {
        if (this != &other) {
            listeners = SnapshotSignalListeners(other.listeners);
            m_argumentType = other.m_argumentType;
            m_argumentName = other.m_argumentName;
        }
        return *this;
    }

    YAML::Node EncodeSignal(const SignalBase& signal)
    {
        YAML::Node node(YAML::NodeType::Sequence);
        for (const auto& listener : signal.listeners)
        {
            YAML::Node item;
            item["target"] = listener.target.GetUUID();
            item["script"] = listener.script;
            item["method"] = listener.method;
            item["enabled"] = listener.enabled;
            item["mode"] = listener.argumentMode == SignalArgumentMode::Configured ? "Configured" :
                listener.argumentMode == SignalArgumentMode::Dynamic ? "Dynamic" : "Constant";
            if (listener.argumentMode == SignalArgumentMode::Configured)
                item["arguments"] = listener.arguments && !listener.arguments.IsNull() ? YAML::Clone(listener.arguments) : YAML::Node(YAML::NodeType::Map);
            if (listener.argumentMode == SignalArgumentMode::Constant)
                std::visit([&](const auto& value)
                {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (!std::is_same_v<T, std::monostate>)
                    {
                        item["constant"]["type"] = SignalArgumentName<T>();
                        item["constant"]["value"] = value;
                    }
                }, listener.constant);
            node.push_back(item);
        }
        return node;
    }

    bool DecodeSignal(const YAML::Node& node, Scene& scene, SignalBase& signal)
    {
        if (!node.IsSequence()) return false;
        std::vector<SignalListener> decoded;
        std::vector<UUID> targets;
        try
        {
            for (const auto& item : node)
            {
                if (!item.IsMap()) return false;
                SignalListener listener;
                targets.push_back(item["target"].as<UUID>(UUID(0)));
                listener.script = item["script"].as<std::string>("");
                listener.method = item["method"].as<std::string>("");
                listener.enabled = item["enabled"].as<bool>(true);
                const auto mode = item["mode"].as<std::string>("Dynamic");
                if (mode != "Dynamic" && mode != "Constant" && mode != "Configured") return false;
                listener.argumentMode = mode == "Configured" ? SignalArgumentMode::Configured :
                    mode == "Dynamic" ? SignalArgumentMode::Dynamic : SignalArgumentMode::Constant;
                if (listener.argumentMode == SignalArgumentMode::Configured) {
                    const auto args = item["arguments"];
                    if (args && !args.IsNull() && !args.IsMap()) return false;
                    listener.arguments = args ? YAML::Clone(args) : YAML::Node(YAML::NodeType::Map);
                }
                if (listener.argumentMode == SignalArgumentMode::Constant)
                {
                    const auto constant = item["constant"];
                    if (constant)
                    {
                        if (!constant.IsMap()) return false;
                        const auto type = constant["type"].as<std::string>("");
                        if (type == "float")
                        {
                            const float value = constant["value"].as<float>();
                            if (!std::isfinite(value)) return false;
                            listener.constant = value;
                        }
                        else if (type == "int") listener.constant = constant["value"].as<int>();
                        else if (type == "bool") listener.constant = constant["value"].as<bool>();
                        else if (type == "string") listener.constant = constant["value"].as<std::string>();
                        else return false;
                    }
                }
                decoded.push_back(std::move(listener));
            }
        }
        catch (const YAML::Exception&) { return false; }
        for (size_t i = 0; i < decoded.size(); ++i) {
            auto& listener = decoded[i];
            scene.GetEntityAfterLoad(targets[i], listener.target);
            // Action metadata is available even when its target's components
            // have not decoded yet. Remap nested callback targets now, while
            // the prefab source-to-instance UUID table is still available.
            if (listener.argumentMode == SignalArgumentMode::Configured && scene.app)
                if (auto* conf = scene.app->GetScriptConf(listener.script))
                    if (auto action = conf->signalActions.find(listener.method); action != conf->signalActions.end() && action->second.prepareArguments)
                        if (!action->second.prepareArguments(scene, listener.arguments)) return false;
        }
        signal.listeners = std::move(decoded);
        return true;
    }
}
