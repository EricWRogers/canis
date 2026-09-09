#include <Canis/Components.hpp>
#include <Canis/Scene.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Yaml.hpp>
#include <Canis/App.hpp>
#include <cstdlib>
#include <iostream>
using namespace Canis;
static void Check(bool condition, const char* message) { if (!condition) { std::cerr << message << '\n'; std::exit(1); } }
struct One : ScriptableEntity {
    static inline int destroyed=0, destructed=0;
    explicit One(Entity& owner) : ScriptableEntity(owner) {}
    ~One() override { ++destructed; }
    void Destroy() override { ++destroyed; }
};
struct Two : ScriptableEntity { using ScriptableEntity::ScriptableEntity; };
struct RemoveSelf : ScriptableEntity {
    static inline int updated=0, destroyed=0;
    using ScriptableEntity::ScriptableEntity;
    void Update(float) override { entity.RemoveScript<RemoveSelf>(); ++updated; }
    void Destroy() override { ++destroyed; }
};
struct RemoveInCreate : ScriptableEntity {
    static inline int finished=0, destructed=0;
    using ScriptableEntity::ScriptableEntity;
    void Create() override { entity.RemoveScript<RemoveInCreate>(); ++finished; }
    ~RemoveInCreate() override { ++destructed; }
};
struct DestroyInCreate : ScriptableEntity {
    static inline int finished=0;
    using ScriptableEntity::ScriptableEntity;
    void Create() override { entity.Destroy(); ++finished; }
};
struct PlainData { int value=0; };
struct DestroyOwner : ScriptableEntity {
    static inline bool childInvalid=false;
    Entity child;
    using ScriptableEntity::ScriptableEntity;
    void Update(float) override { entity.Destroy(); childInvalid=!child; }
};
struct TargetProperty { Entity* entity; Entity target; };
struct ReferenceList { std::vector<Entity> targets; };
static void LoadReferenceTests() {
    App app; app.scene.app=&app;
    ScriptConf conf; conf.name="References";
    conf.Decode=[](YAML::Node& node, Entity& entity, bool) {
        if (!node["Targets"]) return;
        auto& list=*entity.AddComponent<ReferenceList>();
        for (const auto& target : node["Targets"]) {
            list.targets.emplace_back();
            entity.scene.GetEntityAfterLoad(target.as<UUID>(),list.targets.back());
        }
        list.targets.reserve(2048); // Pending destinations move before reference resolution.
    };
    app.RegisterScript(conf);
    auto nodes=YAML::Load("- {Entity: 101, Name: Owner, Targets: [102, 103, 999]}\n- {Entity: 102, Name: First}\n- {Entity: 103, Name: Second}\n");
    auto loaded=app.scene.LoadEntityNodes(nodes);
    auto references=loaded.front()->GetComponent<ReferenceList>().targets;
    Check(references[0] && references[1] && !references[2] && references[2].GetUUID()==UUID(999),"Moved load fixups failed or missing UUID lost");
    const auto originals=references;
    loaded=app.scene.LoadEntityNodes(nodes,false);
    auto& duplicate=loaded.front()->GetComponent<ReferenceList>().targets;
    Check(duplicate[0] && duplicate[0].GetUUID()!=UUID(102) && duplicate[0]!=originals[0],"Prefab duplication did not remap internal reference");
    app.scene.Unload();
    Check(!references[0] && !originals[0],"Reload left live old handles");
    loaded=app.scene.LoadEntityNodes(nodes);
    Check(!references[0] && loaded.front()->GetComponent<ReferenceList>().targets[0],"Same UUID restored a stale runtime handle");
}
struct WrapperLayout { entt::entity handle; Scene& scene; };
static_assert(sizeof(Entity)==sizeof(WrapperLayout));
static_assert(std::is_copy_constructible_v<Entity>);
struct OwnerData { Entity* entity=nullptr; };
static void EntityWrapperTests() {
    Scene scene;
    Scene secondScene;
    Entity empty;
    Check(!empty && empty.GetHandle()==entt::null, "Default entity is not empty");
    auto elsewhere=secondScene.CreateEntity("Elsewhere");
    empty=elsewhere;
    Check(empty && &empty.scene==&secondScene, "Assignment did not bind the reference scene");
    auto original=scene.CreateEntity("Wrapped");
    empty=original;
    Check(empty==original && &empty.scene==&scene, "Cross-scene value assignment failed");
    empty=nullptr;
    Check(!empty && empty.GetUUID()==UUID(0), "Clearing a value retained its identity");
    Entity copy=*original;
    Entity saved;
    {
        Entity temporary=copy;
        temporary.SetName("Shared metadata");
        saved=temporary;
        temporary.AddComponent<OwnerData>();
        auto* script=temporary.AddScript<Two>();
        Check(&script->entity==original.TryGet(), "Script retained a temporary wrapper reference");
    }
    Check(original && saved && original->GetName()=="Shared metadata", "Destroying wrapper copy changed entity lifetime");
    Check(copy.GetComponent<OwnerData>().entity==original.TryGet(), "Component retained a temporary wrapper pointer");
    Check(scene.GetEntityIndex(copy)==scene.GetEntityIndex(*original), "Copied wrapper has no scene ordering index");
    copy.Destroy();
    Check(!copy && !original && !saved, "Wrapper destruction request did not invalidate identity");
    auto replacement=scene.CreateEntity("Replacement");
    Check(!copy && replacement, "EnTT slot reuse revived a stale wrapper");
    Entity beforeReload=*replacement;
    scene.Unload();
    auto afterReload=scene.CreateEntity("After reload");
    Check(!beforeReload && afterReload, "Scene reload reset generations and revived a stale wrapper");
    Check(!beforeReload.TryGetComponent<EntityMetadata>(), "Stale wrapper returned replacement metadata");
}
template<class T> concept HasOrderingId = requires(T& entity) { entity.id; };
static_assert(!HasOrderingId<Entity>);
static void SceneIndexTests() {
    Scene scene, other;
    auto first=scene.CreateEntity("First");
    auto second=scene.CreateEntity("Second");
    auto foreign=other.CreateEntity("Foreign");
    const int firstIndex=scene.GetEntityIndex(*first), secondIndex=scene.GetEntityIndex(*second);
    Check(firstIndex>=0 && secondIndex>=0 && firstIndex!=secondIndex && scene.GetEntity(firstIndex)==first,
          "Scene index does not map back to entity");
    Check(scene.GetEntityIndex(*foreign)==-1, "Foreign scene entity resolved to a local ordering slot");
    scene.Destroy(*foreign);
    Check(foreign && first, "Destroying a foreign entity affected a local slot");
    first.Destroy();
    Check(scene.GetEntity(firstIndex)==nullptr && scene.GetEntityIndex(*second)==secondIndex,
          "Deletion corrupted scene ordering");
    auto replacement=scene.CreateEntity("Replacement");
    Check(scene.GetEntityIndex(*replacement)==firstIndex && !first, "Reused ordering slot revived stale identity");
    scene.Unload();
    auto reloaded=scene.CreateEntity("Reloaded");
    Check(!replacement && !second && scene.GetEntityIndex(*reloaded)==0 && scene.GetEntity(0)==reloaded,
          "Unload did not reset scene ordering");
}
static void NamedScriptComponentTests() {
    App app; app.scene.app=&app;
    ScriptConf conf; conf.name="Two";
    conf.Get=[](Entity& entity) -> void* { return entity.GetScript<Two>(); };
    app.RegisterScript(conf);
    auto owner=app.scene.CreateEntity("Named script owner");
    auto* attached=owner->AttachScript("Two",new Two(*owner));
    Check(attached && owner->HasComponent<ScriptComponent>(), "Named attachment did not create script storage");
    Check(owner->AttachScript("Two",new Two(*owner))==attached && owner->GetScripts().size()==1,
          "Named attachment duplicated a script");
    ScriptHandle<Two> saved(static_cast<Two*>(attached));
    owner->RemoveScript("Two");
    Check(!saved && !owner->HasComponent<ScriptComponent>(), "Named removal left the last script component");
    owner->AddScript<RemoveSelf>();
    app.scene.Update(0.01f);
    Check(!owner->HasComponent<ScriptComponent>() && owner->GetScripts().empty(),
          "Last script self-removal during Update left a component");
}
static void NumericTagTests() {
    static_assert(std::is_same_v<decltype(EntityMetadata::tag), TagId>);
    static_assert(TagIdFromName("") == NoTag);
    static_assert(TagIdFromName("Player") == 0x333dc56ddffd8ea0ull);
    App app; app.scene.app=&app;
    auto first=app.scene.CreateEntity("First", "Player");
    auto second=app.scene.CreateEntity("Second", "Player");
    auto untagged=app.scene.CreateEntity("Untagged");
    const TagId player=TagIdFromName("Player");
    Check(first->GetTag()==player && first->HasTag(player), "Tags are not stored as numeric IDs");
    Check(app.scene.GetEntityWithTag(player)==first && app.scene.GetEntityWithTag("Player")==first,
          "Numeric/name tag queries disagree");
    Check(app.scene.GetEntitiesWithTag(player).size()==2 && untagged->GetTag()==NoTag, "Multiple/empty tag lookup failed");
    first->SetTag("UnlistedLegacyTag");
    auto encoded=app.scene.EncodeEntity(*first);
    Check(encoded["Tag"].as<std::string>()=="UnlistedLegacyTag", "Tag name lost in serialization");
    YAML::Node nodes(YAML::NodeType::Sequence); nodes.push_back(encoded);
    app.scene.Unload();
    auto restored=app.scene.LoadEntityNodes(nodes);
    Check(restored.front()->GetTag()==TagIdFromName("UnlistedLegacyTag") && restored.front()->GetTagName()=="UnlistedLegacyTag",
          "Unlisted tag did not round-trip");
    restored.front()->SetTag("");
    Check(restored.front()->GetTag()==NoTag && restored.front()->GetTagName().empty(), "Clearing numeric tag failed");
}
int main() {
    EntityWrapperTests();
    SceneIndexTests();
    NumericTagTests();
    LoadReferenceTests();
    Entity outside;
    {
        Scene scene;
        auto target =scene.CreateEntity("A"); target->AddComponent<Transform>();
        auto owner =scene.CreateEntity("B");
        TargetProperty property{owner,target};
        Check(!owner->HasComponent<ScriptComponent>() && owner->GetScripts().empty() && !owner->GetScript<One>(),
              "Empty entity has a script component or querying scripts created one");
        owner->RemoveScript<One>(); owner->RemoveAllScripts();
        Check(!owner->HasComponent<ScriptComponent>(), "Removing absent scripts created a component");
        auto temporary=owner->AddScriptHandle<Two>();
        Check(owner->HasComponent<ScriptComponent>(), "First script did not create ScriptComponent");
        owner->RemoveScript(temporary);
        Check(!temporary && !owner->HasComponent<ScriptComponent>(), "Last script did not remove ScriptComponent");
        auto* ownerRecord = owner.TryGet();
        auto* metadata = owner.TryGetComponent<EntityMetadata>();
        for (int i=0; i<128; ++i) { auto transient=scene.CreateEntity("Transient"); transient.Destroy(); }
        Check(owner.TryGet()==ownerRecord && owner.TryGetComponent<EntityMetadata>()==metadata && &ownerRecord->GetName()==&metadata->name,
              "Registry relocation invalidated entity record or metadata");
        Check(!owner->AddScriptHandle<RemoveInCreate>() && RemoveInCreate::finished==1 && RemoveInCreate::destructed==1,
              "Create self-removal did not complete safely");
        auto selfDestroy=scene.CreateEntity("Destroy in Create");
        Check(!selfDestroy->AddScriptHandle<DestroyInCreate>() && !selfDestroy && DestroyInCreate::finished==1,
              "Create owner deletion did not complete safely");
        scene.Update(0.01f);
        bool missingThrew=false;
        try { owner->GetComponent<PlainData>(); } catch (const std::runtime_error&) { missingThrew=true; }
        Check(missingThrew && !owner->HasComponent<PlainData>(), "Missing get implicitly added component");
        owner.GetOrAddComponent<PlainData>().value=7;
        Check(owner.GetComponent<PlainData>().value==7,"Plain component or explicit creation failed");
        const auto uuid=target->GetUUID();
        outside=target;
        Check(property.target.TryGetComponent<Transform>()!=nullptr,"Live component lookup failed");
        target->Destroy();
        Check(!outside && !property.target && property.target.GetUUID()==uuid,"Deletion lost identity or left live handle");
        Check(property.target.TryGetComponent<Transform>()==nullptr,"Deleted component lookup not neutral");
        Check(GetRegisteredProperty(property,property.target).as<UUID>()==uuid,"Missing target UUID lost when saved");
#if CANIS_EDITOR
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename=nullptr; ImGui::GetIO().DisplaySize=ImVec2(800,600);
        unsigned char* pixels=nullptr; int width=0,height=0;
        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
        {
            Editor inspector;
            ImGui::NewFrame(); ImGui::Begin("Deleted reference regression");
            inspector.InputEntity("Target",property.target); // Exact inspector dereference path.
            ImGui::End(); ImGui::Render();
        }
        ImGui::DestroyContext();
#endif
        auto replacement =scene.CreateEntity("C");
        Check(!outside && property.target.TryGet()!=replacement,"Slot/address reuse revived old reference");
        property.target=replacement;
        Check(property.target && outside!=property.target,"Replacement did not receive new identity");
        property.target.Reset(); Check(property.target.GetUUID()==UUID(0),"Clear retained missing UUID");
        Check(!owner->HasComponent<ScriptComponent>(), "Create self-removal left an empty ScriptComponent");
        auto first=owner->AddScriptHandle<One>(); auto second=owner->AddScriptHandle<Two>();
        Check(first && second && owner->GetScripts().size()==2,"Multiple scripts were not retained");
        owner->RemoveScript(first);
        Check(owner->HasComponent<ScriptComponent>() && owner->GetScripts().size()==1, "Removing one of two scripts removed their component");
        Check(!first && second && One::destroyed==1 && One::destructed==1,"Script removal invalidated wrong instance or skipped virtual destructor");
        auto renewed=owner->AddScriptHandle<One>(); Check(renewed && !first,"Reattachment revived stale script handle");
        owner->AddScript<RemoveSelf>(); scene.Update(0.01f);
        Check(RemoveSelf::updated==1 && RemoveSelf::destroyed==1,"Self removal did not complete safely");
        scene.Update(0.01f); Check(RemoveSelf::updated==1,"Removed script updated again");
        auto parent=scene.CreateEntity("Parent"); auto child=scene.CreateEntity("Child");
        parent.AddComponent<Transform>(); child.AddComponent<Transform>()->SetParent(parent);
        bool cycleRejected=false;
        try { parent.GetComponent<Transform>().SetParent(child); } catch(const std::runtime_error&) { cycleRejected=true; }
        Check(cycleRejected,"Hierarchy cycle accepted");
        auto destroy=parent->AddScript<DestroyOwner>(); destroy->child=child;
        scene.Update(0.01f);
        Check(DestroyOwner::childInvalid && !child && !parent,"Deferred subtree deletion left live child handle");
        auto emptyAgain=scene.CreateEntity("Remove all");
        emptyAgain->AddScript<Two>(); emptyAgain->AddScript<RemoveSelf>();
        emptyAgain->RemoveAllScripts();
        Check(!emptyAgain->HasComponent<ScriptComponent>() && emptyAgain->GetScripts().empty(), "RemoveAllScripts left its component");
        emptyAgain->AddScript<Two>();
        Check(emptyAgain->HasComponent<ScriptComponent>(), "Adding after removal failed to recreate the component");
        outside=owner;
        scene.Unload(); Check(!outside && !renewed && !second,"Unload retained live handles");
        outside=scene.CreateEntity("New scene entity");
        outside.Reset(); // Scene-scoped references must be cleared before their Scene dies.
    }
    Check(!outside,"Reset did not clear scene-scoped reference");
    Check(One::destroyed==2 && One::destructed==2,"Script teardown not exactly once");
    NamedScriptComponentTests();
    std::cout << "Entity lifetime and multiple-script tests passed\n";
}
