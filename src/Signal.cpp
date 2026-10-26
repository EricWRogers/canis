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
            return action.argumentType == typeid(void) || action.argumentType ==
                (listener.argumentMode == SignalArgumentMode::Dynamic ? type : ConstantType(listener.constant));
        }
        const SignalAction* FindAction(const SignalListener& listener, ScriptableEntity*& script)
        {
            script = nullptr;
            if (!listener.target || !listener.target.scene.app || listener.script.empty() || listener.method.empty()) return nullptr;
            const auto* conf = listener.target.scene.app->GetScriptConf(listener.script);
            if (!conf || conf->kind != RegistryEntryKind::Script || !conf->Get) return nullptr;
            const auto it = conf->signalActions.find(listener.method);
            if (it == conf->signalActions.end() || !it->second.invoke) return nullptr;
            auto target = listener.target;
            script = static_cast<ScriptableEntity*>(conf->Get(target));
            return script ? &it->second : nullptr;
        }
    }
    void InvokeSignalAction(ScriptableEntity& script, const std::function<void()>& action)
    {
        script.entity.scene.InvokeScriptAction(script, action);
    }
    bool SignalBase::CanInvoke(const SignalListener& listener, const SignalAction& action) const
    {
        return Compatible(listener, action, ArgumentType());
    }
    bool SignalBase::IsListenerValid(const SignalListener& listener) const
    {
        ScriptableEntity* script;
        const auto* action = FindAction(listener, script);
        return action && CanInvoke(listener, *action);
    }
    void SignalBase::EmitPersistent(const std::vector<SignalListener>& listeners, std::type_index type, const void* value)
    {
        for (const auto& listener : listeners)
        {
            if (!listener.enabled) continue;
            ScriptableEntity* script;
            const auto* registered = FindAction(listener, script);
            if (!registered || !Compatible(listener, *registered, type)) continue;
            // A callback may change registrations or remove itself.
            const auto action = *registered;
            auto call = [&](const void* argument)
            {
                listener.target.scene.InvokeScriptAction(*script, [&] { action.invoke(*script, argument); });
            };
            if (action.argumentType == typeid(void)) call(nullptr);
            else if (listener.argumentMode == SignalArgumentMode::Dynamic) call(value);
            else std::visit([&](const auto& constant) { call(&constant); }, listener.constant);
        }
    }
}

namespace Canis
{
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
            item["mode"] = listener.argumentMode == SignalArgumentMode::Dynamic ? "Dynamic" : "Constant";
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
                if (mode != "Dynamic" && mode != "Constant") return false;
                listener.argumentMode = mode == "Dynamic" ? SignalArgumentMode::Dynamic : SignalArgumentMode::Constant;
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
        for (size_t i = 0; i < decoded.size(); ++i)
            scene.GetEntityAfterLoad(targets[i], decoded[i].target);
        signal.listeners = std::move(decoded);
        return true;
    }
}
