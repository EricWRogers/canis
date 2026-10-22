#include <Canis/Asset.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/App.hpp>
#include <Canis/Canis.hpp>
#include <Canis/CombineModel.hpp>
#include <Canis/CombineModelLoad.hpp>
#include <Canis/Components.hpp>
#include <Canis/OpenGL.hpp>
#include <Canis/Window.hpp>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

using namespace Canis;
namespace fs = std::filesystem;
namespace
{
    void Check(bool value, const std::string& message)
    { if (!value) throw std::runtime_error(message); }

    std::string Read(const fs::path& path)
    { std::ifstream file(path, std::ios::binary); return {std::istreambuf_iterator<char>(file), {}}; }

    void Write(const fs::path& path, const std::string& bytes)
    { std::ofstream file(path, std::ios::binary); file.write(bytes.data(), bytes.size()); }

    struct Fixture
    {
        fs::path root, model, cache;
        YAML::Node entities;
        explicit Fixture(const std::string& label)
        {
            root = fs::temp_directory_path() / ("canis-combine-" + label + "-" + std::to_string(SDL_GetTicksNS()));
            fs::create_directories(root);
            model = root / "source.gltf";
            cache = root / "combined.cache";
            // A slanted, textured triangle exercises normal matrices and mirrored winding.
            const float positions[] = {0,0,0, 1,0,0, 0,1,1};
            const float normals[] = {0,-.707106781f,.707106781f, 0,-.707106781f,.707106781f, 0,-.707106781f,.707106781f};
            const float uv[] = {0,0, 1,0, 0,1};
            const float colors[] = {.3f,.5f,.7f,1, .4f,.6f,.8f,1, .5f,.7f,.9f,1};
            const uint16_t indices[] = {0,1,2};
            std::ofstream binary(root / "source.bin", std::ios::binary);
            for (const auto& block : {std::pair<const void*,size_t>{positions,sizeof(positions)}, {normals,sizeof(normals)},
                                     {uv,sizeof(uv)}, {colors,sizeof(colors)}, {indices,sizeof(indices)}})
                binary.write(static_cast<const char*>(block.first), block.second);
            binary.close();
            const unsigned char png[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,19,73,68,65,84,120,156,99,248,207,192,240,31,12,25,24,254,131,1,0,73,200,9,247,20,99,50,157,0,0,0,0,73,69,78,68,174,66,96,130};
            for (const char* name : {"albedo.png", "normal.png", "emission.png"})
            {
                std::ofstream image(root / name, std::ios::binary);
                image.write(reinterpret_cast<const char*>(png), sizeof(png));
            }
            Write(model, R"({
              "asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],
              "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2,"COLOR_0":3},"indices":4,"material":0}]}],
              "buffers":[{"uri":"source.bin","byteLength":150}],
              "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},
                {"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":48},{"buffer":0,"byteOffset":144,"byteLength":6}],
              "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,1]},
                {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
                {"bufferView":3,"componentType":5126,"count":3,"type":"VEC4"},{"bufferView":4,"componentType":5123,"count":3,"type":"SCALAR"}],
              "materials":[{"pbrMetallicRoughness":{"baseColorFactor":[0.8,0.6,0.4,1],"baseColorTexture":{"index":0}},
                "normalTexture":{"index":1},"emissiveTexture":{"index":2},"emissiveFactor":[0.1,0.2,0.3]}],
              "textures":[{"source":0},{"source":1},{"source":2}],
              "images":[{"uri":"albedo.png"},{"uri":"normal.png"},{"uri":"emission.png"}]
            })");
            entities = YAML::Node(YAML::NodeType::Sequence);
        }
        ~Fixture() { std::error_code ec; fs::remove_all(root, ec); }

        YAML::Node Add(uint64_t id, uint64_t parent, bool rendered = false)
        {
            YAML::Node entity;
            entity["Entity"] = id;
            entity["Name"] = "Fixture " + std::to_string(id);
            entity["Active"] = true;
            auto transform = entity["Canis::Transform"];
            transform["active"] = true;
            transform["position"] = Vector3(0);
            transform["rotation"] = Vector3(0);
            transform["scale"] = Vector3(1);
            transform["parent"] = parent;
            transform["children"] = YAML::Node(YAML::NodeType::Sequence);
            if (rendered)
            {
                auto render = entity["Canis::Model"];
                render["ModelAsset"]["path"] = model.string();
                render["static"] = true;
                render["nodeIndex"] = -1;
                render["applyNodeTransform"] = true;
                render["color"] = Color(1);
            }
            entities.push_back(entity);
            for (auto possibleParent : entities)
                if (possibleParent["Entity"].as<uint64_t>() == parent)
                    possibleParent["Canis::Transform"]["children"].push_back(id);
            return entity;
        }

        void Build(float cell = 0)
        {
            std::string error;
            const bool built = BuildCombineModel(entities, 101, cache.string(), cell, error);
            Check(built, "Cache build failed: " + error);
        }

        YAML::Node Load(std::unordered_set<uint64_t>& removed, CombineModelSummary& summary, float cell = 0)
        {
            YAML::Node replacements;
            std::string error;
            const bool loaded = LoadCombineModelReplacement(entities, 101, cache.string(), cell,
                  replacements, removed, summary, error);
            Check(loaded, "Cache load failed: " + error);
            return replacements;
        }
    };

    fs::path GeometryPath(const YAML::Node& entity)
    {
        Check(bool(entity["Canis::Model"]), "Replacement omitted its Model component");
        auto handle = entity["Canis::Model"]["ModelAsset"];
        Check(handle && handle["path"], "Compiled model reference must have a persistent path");
        return handle["path"].as<std::string>();
    }

    void TextureReferences(const ModelAsset::PrimitiveBuild3D& primitive, const fs::path& root)
    {
        Check(primitive.textureId >= 0 && fs::path(AssetManager::GetPath(primitive.textureId)) == root / "albedo.png",
              "Compiled albedo lost its persistent texture path");
        Check(primitive.normalTextureId >= 0 && fs::path(AssetManager::GetPath(primitive.normalTextureId)) == root / "normal.png",
              "Compiled normal map lost its persistent texture path");
        Check(primitive.emissionTextureId >= 0 && fs::path(AssetManager::GetPath(primitive.emissionTextureId)) == root / "emission.png",
              "Compiled emission map lost its persistent texture path");
        Check(glm::distance(primitive.emissionFactor, Vector3(.1f,.2f,.3f)) < .00001f,
              "Compiled model lost its imported emission factor");
    }

    void GeometryAndFreshProcess(const fs::path& executable)
    {
        Fixture fixture("geometry");
        auto root = fixture.Add(101, 0);
        root["Canis::Transform"]["position"] = Vector3(10,2,-3);
        root["Canis::Transform"]["rotation"] = Vector3(.1f,.3f,.2f);
        root["Canis::Transform"]["scale"] = Vector3(2,3,4);
        auto group = fixture.Add(102, 101);
        group["Canis::Transform"]["position"] = Vector3(4,1,2);
        group["Canis::Transform"]["rotation"] = Vector3(0,0,.4f);
        group["Canis::Transform"]["scale"] = Vector3(2,3,4);
        auto child = fixture.Add(103, 102, true);
        child["Canis::Transform"]["position"] = Vector3(1,2,3);
        child["Canis::Transform"]["rotation"] = Vector3(0,.5f,0);
        child["Canis::Transform"]["scale"] = Vector3(-2,.5f,1);
        const Color tint(.7f,.8f,.9f,1);
        child["Canis::Model"]["color"] = tint;

        auto* source = AssetManager::GetModel(AssetManager::LoadModel(fixture.model.string()));
        std::vector<ModelAsset::PrimitiveBuild3D> original;
        Check(source && source->ExportStaticPrimitives(original) && original.size() == 1, "Source export failed");
        fixture.Build();
        std::unordered_set<uint64_t> removed;
        CombineModelSummary summary;
        const auto replacements = fixture.Load(removed, summary);
        Check(removed == std::unordered_set<uint64_t>({102,103}), "Pure source hierarchy was not fully replaced");
        Check(summary.sourceModels == 1 && summary.combinedModels == 1 && summary.triangleCount == 1,
              "Cache summary did not describe its actual model/triangle counts");
        Check(replacements.IsSequence() && replacements.size() == 1, "Expected one compiled replacement");
        Check(replacements[0]["Canis::Transform"]["parent"].as<uint64_t>() == 101,
              "Replacement was not parented to the authored root UUID");
        Check(!removed.contains(replacements[0]["Entity"].as<uint64_t>()), "Replacement reused a removed source UUID");
        const fs::path cacheModel = GeometryPath(replacements[0]);
        Check(fs::exists(cacheModel), "Compiled model sidecar missing");
        std::vector<ModelAsset::PrimitiveBuild3D> compiled;
        std::vector<std::string> slots;
        Check(ReadCombinedModelGeometry(cacheModel.string(), compiled, slots) && compiled.size() == 1,
              "Compiled geometry could not be decoded");
        const auto matrix = glm::translate(Matrix4(1),Vector3(4,1,2)) * glm::mat4_cast(Quaternion(Vector3(0,0,.4f))) *
            glm::scale(Matrix4(1),Vector3(2,3,4)) * glm::translate(Matrix4(1),Vector3(1,2,3)) *
            glm::mat4_cast(Quaternion(Vector3(0,.5f,0))) * glm::scale(Matrix4(1),Vector3(-2,.5f,1));
        const auto normalMatrix = glm::transpose(glm::inverse(glm::mat3(matrix)));
        Check(compiled[0].vertices.size() == original[0].vertices.size(), "Compiled topology changed its vertex count");
        const Color replacementTint = replacements[0]["Canis::Model"]["color"].as<Color>(Color(1));
        for (size_t i = 0; i < original[0].vertices.size(); ++i)
        {
            const auto& expected = original[0].vertices[i];
            const auto& actual = compiled[0].vertices[i];
            Check(glm::distance(actual.position, Vector3(matrix * Vector4(expected.position,1))) < .0001f,
                  "Compiled positions lost a parent transform or included the root transform twice");
            Check(glm::distance(actual.normal, glm::normalize(normalMatrix * expected.normal)) < .0001f,
                  "Compiled normal did not use the inverse transpose under nonuniform mirror scaling");
            Check(glm::distance(actual.uv, expected.uv) < .00001f, "Compiled model changed imported UV orientation");
            Check(glm::distance(actual.color * compiled[0].baseColor * replacementTint,
                                expected.color * original[0].baseColor * tint) < .0001f,
                  "Compiled model lost or multiplied its tint/vertex colors twice");
        }
        const auto& p = compiled[0];
        Check(p.indices.size() == 3 && glm::dot(glm::cross(p.vertices[p.indices[1]].position - p.vertices[p.indices[0]].position,
                  p.vertices[p.indices[2]].position - p.vertices[p.indices[0]].position), p.vertices[p.indices[0]].normal) > 0,
              "Mirrored combined model has inverted front-face winding");
        TextureReferences(p, fixture.root);
        Check(slots.size() == static_cast<size_t>(source->GetMaterialSlotCount()),
              "Compiled model changed the imported material slot count");
        for (size_t i = 0; i < slots.size(); ++i)
            Check(slots[i] == source->GetMaterialSlotName(static_cast<int>(i)), "Compiled model changed a material slot name");
        ModelAsset loaded;
        Check(loaded.Load(cacheModel.string()), "Compiled model did not load through the normal ModelAsset codec");
        Vector3 low, high;
        Check(loaded.GetLocalBounds(low, high) && glm::distance(low, high) > .1f, "Compiled model bounds were not rebuilt");
        loaded.Free();
        Write(fixture.root / "entities.yaml", YAML::Dump(fixture.entities));

        // Run the actual cache loader with a fresh asset library whose numeric IDs differ.
        const std::string executableArg = executable.string(), rootArg = fixture.root.string();
        const char* args[] = {executableArg.c_str(), "--fresh-load", rootArg.c_str(), nullptr};
        SDL_Process* process = SDL_CreateProcess(args, false);
        Check(process != nullptr, "Could not start fresh-process cache check");
        int status = 1;
        const bool waited = SDL_WaitProcess(process, true, &status);
        SDL_DestroyProcess(process);
        Check(waited && status == 0, "Fresh-process geometry/texture cache load failed");
        std::cout << "Combined hierarchy, mirrored geometry, colors, material maps, and fresh-process texture rebinding passed\n";
    }

    void SafeSelection()
    {
        Fixture fixture("selection");
        fixture.Add(101,0);
        fixture.Add(102,101,true);
        auto script = fixture.Add(103,101,true);
        script["Game::UnknownBehavior"]["value"] = 7;
        fixture.Add(104,103,true); // A script may move or change any descendant.
        auto physics = fixture.Add(105,101,true);
        physics["Canis::BoxCollider"]["active"] = true;
        auto dynamic = fixture.Add(106,101,true);
        dynamic["Canis::Model"]["static"] = false;
        auto inactive = fixture.Add(107,101,true);
        inactive["Active"] = false;
        fixture.Add(108,101,true);
        auto observer = fixture.Add(109,0);
        observer["Game::References"]["target"] = uint64_t(108);
        const auto original = YAML::Dump(fixture.entities);
        fixture.Build();
        std::unordered_set<uint64_t> removed;
        CombineModelSummary summary;
        auto replacements = fixture.Load(removed, summary);
        Check(removed == std::unordered_set<uint64_t>({102}),
              "Combine removed an unknown/scripted, dynamic, inactive, referenced, or physics entity");
        Check(summary.sourceModels == 1 && summary.combinedModels == 1 && replacements.size() == 1,
              "Unsafe sources were included in the compiled geometry");
        Check(YAML::Dump(fixture.entities) == original, "Cache selection mutated the authored scene");
        std::cout << "Gameplay, collider, inactive, dynamic, unknown-component, and UUID-reference preservation passed\n";
    }

    void CacheFallbacksAndCells()
    {
        Fixture fixture("fallback");
        fixture.Add(101,0);
        fixture.Add(102,101,true);
        auto second = fixture.Add(103,101,true);
        second["Canis::Transform"]["position"] = Vector3(100,0,0);
        fixture.Build(16);
        std::unordered_set<uint64_t> removed;
        CombineModelSummary summary;
        auto replacements = fixture.Load(removed, summary,16);
        Check(replacements.size() == 2 && summary.combinedModels == 2 && summary.sourceModels == 2 && summary.triangleCount == 2,
              "Root-local cells did not keep far-apart sources independently cullable");
        auto reject = [&](const std::string& reason, float cell = 16)
        {
            std::string error;
            YAML::Node replacement;
            std::unordered_set<uint64_t> removedOnFailure;
            CombineModelSummary rejectedSummary;
            Check(!LoadCombineModelReplacement(fixture.entities,101,fixture.cache.string(),cell,
                  replacement,removedOnFailure,rejectedSummary,error), reason + " incorrectly accepted the cache");
            Check(!error.empty(), reason + " did not give an actionable fallback reason");
            Check(removedOnFailure.empty() && (!replacement || replacement.size() == 0),
                  reason + " partially replaced the authored scene before cache validation finished");
        };
        reject("Changed partition size",8);
        second["Canis::Transform"]["position"] = Vector3(101,0,0);
        reject("Changed child transform");
        second["Canis::Transform"]["position"] = Vector3(100,0,0);
        fixture.Load(removed,summary,16);
        const auto sourceBytes = Read(fixture.root / "source.bin");
        const auto sourceTime = fs::last_write_time(fixture.root / "source.bin");
        Write(fixture.root / "source.bin", sourceBytes + "stale");
        reject("Changed source geometry dependency");
        Write(fixture.root / "source.bin", sourceBytes);
        fs::last_write_time(fixture.root / "source.bin", sourceTime);
        fixture.Load(removed,summary,16);
        const auto cacheModel = GeometryPath(replacements[0]);
        const auto geometryBytes = Read(cacheModel);
        Write(cacheModel,"corrupt compiled geometry");
        reject("Corrupt compiled geometry");
        Write(cacheModel,geometryBytes);
        fixture.Load(removed,summary,16);
        fs::remove(cacheModel);
        reject("Missing compiled geometry");
        Write(cacheModel,geometryBytes);
        const auto manifestBytes = Read(fixture.cache);
        auto collision = YAML::Load(manifestBytes);
        collision["replacements"][0]["Entity"] = uint64_t(101);
        Write(fixture.cache,YAML::Dump(collision));
        reject("Replacement UUID collides with retained root");
        Write(fixture.cache,"not a valid manifest");
        reject("Corrupt manifest");
        Write(fixture.cache,manifestBytes);
        fs::remove(fixture.cache);
        reject("Missing manifest");
        std::cout << "Spatial partitions and transactional stale/missing/corrupt cache fallbacks passed\n";
    }

    void MaterialAndAssetUUIDs()
    {
        Fixture fixture("materials");
        fixture.Add(101,0);
        const auto materialA = fixture.root / "a.material", materialB = fixture.root / "b.material";
        Write(materialA,"{color: [0.7, 0.6, 0.5, 1], roughnessValue: 0.4, specularValue: 0.2}");
        Write(materialB,"{color: [0.2, 0.3, 0.4, 1], roughnessValue: 0.8, specularValue: 0.6}");
        auto first = fixture.Add(102,101,true), second = fixture.Add(103,101,true), third = fixture.Add(104,101,true);
        const auto uuid = AssetManager::GetMetaFile(fixture.model.string())->uuid;
        first["Canis::Model"]["ModelAsset"].remove("path");
        first["Canis::Model"]["ModelAsset"]["uuid"] = static_cast<uint64_t>(uuid);
        for (auto entity : {first,second})
        {
            entity["Canis::Material"]["MaterialAsset"]["path"] = materialA.string();
            entity["Canis::Material"]["color"] = Color(.9f,.8f,.7f,1);
        }
        third["Canis::Material"]["MaterialAsset"]["path"] = materialB.string();
        third["Canis::Material"]["color"] = Color(.6f,.5f,.4f,1);
        fixture.Build();
        std::unordered_set<uint64_t> removed;
        CombineModelSummary summary;
        auto replacements = fixture.Load(removed,summary);
        Check(removed == std::unordered_set<uint64_t>({102,103,104}) && replacements.size() == 2 && summary.triangleCount == 3,
              "Compatible materials did not combine or incompatible materials were collapsed together");
        int seenA = 0, seenB = 0;
        for (auto replacement : replacements)
        {
            const auto material = replacement["Canis::Material"];
            const auto path = material["MaterialAsset"]["path"].as<std::string>();
            std::vector<ModelAsset::PrimitiveBuild3D> primitives;
            std::vector<std::string> slots;
            Check(ReadCombinedModelGeometry(GeometryPath(replacement).string(),primitives,slots),
                  "Material-group geometry could not be decoded");
            size_t triangles = 0;
            for (const auto& primitive : primitives) triangles += primitive.indices.size() / 3;
            if (path == materialA.string())
            {
                ++seenA;
                Check(triangles == 2 && material["color"].as<Color>() == Color(.9f,.8f,.7f,1),
                      "Shared material geometry or per-entity material tint was lost");
            }
            else if (path == materialB.string())
            {
                ++seenB;
                Check(triangles == 1 && material["color"].as<Color>() == Color(.6f,.5f,.4f,1),
                      "Distinct material geometry or per-entity material tint was lost");
            }
            else Check(false,"Compiled output references an unknown material");
            Check(!replacement["Canis::Model"]["ModelAsset"]["uuid"],
                  "Compiled model kept its source asset UUID instead of its new persistent cache path");
        }
        Check(seenA == 1 && seenB == 1,"Compiled output lost or duplicated a material group");
        std::cout << "Compatible material grouping, material tint, slots, and UUID-only source handles passed\n";
    }

    struct FixtureReference { Entity target; };
    struct FixtureUnknownBehavior { int value = 0; };
    void PreallocationAndSceneReferences()
    {
        Fixture fixture("scene");
        auto root = fixture.Add(101,0);
        root["Canis::CombineModel"]["active"] = true;
        root["Canis::CombineModel"]["cachePath"] = fixture.cache.string();
        root["Canis::CombineModel"]["cellSize"] = 0;
        fixture.Add(102,101);
        fixture.Add(103,102,true);
        fixture.Add(104,101,true);
        auto preserved = fixture.Add(105,101);
        preserved["Game::UnknownBehavior"]["value"] = 7;
        fixture.Add(106,105,true);
        auto observer = fixture.Add(107,0);
        observer["Game::References"]["target"] = uint64_t(106);
        fixture.Build();
        const auto authoredBytes = YAML::Dump(fixture.entities);
        std::vector<CombinedModelLoad> groups;
        auto prepared = PrepareCombinedModelNodes(fixture.entities,false,groups);
        Check(groups.size() == 1 && groups[0].removed == std::unordered_set<uint64_t>({102,103,104}) && prepared.size() == 5,
              "Scene preparation did not replace the eligible sources before allocation");
        Check(YAML::Dump(fixture.entities) == authoredBytes, "Scene preparation mutated authored YAML nodes through an alias");
        const auto replacementId = prepared[prepared.size()-1]["Entity"].as<uint64_t>();
        std::unordered_set<uint64_t> rootChildren;
        for (auto child : prepared[0]["Canis::Transform"]["children"])
            rootChildren.insert(child.as<uint64_t>());
        Check(rootChildren == std::unordered_set<uint64_t>({105,replacementId}),
              "Prepared parent children list retained a removed UUID or omitted the compiled replacement");
        prepared[0]["Name"] = "Runtime-only mutation";
        Check(YAML::Dump(fixture.entities) == authoredBytes, "Runtime replacement data still aliases the editable scene");

        // Exercise the real Scene allocator and UUID fixups with a counting codec.
        // This intentionally avoids importing a source model in the fixture codec:
        // the question under test is which entities reach DecodeEntity at all.
        const bool editorWasEnabled = IsEditorRuntimeEnabled();
        struct RestoreEditorMode { bool value; ~RestoreEditorMode() { SetEditorRuntimeEnabled(value); } } restore{editorWasEnabled};
        SetEditorRuntimeEnabled(false);
        App app;
        app.scene.app = &app;
        std::vector<uint64_t> decoded;
        ComponentConf codec;
        codec.name = "CombineModel counting fixture";
        codec.Decode = [&](YAML::Node& node, Entity& entity, bool)
        {
            decoded.push_back(uint64_t(entity.GetUUID()));
            if (auto transform = node["Canis::Transform"])
            {
                auto& value = *entity.AddComponent<Transform>();
                value.position = transform["position"].as<Vector3>(Vector3(0));
                value.rotation = transform["rotation"].as<Quaternion>(Quaternion(Vector3(0)));
                value.scale = transform["scale"].as<Vector3>(Vector3(1));
                entity.scene.GetEntityAfterLoad(transform["parent"].as<UUID>(0),value.parent);
                for (auto child : transform["children"])
                {
                    value.children.emplace_back();
                    entity.scene.GetEntityAfterLoad(child.as<UUID>(),value.children.back());
                }
            }
            if (node["Canis::Model"]) entity.AddComponent<Model>();
            if (node["Canis::CombineModel"]) entity.AddComponent<CombineModel>();
            if (auto behavior = node["Game::UnknownBehavior"])
                entity.AddComponent<FixtureUnknownBehavior>()->value = behavior["value"].as<int>();
            if (auto reference = node["Game::References"])
                entity.scene.GetEntityAfterLoad(reference["target"].as<UUID>(),entity.AddComponent<FixtureReference>()->target);
        };
        app.RegisterComponent(codec);
        const auto loaded = app.scene.LoadEntityNodes(fixture.entities);
        Check(loaded.size() == 5 && decoded.size() == 5 && app.scene.GetEntities().size() == 5,
              "Scene allocated or decoded authored source entities before replacing them");
        for (const uint64_t id : {102,103,104})
            Check(!app.scene.GetEntityWithUUID(UUID(id)) && std::find(decoded.begin(),decoded.end(),id) == decoded.end(),
                  "Removed source entity reached the actual allocator/decoder");
        auto* runtimeRoot = app.scene.GetEntityWithUUID(UUID(101));
        auto* runtimePreserved = app.scene.GetEntityWithUUID(UUID(105));
        auto* runtimeLeaf = app.scene.GetEntityWithUUID(UUID(106));
        auto* runtimeObserver = app.scene.GetEntityWithUUID(UUID(107));
        auto* runtimeCombined = app.scene.GetEntityWithUUID(UUID(replacementId));
        Check(runtimeRoot && runtimePreserved && runtimeLeaf && runtimeObserver && runtimeCombined,
              "Scene lost a retained UUID or the compiled model UUID");
        Check(runtimePreserved->GetComponent<FixtureUnknownBehavior>().value == 7,
              "Combined loading removed or changed a preserved gameplay component");
        Check(runtimeLeaf->GetComponent<Transform>().parent == runtimePreserved &&
              runtimeCombined->GetComponent<Transform>().parent == runtimeRoot &&
              runtimeObserver->GetComponent<FixtureReference>().target == runtimeLeaf,
              "Scene UUID references did not resolve to the retained and generated entities");
        const auto& component = runtimeRoot->GetComponent<CombineModel>();
        Check(component.sourceModels == 2 && component.combinedModels == 1 && component.triangleCount == 2,
              "Live CombineModel counters did not reflect the loaded cache");
        Check(YAML::Dump(fixture.entities) == authoredBytes,"Native scene loading mutated authored nodes");

        app.scene.Unload();
        decoded.clear();
        fs::remove(GeometryPath(prepared[prepared.size()-1]));
        const auto fallback = app.scene.LoadEntityNodes(fixture.entities);
        Check(fallback.size() == fixture.entities.size() && decoded.size() == fixture.entities.size() &&
              app.scene.GetEntityWithUUID(UUID(103)) && app.scene.GetEntityWithUUID(UUID(104)) &&
              !app.scene.GetEntityWithUUID(UUID(replacementId)),
              "Missing cache did not load the complete original scene before allocation");
        Check(app.scene.GetEntityWithUUID(UUID(107))->GetComponent<FixtureReference>().target == app.scene.GetEntityWithUUID(UUID(106)),
              "Fallback loading changed preserved gameplay UUID references");
        std::cout << "Native preallocation replacement, authoring isolation, hierarchy/UUID fixups, counters, and complete fallback passed\n";
    }

    void LivePlayAndSnapshotRestore(Window& window)
    {
        Fixture fixture("live-play");
        auto authoredRoot = fixture.Add(101,0);
        authoredRoot["Canis::CombineModel"]["active"] = false;
        authoredRoot["Canis::CombineModel"]["cachePath"] = fixture.cache.string();
        authoredRoot["Canis::CombineModel"]["cellSize"] = 0;
        fixture.Add(102,101);
        fixture.Add(103,102,true);
        fixture.Add(104,101,true);
        auto gameplay = fixture.Add(105,101);
        gameplay["Game::UnknownBehavior"]["value"] = 7;
        fixture.Add(106,105,true);
        auto observer = fixture.Add(107,0);
        observer["Game::References"]["target"] = uint64_t(106);

        const bool editorWasEnabled = IsEditorRuntimeEnabled();
        struct RestoreEditorMode { bool value; ~RestoreEditorMode() { SetEditorRuntimeEnabled(value); } } restore{editorWasEnabled};
        SetEditorRuntimeEnabled(false);
        App app;
        app.scene.Init(&app,&window,nullptr);
        ComponentConf codec;
        codec.name = "CombineModel live-play fixture";
        codec.Decode = [](YAML::Node& node, Entity& entity, bool)
        {
            if (auto transform = node["Canis::Transform"])
            {
                auto& value = *entity.AddComponent<Transform>();
                value.position = transform["position"].as<Vector3>(Vector3(0));
                value.rotation = transform["rotation"].as<Quaternion>(Quaternion(Vector3(0)));
                value.scale = transform["scale"].as<Vector3>(Vector3(1));
                entity.scene.GetEntityAfterLoad(transform["parent"].as<UUID>(0),value.parent);
                for (auto child : transform["children"])
                {
                    value.children.emplace_back();
                    entity.scene.GetEntityAfterLoad(child.as<UUID>(),value.children.back());
                }
            }
            if (auto model = node["Canis::Model"])
            {
                auto& value = *entity.AddComponent<Model>();
                value.modelId = AssetManager::LoadModel(model["ModelAsset"]["path"].as<std::string>());
                value.nodeIndex = model["nodeIndex"].as<int>(-1);
                value.applyNodeTransform = model["applyNodeTransform"].as<bool>(true);
                value.staticModel = model["static"].as<bool>(false);
                value.color = model["color"].as<Color>(Color(1));
            }
            if (auto combine = node["Canis::CombineModel"])
            {
                auto& value = *entity.AddComponent<CombineModel>();
                value.active = combine["active"].as<bool>(true);
                value.cachePath = combine["cachePath"].as<std::string>();
                value.cellSize = combine["cellSize"].as<float>(0);
            }
            if (auto behavior = node["Game::UnknownBehavior"])
                entity.AddComponent<FixtureUnknownBehavior>()->value = behavior["value"].as<int>();
            if (auto reference = node["Game::References"])
                entity.scene.GetEntityAfterLoad(reference["target"].as<UUID>(),entity.AddComponent<FixtureReference>()->target);
        };
        codec.Encode = [](YAML::Node& node, Entity& entity)
        {
            if (auto* value = entity.TryGetComponent<Transform>())
            {
                auto transform = node["Canis::Transform"];
                transform["active"] = value->active;
                transform["position"] = value->position;
                transform["rotation"] = value->rotation;
                transform["scale"] = value->scale;
                transform["parent"] = uint64_t(value->parent.GetUUID());
                transform["children"] = YAML::Node(YAML::NodeType::Sequence);
                for (auto child : value->children) transform["children"].push_back(uint64_t(child.GetUUID()));
            }
            if (auto* value = entity.TryGetComponent<Model>())
            {
                auto model = node["Canis::Model"];
                model["ModelAsset"]["path"] = AssetManager::GetPath(value->modelId);
                model["nodeIndex"] = value->nodeIndex;
                model["applyNodeTransform"] = value->applyNodeTransform;
                model["static"] = value->staticModel;
                model["color"] = value->color;
            }
            if (auto* value = entity.TryGetComponent<CombineModel>())
            {
                auto combine = node["Canis::CombineModel"];
                combine["active"] = value->active;
                combine["cachePath"] = value->cachePath;
                combine["cellSize"] = value->cellSize;
            }
            if (auto* value = entity.TryGetComponent<FixtureUnknownBehavior>())
                node["Game::UnknownBehavior"]["value"] = value->value;
            if (auto* value = entity.TryGetComponent<FixtureReference>())
                node["Game::References"]["target"] = uint64_t(value->target.GetUUID());
        };
        app.RegisterComponent(codec);
        // The fixture has no App-owned Editor instance. Temporarily disabling
        // the component for authoring loads emulates EditorMode::EDIT's gate;
        // the real live Play activation below is not mocked or preprocessed.
        app.scene.LoadEntityNodes(fixture.entities);
        app.scene.GetEntityWithUUID(UUID(101))->GetComponent<CombineModel>().active = true;
        const auto snapshot = app.scene.EncodeScene();
        const auto snapshotBytes = YAML::Dump(snapshot);
        const auto sourceEntities = snapshot["Entities"];
        std::string error;
        const bool built = BuildCombineModel(sourceEntities,101,fixture.cache.string(),0,error);
        Check(built,"Live Play fixture cache build failed: " + error);
        const auto liveCount = [&]
        {
            size_t count = 0;
            for (auto* entity : app.scene.GetEntities()) if (entity) ++count;
            return count;
        };
        Check(liveCount() == 7,"Authoring fixture did not retain all source objects");
        for (int pass = 0; pass < 2; ++pass)
        {
            Entity root = app.scene.FindEntity(UUID(101));
            Entity sourceGroup = app.scene.FindEntity(UUID(102));
            Entity firstSource = app.scene.FindEntity(UUID(103));
            Entity secondSource = app.scene.FindEntity(UUID(104));
            Entity retained = app.scene.FindEntity(UUID(105));
            Entity retainedLeaf = app.scene.FindEntity(UUID(106));
            Entity references = app.scene.FindEntity(UUID(107));
            Check(app.scene.ActivateCombinedModelsForPlay(),"Live editor Play did not activate its cached model");
            Check(liveCount() == 5 && !sourceGroup && !firstSource && !secondSource,
                  "Live Play failed to destroy source render objects and their empty authoring group");
            Check(root && retained && retainedLeaf && references &&
                  root == app.scene.FindEntity(UUID(101)) && retained == app.scene.FindEntity(UUID(105)) &&
                  retainedLeaf == app.scene.FindEntity(UUID(106)) && references == app.scene.FindEntity(UUID(107)),
                  "Live Play recreated retained gameplay entities instead of preserving their identity");
            Check(retained->GetComponent<FixtureUnknownBehavior>().value == 7 &&
                  retainedLeaf->GetComponent<Transform>().parent == retained &&
                  references->GetComponent<FixtureReference>().target == retainedLeaf,
                  "Live Play changed a preserved gameplay component, transform, or entity reference");
            auto& children = root->GetComponent<Transform>().children;
            Check(children.size() == 2,"Live Play root contains stale or duplicate child links");
            Entity compiled;
            for (auto child : children)
            {
                Check(child && child->GetComponent<Transform>().parent == root,"Live Play left an invalid child/parent link");
                if (child->HasComponent<Model>()) compiled = child;
            }
            Check(compiled && compiled.GetUUID() != UUID(103) && compiled.GetUUID() != UUID(104) &&
                  AssetManager::GetPath(compiled->GetComponent<Model>().modelId).ends_with(".cmodel"),
                  "Live Play did not decode a compiled model with its own valid entity identity");
            const auto& counters = root->GetComponent<CombineModel>();
            Check(counters.sourceModels == 2 && counters.combinedModels == 1 && counters.triangleCount == 2,
                  "Live Play did not update its CombineModel counters");
            Check(!app.scene.ActivateCombinedModelsForPlay() && liveCount() == 5,
                  "Repeated live activation duplicated its generated model");
            retained->GetComponent<FixtureUnknownBehavior>().value = 99;
            retainedLeaf->GetComponent<Transform>().position = Vector3(9,8,7);
            Check(YAML::Dump(snapshot) == snapshotBytes,"Live Play mutated its pre-Play authoring snapshot");

            app.scene.Unload();
            Check(!root && !retained && !retainedLeaf && !references && !compiled,
                  "Stop left runtime entity handles alive after scene unload");
            auto editEntities = YAML::Clone(sourceEntities);
            editEntities[0]["Canis::CombineModel"]["active"] = false;
            app.scene.LoadEntityNodes(editEntities);
            app.scene.GetEntityWithUUID(UUID(101))->GetComponent<CombineModel>().active = true;
            Check(liveCount() == 7 && app.scene.FindEntity(UUID(103)) && app.scene.FindEntity(UUID(104)),
                  "Stop snapshot restoration lost the original editable render objects");
            Check(YAML::Dump(app.scene.EncodeScene()["Entities"]) == YAML::Dump(sourceEntities),
                  "Stop did not restore the original transforms, gameplay data, UUIDs, or hierarchy exactly");
        }
        std::cout << "Live Play replacement preserves gameplay identity; two Stop snapshot restores recover all editable sources exactly\n";
    }

    int FreshLoad(const fs::path& root)
    {
        Window window("Fresh compiled-model process",64,64,true);
        for (int i = 0; i < 11; ++i)
        {
            const auto path = root / ("unrelated-" + std::to_string(i) + ".png");
            fs::copy_file(root / "albedo.png",path);
            AssetManager::LoadTexture(path.string());
        }
        YAML::Node entities = YAML::LoadFile((root / "entities.yaml").string()), replacements;
        std::unordered_set<uint64_t> removed;
        CombineModelSummary summary;
        std::string error;
        const bool loaded = LoadCombineModelReplacement(entities,101,(root / "combined.cache").string(),0,
              replacements,removed,summary,error);
        Check(loaded, "Fresh process rejected a valid manifest: " + error);
        Check(AssetManager::GetID((root / "source.gltf").string()) < 0,
              "Play loaded a source model while validating its compiled replacement");
        std::vector<ModelAsset::PrimitiveBuild3D> primitives;
        std::vector<std::string> slots;
        Check(ReadCombinedModelGeometry(GeometryPath(replacements[0]).string(),primitives,slots) && !primitives.empty(),
              "Fresh process did not decode compiled geometry");
        TextureReferences(primitives[0],root);
        Check(AssetManager::GetID((root / "source.gltf").string()) < 0,
              "Compiled geometry decoder imported its source model");
        return 0;
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc > 2 && std::string(argv[1]) == "--fresh-load") return FreshLoad(fs::path(argv[2]));
        const auto executable = fs::absolute(argv[0]);
        if (argc > 1) fs::current_path(argv[1]);
        Window window("CombineModel tests",64,64,true);
        GeometryAndFreshProcess(executable);
        SafeSelection();
        CacheFallbacksAndCells();
        MaterialAndAssetUUIDs();
        PreallocationAndSceneReferences();
        LivePlayAndSnapshotRestore(window);
        Check(glGetError() == GL_NO_ERROR,"OpenGL error during compiled-model checks");
        std::cout << "COMBINE_MODEL_TESTS_PASS\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "COMBINE_MODEL_TESTS_FAIL: " << error.what() << '\n';
        return 1;
    }
}
