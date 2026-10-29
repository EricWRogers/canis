#include "EditorExportTests.hpp"
#include <Canis/ConfigHelper.hpp>
#include <iostream>
#include <stdexcept>
using namespace Canis;
void Check(bool v, const char *error)
{
    if (!v)
        throw std::runtime_error(error);
}
SignalListener Action(Entity entity, const char *method)
{
    SignalListener l;
    l.target = entity;
    l.script = PrivateFixture::ScriptName;
    l.method = method;
    return l;
}
int main()
{
    try
    {
        App app;
        app.scene.app = &app;
        RegisterPrivateFixtureScript(app);
        auto owner = app.scene.CreateEntity("Private source"), sink = app.scene.CreateEntity("Private sink");
        auto *value = owner.AddScript<PrivateFixture>();
        auto *target = sink.AddScript<PrivateFixture>();
        auto *conf = app.GetScriptConf(PrivateFixture::ScriptName);
        Check(conf->registry.getters.size() == 3 && !conf->registry.getters.contains("unmarkedPublic"),
              "Public unmarked member was exposed");
        auto amount = YAML::Node(2.5f);
        conf->registry.setters.at("value")(amount, value);
        Check(value->Read() == 2.5f, "Private field could not be edited");
        Signal<> callback;
        callback.listeners.push_back(Action(sink, "Set"));
        auto bindings = EncodeSignal(callback);
        conf->registry.setters.at("changed")(bindings, value);
        Signal<> set;
        auto l = Action(owner, "Set (amount)");
        l.argumentMode = SignalArgumentMode::Constant;
        l.constant = 4.5f;
        set.listeners.push_back(l);
        set.Emit();
        Check(value->Read() == 4.5f && target->Read() == 7,
              "Private overloaded action or private Signal callback failed");
        Signal<> configure;
        l = Action(owner, "Configure");
        l.argumentMode = SignalArgumentMode::Configured;
        l.arguments = YAML::Load("{amount: 8.5, repeat: true}");
        configure.listeners.push_back(l);
        configure.Emit();
        Check(value->Read() == 8.5f && value->Looping(), "Private multiple-argument action failed");
        YAML::Node nodes(YAML::NodeType::Sequence);
        nodes.push_back(app.scene.EncodeEntity(owner));
        nodes.push_back(app.scene.EncodeEntity(sink));
        Check(!nodes[0][PrivateFixture::ScriptName]["unmarkedPublic"], "Unmarked field was serialized");
        auto copy = app.scene.LoadEntityNodes(nodes, false);
        auto *copied = copy[0]->GetScript<PrivateFixture>();
        Check(copied->Read() == 8.5f, "Private field did not survive a prefab copy");
        auto signalNode = conf->registry.getters.at("changed")(copied);
        Check(signalNode[0]["target"].as<UUID>() == copy[1]->GetUUID(),
              "Private Signal escaped prefab remapping");
        std::cout << "Private exports: editor setters, overloads, callbacks, configured arguments and prefab "
                     "serialization passed\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}


// CANIS EDITOR EXPORTS BEGIN PrivateFixture
template<> void Canis::EditorExports<PrivateFixture>::Register(Canis::ScriptConf& config)
{
    REGISTER_PROPERTY(config, PrivateFixture, value);
    REGISTER_PROPERTY(config, PrivateFixture, loop);
    REGISTER_PROPERTY(config, PrivateFixture, changed);
    RegisterSignalAction(config, "Set", static_cast<void(PrivateFixture::*)()>(&PrivateFixture::Set));
    RegisterSignalAction(config, "Set (amount)", static_cast<void(PrivateFixture::*)(float)>(&PrivateFixture::Set));
    RegisterSignalAction(config, "Configure", static_cast<void(PrivateFixture::*)(float, bool)>(&PrivateFixture::Configure), {"amount", "repeat"});
}
// CANIS EDITOR EXPORTS END PrivateFixture
namespace
{
Canis::ScriptConf config;
}
void RegisterPrivateFixtureScript(Canis::App &app)
{
    config = {};
    Canis::EditorExports<PrivateFixture>::Register(config);
    DEFAULT_CONFIG(config, PrivateFixture);
    config.DEFAULT_DRAW_INSPECTOR(PrivateFixture);
    app.RegisterScript(config);
}
