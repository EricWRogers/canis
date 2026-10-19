#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/AssetPipeline.hpp>
#include <Canis/Components.hpp>
#include <Canis/Editor.hpp>
#include <Canis/LightingBake.hpp>
#include <Canis/UnityImport.hpp>
#include <Canis/VisibilityBake.hpp>
#include <Canis/Yaml.hpp>
#include <fstream>
#include <iostream>
#include <stb_image.h>
using namespace Canis;
static void Check(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}
int main()
{
    auto directory = std::filesystem::temp_directory_path() / "canis-native-bake-tests";
    try
    {
        std::filesystem::create_directories(directory);
        const std::filesystem::path fixtures = CANIS_BAKE_FIXTURES;
        auto imported = ImportUnityScene(fixtures, fixtures / "layout.unity", directory / "unity");
        auto layout = YAML::LoadFile(imported.scene.string());
        Check(imported.entities == 2, "Unity import lost prefab instances");
        const auto sky = YAML::LoadFile(layout["Environment"]["SkyboxAsset"]["path"].as<std::string>());
        Check(sky.size() == 6 && sky["top"]["rotation"].as<int>(0) == 180 &&
                  sky["bottom"]["rotation"].as<int>(0) == 180 && sky["right"]["rotation"].as<int>(0) == 0,
              "Unity skybox pole orientation does not match the converted side "
              "faces");
        const auto slots = layout["Entities"][0]["Canis::Material"]["MaterialSlots"];
        const auto material = YAML::LoadFile(slots[0]["path"].as<std::string>());
        int width, height, channels;
        auto* dark =
            stbi_load(material["emission"]["path"].as<std::string>().c_str(), &width, &height, &channels, 3);
        Check(dark && dark[0] == 0 && dark[1] == 0 && dark[2] == 0,
              "Zero-alpha Unity illumination became white emission");
        stbi_image_free(dark);
        std::vector<float> tilePositions;
        for (auto tile : layout["Entities"])
        {
            auto transform = tile["Canis::Transform"];
            auto position = transform["position"].as<Vector3>();
            tilePositions.push_back(position.x);
            auto rotation = transform["rotation"].as<Quaternion>();
            auto model = Pipeline::Flatten(
                Pipeline::ReadGltf(tile["Canis::Model"]["ModelAsset"]["path"].as<std::string>()));
            Check(model.primitives.size() == 2 && model.primitives[0].material == 0 &&
                      model.primitives[1].material == 1,
                  "Unity FBX submesh material slots were lost");
            bool firstTriangle = false, secondTriangle = false;
            for (auto v : model.primitives[0].vertices)
                firstTriangle |= glm::distance(v.position, Vector3(-6, -6, 0)) < .001f;
            for (auto v : model.primitives[1].vertices)
                secondTriangle |= glm::distance(v.position, Vector3(0, 0, 0)) < .001f;
            Check(firstTriangle && secondTriangle,
                  "FBX connection order swapped Unity's first-use material slots");
            Vector3 minimum(FLT_MAX), maximum(-FLT_MAX);
            for (auto vertex : model.primitives[0].vertices)
            {
                auto point = position + rotation * vertex.position;
                minimum = glm::min(minimum, point);
                maximum = glm::max(maximum, point);
                Check((rotation * vertex.normal).y > .99f, "FBX conversion reversed floor normals");
            }
            Check(std::abs(minimum.x - (position.x - 6)) < .001f && std::abs(maximum.x - position.x) < .001f,
                  "FBX mirror and prefab rotation placed the floor outside its "
                  "authored tile");
            Check(std::abs(minimum.y) < .001f && std::abs(maximum.y) < .001f &&
                      std::abs(minimum.z + 6) < .001f,
                  "FBX axis rotation was applied twice");
        }
        std::sort(tilePositions.begin(), tilePositions.end());
        Check(tilePositions == std::vector<float>({6, 12}), "Prefab overrides leaked between instances");
        Check(TriangleOccludesCell({-.1f, -.1f, -.1f}, {.1f, .1f, .1f}, {-.1f, -.1f, -4.1f},
                                   {.1f, .1f, -3.9f}, {-10, -10, -2}, {10, -10, -2}, {0, 10, -2}),
              "Native occlusion proof rejected a solid face");
        Check(!TriangleOccludesCell({-.1f, -.1f, -.1f}, {.1f, .1f, .1f}, {-20, -.1f, -4.1f}, {20, .1f, -3.9f},
                                    {-10, -10, -2}, {10, -10, -2}, {0, 10, -2}),
              "Partial triangle coverage culled a visible target");
        Check(!TriangleOccludesCell({-1, -1, -3}, {1, 1, 0}, {-.1f, -.1f, -4.1f}, {.1f, .1f, -3.9f},
                                    {-10, -10, -2}, {10, -10, -2}, {0, 10, -2}),
              "Camera cell crossing the occluder was rejected");
        Pipeline::Mesh mesh;
        Pipeline::Primitive p;
        p.vertices = {{{-1, 0, -1}, {0, 1, 0}, {0, 0}, {1, 1, 1, 1}},
                      {{-1, 0, 1}, {0, 1, 0}, {0, 1}, {1, 1, 1, 1}},
                      {{1, 0, 1}, {0, 1, 0}, {1, 1}, {1, 1, 1, 1}},
                      {{1, 0, -1}, {0, 1, 0}, {1, 0}, {1, 1, 1, 1}}};
        p.indices = {0, 1, 2, 0, 2, 3};
        mesh.primitives.push_back(p);
        auto source = directory / "floor.gltf";
        Pipeline::WriteGltf(mesh, source);
        auto loaded = Pipeline::Flatten(Pipeline::ReadGltf(source));
        Check(loaded.primitives[0].indices.size() == 6, "Native glTF roundtrip lost geometry");
        auto imageFixture = Pipeline::ReadGltf(source);
        imageFixture.json["images"] = Pipeline::Json::array({{{"uri", "source.png"}}});
        std::filesystem::copy_file(fixtures / "dark.png", directory / "source.png");
        auto relocated = Pipeline::Flatten(imageFixture);
        const auto relocatedPath = directory / "relocated" / "mesh.gltf";
        Pipeline::WriteGltf(relocated, relocatedPath);
        const auto relocatedJson = Pipeline::ReadGltf(relocatedPath).json;
        Check(std::filesystem::exists(relocatedPath.parent_path() /
                                      relocatedJson["images"][0]["uri"].get<std::string>()),
              "Native bake left an image URI relative to the source model directory");
        YAML::Node scene;
        scene["Environment"]["AmbientLight"] = Vector4(.02f, .02f, .02f, 1);
        scene["Environment"]["AmbientLightIntensity"] = 1;
        YAML::Node floor;
        floor["Entity"] = 1;
        floor["Name"] = "Floor";
        floor["Active"] = true;
        floor["Canis::Transform"]["position"] = Vector3(0);
        floor["Canis::Transform"]["rotation"] = Vector3(0);
        floor["Canis::Transform"]["scale"] = Vector3(1);
        floor["Canis::Transform"]["parent"] = 0;
        floor["Canis::Model"]["static"] = true;
        floor["Canis::Model"]["ModelAsset"]["path"] = source.string();
        scene["Entities"].push_back(floor);
        YAML::Node lamp = YAML::Clone(floor);
        lamp["Entity"] = 2;
        lamp["Name"] = "Lamp";
        lamp.remove("Canis::Model");
        lamp["Canis::Transform"]["position"] = Vector3(0, 1, 0);
        lamp["Canis::PointLight"]["enabled"] = true;
        lamp["Canis::PointLight"]["color"] = Vector4(1, .3f, .1f, 1);
        lamp["Canis::PointLight"]["intensity"] = 4;
        lamp["Canis::PointLight"]["range"] = 5;
        scene["Entities"].push_back(lamp);
        LightingBakeOptions options;
        options.vertexSpacing = .5f;
        options.indirectSamples = 0;
        auto baked = BakeSceneLighting(scene, directory / "baked", options);
        Check(baked.models.size() == 1 && baked.lights == 1,
              "Native lighting bake failed to collect geometry/lights");
        auto geometry = Pipeline::Flatten(Pipeline::ReadGltf(baked.models[0]));
        Check(geometry.primitives[0].vertices.size() > 4,
              "Lighting sampling did not subdivide sparse surfaces");
        float brightest = 0;
        for (auto v : geometry.primitives[0].vertices)
        {
            brightest = std::max(brightest, v.color.x);
            Check(v.color.x > v.color.y && std::isfinite(v.color.x),
                  "Baked warm light has invalid irradiance");
        }
        Check(brightest > .5f, "Native lighting bake omitted direct light");
        auto json = Pipeline::ReadGltf(baked.models[0]).json;
        Check(json["meshes"][0]["primitives"][0]["extras"]["canisBakedLighting"].get<bool>(),
              "Baked geometry lacks shader metadata");
        // Realtime lamps must never be frozen into the offline result.
        auto realtime = YAML::Clone(scene);
        realtime["Entities"][1]["Canis::PointLight"]["mode"] = 0;
        auto realtimeBake = BakeSceneLighting(realtime, directory / "realtime", options);
        Check(realtimeBake.lights == 0, "Realtime light leaked into the bake");
        auto bakedOnly = YAML::Clone(scene);
        bakedOnly["Entities"][1]["Canis::PointLight"]["mode"] = 2;
        Check(BakeSceneLighting(bakedOnly, directory / "baked-only", options).lights == 1,
              "Baked light was omitted from the bake");
        auto mixed = YAML::Clone(scene);
        mixed["Entities"][1]["Canis::PointLight"]["mode"] = 1;
        Check(BakeSceneLighting(mixed, directory / "mixed", options).lights == 1,
              "Mixed light was omitted from the bake");
        auto spotScene = YAML::Clone(scene);
        auto spotNode = spotScene["Entities"][1];
        spotNode.remove("Canis::PointLight");
        auto spot = spotNode["Canis::SpotLight"];
        spot["mode"] = 2;
        spot["enabled"] = true;
        spot["color"] = Vector4(1);
        spot["intensity"] = 4;
        spot["range"] = 5;
        spot["spotAngle"] = 50;
        spot["innerAngle"] = 30;
        spotNode["Canis::Transform"]["rotation"] = Vector3(-glm::half_pi<float>(), 0, 0);
        auto coneBake = BakeSceneLighting(spotScene, directory / "spot", options);
        Check(coneBake.lights == 1, "Spotlight was omitted from the bake");
        auto coneMesh = Pipeline::Flatten(Pipeline::ReadGltf(coneBake.models[0]));
        float litCenter = 0, darkEdge = 0;
        for (auto vertex : coneMesh.primitives[0].vertices)
        {
            auto distance = glm::length(Vector2(vertex.position.x, vertex.position.z));
            if (distance < .2f)
                litCenter = std::max(litCenter, vertex.color.x);
            if (distance > 1.f)
                darkEdge = std::max(darkEdge, vertex.color.x);
        }
        Check(litCenter > darkEdge + .2f, "Baked spotlight did not preserve its cone");
        spot["mode"] = 0;
        Check(BakeSceneLighting(spotScene, directory / "realtime-spot", options).lights == 0,
              "Realtime spotlight leaked into the bake");
        LightingScenario dusk;
        dusk.id = "dusk-stable-id";
        dusk.name = "Dusk";
        dusk.ambientLight = Vector4(.4f, .2f, .1f, 1);
        dusk.ambientIntensity = .7f;
        dusk.sunDirection = Vector3(0, -1, 0);
        dusk.sunColor = Vector4(1, .7f, .4f, 1);
        dusk.sunIntensity = 3.5f;
        dusk.skyboxIntensity = .8f;
        LightingScenario night = dusk;
        night.id = "night-stable-id";
        night.name = "Night";
        night.ambientLight = Vector4(.03f, .06f, .15f, 1);
        night.ambientIntensity = .2f;
        night.sunEnabled = false;
        night.skyboxIntensity = .1f;
        auto roundtrip = DeserializeLightingScenario(SerializeLightingScenario(dusk));
        Check(roundtrip.id == dusk.id && roundtrip.name == dusk.name &&
                  roundtrip.sunIntensity == dusk.sunIntensity && roundtrip.skyboxIntensity == .8f,
              "Lighting scenario roundtrip lost authoring settings");
        auto renamed = dusk;
        renamed.name = "Evening";
        Check(LightingScenarioSettingsFingerprint(renamed) == LightingScenarioSettingsFingerprint(dusk),
              "Renaming a scenario invalidated its stable baked channel");
        auto skyScenario = dusk;
        const auto skyPath = directory / "fingerprint.skybox";
        std::ofstream(skyPath) << "right:\n  path: " << (directory / "source.png").string() << '\n';
        skyScenario.skyboxPath = skyPath.string();
        const auto beforeSkyImage = LightingScenarioSettingsFingerprint(skyScenario);
        const auto beforeSkyAuthoring = LightingScenarioAuthoringFingerprint(skyScenario);
        std::ofstream(directory / "source.png", std::ios::binary | std::ios::app).put('\0');
        Check(beforeSkyImage != LightingScenarioSettingsFingerprint(skyScenario),
              "Editing a skybox facet did not invalidate the captured lighting");
        Check(beforeSkyAuthoring == LightingScenarioAuthoringFingerprint(skyScenario),
              "Metadata-only runtime signature read skybox image contents");
        auto scenarios = BakeLightingScenarios(scene, directory / "scenarios", {dusk, night}, options);
        Check(scenarios.models.size() == 1 && scenarios.vertices == baked.vertices,
              "Scenarios duplicated or changed shared geometry");
        const auto scenarioGeometry = Pipeline::ReadGltf(scenarios.models[0]);
        const auto& scenarioPrimitive = scenarioGeometry.json["meshes"][0]["primitives"][0];
        Check(scenarioPrimitive["extras"]["canisLightingScenarios"].size() == 2,
              "Shared geometry does not contain both lighting channels");
        for (const auto& scenario : {dusk, night})
        {
            auto independent = YAML::Clone(scene);
            independent["Environment"]["AmbientLight"] = scenario.ambientLight;
            independent["Environment"]["AmbientLightIntensity"] = scenario.ambientIntensity;
            YAML::Node sun;
            sun["Entity"] = 3;
            sun["Canis::Transform"] = YAML::Clone(floor["Canis::Transform"]);
            auto light = sun["Canis::DirectionalLight"];
            light["enabled"] = scenario.sunEnabled;
            light["direction"] = scenario.sunDirection;
            light["color"] = scenario.sunColor;
            light["intensity"] = scenario.sunIntensity;
            light["mode"] = 2;
            independent["Entities"].push_back(sun);
            const auto endpoint =
                BakeSceneLighting(independent, directory / ("endpoint-" + scenario.id), options);
            const auto endpointGltf = Pipeline::ReadGltf(endpoint.models[0]);
            const auto& endpointPrimitive = endpointGltf.json["meshes"][0]["primitives"][0];
            for (const char* attribute : {"POSITION", "NORMAL", "TEXCOORD_0"})
                Check(Pipeline::Attribute(scenarioGeometry,
                                          scenarioPrimitive["attributes"][attribute].get<int>()) ==
                          Pipeline::Attribute(endpointGltf,
                                              endpointPrimitive["attributes"][attribute].get<int>()),
                      "Scenario geometry differs from independent native bake");
            const auto rgb = Pipeline::Attribute(
                scenarioGeometry,
                scenarioPrimitive["extras"]["canisLightingScenarios"][scenario.id].get<int>());
            const auto rgba =
                Pipeline::Attribute(endpointGltf, endpointPrimitive["attributes"]["COLOR_0"].get<int>());
            Check(rgb.size() / 3 == rgba.size() / 4, "Scenario lighting has the wrong vertex order/count");
            for (size_t v = 0; v < rgb.size() / 3; ++v)
                for (size_t c = 0; c < 3; ++c)
                    Check(std::abs(rgb[v * 3 + c] - rgba[v * 4 + c]) < .00001f,
                          "Scenario endpoint differs from independent lighting");
            if (scenario.id == dusk.id)
                Check(*std::max_element(rgb.begin(), rgb.end()) > 1, "Scenario irradiance lost HDR range");
        }
        const auto sourceFingerprint = LightingScenarioSourceFingerprint(scene);
        Check(sourceFingerprint == LightingScenarioSourceFingerprint(scenarios.scene),
              "Saving baked scenario references invalidated unchanged source "
              "geometry");
        // Scene encoding adds component defaults, turns asset paths into UUIDs and writes
        // floats/quaternions through the engine's serializers. Those are not geometry edits.
        Pipeline::WriteMeta(source, "MODEL");
        AssetManager::LoadMetaFile(source.string());
        const auto materialPath = directory / "canonical.material";
        std::ofstream(materialPath) << "color: [0.62, 0.3, 0.1, 1]\n";
        Pipeline::WriteMeta(materialPath, "MATERIAL");
        AssetManager::LoadMetaFile(materialPath.string());
        auto authoring = YAML::Clone(scene);
        authoring["Entities"][0]["Canis::Transform"]["position"] = YAML::Load("[0.62,0,0]");
        authoring["Entities"][0]["Canis::Transform"]["rotation"] = Vector3(0, .7f, 0);
        authoring["Entities"][0]["Canis::Material"]["MaterialAsset"]["path"] = materialPath.string();
        authoring["Entities"][0]["Canis::Material"]["MaterialSlots"][0]["path"] = materialPath.string();
        authoring["Entities"][0]["Canis::Material"]["UniformOverrides"]["roughnessValue"] =
            YAML::Load("0.62");
        auto encodedLikeScene = YAML::Clone(authoring);
        auto encodedFloor = encodedLikeScene["Entities"][0];
        auto encodedModel = encodedFloor["Canis::Model"];
        encodedModel["ModelAsset"].remove("path");
        encodedModel["ModelAsset"]["uuid"] = Pipeline::AssetUUID(source.generic_string());
        encodedModel["nodeIndex"] = -1;
        encodedModel["applyNodeTransform"] = true;
        encodedModel["color"] = Vector4(1);
        encodedModel["castShadow"] = true;
        encodedModel["bakedVisibility"] = false;
        YAML::Node encodedTransform;
        encodedTransform["parent"] = 0;
        encodedTransform["active"] = true;
        encodedTransform["rotation"] =
            authoring["Entities"][0]["Canis::Transform"]["rotation"].as<Quaternion>();
        encodedTransform["scale"] = Vector3(1);
        encodedTransform["children"] = YAML::Node(YAML::NodeType::Sequence);
        encodedTransform["position"] = authoring["Entities"][0]["Canis::Transform"]["position"].as<Vector3>();
        encodedFloor["Canis::Transform"] = encodedTransform;
        auto encodedMaterial = encodedFloor["Canis::Material"];
        encodedMaterial["color"] = Vector4(1);
        encodedMaterial["MaterialAsset"].remove("path");
        encodedMaterial["MaterialAsset"]["uuid"] = Pipeline::AssetUUID(materialPath.generic_string());
        encodedMaterial["MaterialSlots"][0].remove("path");
        encodedMaterial["MaterialSlots"][0]["uuid"] = Pipeline::AssetUUID(materialPath.generic_string());
        encodedMaterial["UniformOverrides"].remove("roughnessValue");
        encodedMaterial["UniformOverrides"]["roughnessValue"]["type"] = "float";
        encodedMaterial["UniformOverrides"]["roughnessValue"]["value"] = .62f;
        Check(
            LightingScenarioSourceFingerprint(authoring) ==
                LightingScenarioSourceFingerprint(encodedLikeScene),
            "Scene serializer defaults, float formatting or UUID references invalidated unchanged lighting");
        {
            App app;
            Editor editor;
            app.RegisterDefaults(editor);
            app.scene.app = &app;
            auto lightNodes = YAML::Load(R"(
- Entity: 8101
  Canis::Transform: {position: [0.62, 0, 0]}
  Canis::PointLight: {mode: 2, color: [1, 0.62, 0.32], intensity: 5, range: 13}
- Entity: 8102
  Canis::Transform: {position: [0, 3, 0]}
  Canis::SpotLight: {mode: 2, color: [0.22, 0.65, 1], intensity: 4, range: 10, spotAngle: 55, innerAngle: 35}
- Entity: 8103
  Canis::Transform: {position: [0, 0, 0]}
  Canis::DirectionalLight: {mode: 2, color: [0.9, 0.8, 0.4], direction: [0, -1, 0], intensity: 2}
- Entity: 8104
  Canis::Transform: {position: [0, 0, 0]}
  Canis::PointLight: {mode: 2, color: [1, 0.5, 0.25, 0.3], intensity: 1, range: 10}
)");
            const auto lights = app.scene.LoadEntityNodes(lightNodes);
            Check(glm::distance(lights[0]->GetComponent<PointLight>().color, Vector4(1, .62f, .32f, 1)) <
                          .00001f &&
                      glm::distance(lights[1]->GetComponent<SpotLight>().color, Vector4(.22f, .65f, 1, 1)) <
                          .00001f &&
                      glm::distance(lights[2]->GetComponent<DirectionalLight>().color,
                                    Vector4(.9f, .8f, .4f, 1)) < .00001f &&
                      std::abs(lights[3]->GetComponent<PointLight>().color.a - .3f) < .00001f,
                  "RGB/RGBA light colors changed when loaded by the runtime registry");
            YAML::Node lightInput, encodedLights;
            lightInput["Environment"] = YAML::Node(YAML::NodeType::Map);
            encodedLights["Environment"] = YAML::Node(YAML::NodeType::Map);
            lightInput["Entities"] = lightNodes;
            for (auto* light : lights)
                encodedLights["Entities"].push_back(app.scene.EncodeEntity(*light));
            Check(LightingScenarioSourceFingerprint(lightInput) ==
                      LightingScenarioSourceFingerprint(encodedLights),
                  "Live light registry roundtrip invalidated identical authored light colors");
            app.scene.Unload();
        }
        // Runtime Model serialization retains UUIDs, whereas generated files remain local/ignored.
        // The versioned environment source map must still allow a clean checkout to open the scene.
        auto reopened = YAML::Clone(scenarios.scene);
        reopened["Entities"][0]["Canis::Model"].remove("LightingScenarioSource");
        reopened["Entities"][0]["Canis::Model"]["ModelAsset"].remove("path");
        reopened["Entities"][0]["Canis::Model"]["ModelAsset"]["uuid"] =
            reopened["Environment"]["LightingScenarioSources"][0]["GeneratedUUID"].as<uint64_t>();
        const auto savedGeneration = std::filesystem::path(scenarios.models[0]).parent_path();
        const auto unavailableGeneration = savedGeneration.string() + "-unavailable";
        std::filesystem::rename(savedGeneration, unavailableGeneration);
        const auto fallback = RestoreLightingScenarioSources(reopened, true);
        Check(fallback["Entities"][0]["Canis::Model"]["ModelAsset"]["path"].as<std::string>() ==
                  source.string(),
              "Missing generated caches did not restore the authored source model");
        Check(sourceFingerprint == LightingScenarioSourceFingerprint(reopened),
              "Ignored bake cache availability changed the authored source fingerprint");
        std::filesystem::rename(unavailableGeneration, savedGeneration);
        auto movedDynamic = YAML::Clone(scene);
        auto dynamic = YAML::Clone(floor);
        dynamic["Entity"] = 99;
        dynamic["Canis::Model"]["static"] = false;
        dynamic["Canis::Transform"]["position"] = Vector3(42, 8, 2);
        movedDynamic["Entities"].push_back(dynamic);
        Check(sourceFingerprint == LightingScenarioSourceFingerprint(movedDynamic),
              "A moving object invalidated static lighting");
        auto changedGeometry = loaded;
        changedGeometry.primitives[0].vertices[0].position.x -= .1f;
        Pipeline::WriteGltf(changedGeometry, source);
        Check(sourceFingerprint != LightingScenarioSourceFingerprint(scene),
              "Source buffer edits did not invalidate scenario lighting");
        Pipeline::WriteGltf(loaded, source);
        auto nextDefinitions = scenarios.scenarios;
        nextDefinitions[1].ambientIntensity = .4f;
        options.selectedScenarioIds = {night.id};
        const auto selected =
            BakeLightingScenarios(scenarios.scene, directory / "scenarios", nextDefinitions, options);
        const auto selectedGeometry = Pipeline::ReadGltf(selected.models[0]);
        const auto& selectedPrimitive = selectedGeometry.json["meshes"][0]["primitives"][0];
        Check(
            Pipeline::Attribute(scenarioGeometry,
                                scenarioPrimitive["extras"]["canisLightingScenarios"][dusk.id].get<int>()) ==
                Pipeline::Attribute(
                    selectedGeometry,
                    selectedPrimitive["extras"]["canisLightingScenarios"][dusk.id].get<int>()),
            "Bake selected did not retain the other valid lighting channel");
        Check(selected.scenarios[1].settingsFingerprint ==
                  LightingScenarioSettingsFingerprint(nextDefinitions[1]),
              "Selected bake did not update the scenario settings fingerprint");
        auto manifestBytes = [](const std::string& path)
        {
            std::ifstream f(path, std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(f), {});
        };
        const auto lastGoodManifest = manifestBytes(selected.manifestPath);
        std::atomic<bool> scenarioCancel{false};
        options.cancel = &scenarioCancel;
        bool scenarioCanceled = false;
        try
        {
            BakeLightingScenarios(selected.scene, directory / "scenarios", selected.scenarios, options,
                                  [&](const std::string&) { scenarioCancel = true; });
        }
        catch (const std::exception&)
        {
            scenarioCanceled = true;
        }
        Check(scenarioCanceled && manifestBytes(selected.manifestPath) == lastGoodManifest &&
                  std::filesystem::exists(selected.models[0]),
              "Canceled scenario bake damaged the last successful generation");
        options.cancel = nullptr;
        auto invalidDefinitions = selected.scenarios;
        invalidDefinitions[1].sunEnabled = true;
        invalidDefinitions[1].sunDirection = Vector3(0);
        bool scenarioFailed = false;
        try
        {
            BakeLightingScenarios(selected.scene, directory / "scenarios", invalidDefinitions, options);
        }
        catch (const std::exception&)
        {
            scenarioFailed = true;
        }
        Check(scenarioFailed && manifestBytes(selected.manifestPath) == lastGoodManifest,
              "Failed scenario bake replaced the last good manifest");
        options.vertexSpacing = .75f;
        auto changedLayout =
            BakeLightingScenarios(selected.scene, directory / "changed-layout", selected.scenarios, options);
        Check(changedLayout.scenarios[0].manifestPath.empty(),
              "Changed bake sampling incorrectly retained an incompatible scenario");
        options.vertexSpacing = .5f;
        options.selectedScenarioIds.clear();
        options.cancel = new std::atomic<bool>(true);
        bool canceled = false;
        try
        {
            BakeSceneLighting(scene, directory / "canceled", options);
        }
        catch (...)
        {
            canceled = true;
        }
        delete options.cancel;
        options.cancel = nullptr;
        Check(canceled, "Native lighting cancel request ignored");
        VisibilityBakeOptions visibility;
        visibility.minimum = {-1, -1, -1};
        visibility.dimensions = {1, 1, 1};
        BakeStaticVisibility(source, directory / "visibility.gltf", visibility);
        Check(std::filesystem::exists(directory / "visibility.gltf.visibility"),
              "Native visibility sidecar missing");
        std::filesystem::remove_all(directory);
        std::cout << "CANIS_NATIVE_BAKE_PASS lighting, color/geometry roundtrip, "
                     "cancellation and "
                     "conservative visibility\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
