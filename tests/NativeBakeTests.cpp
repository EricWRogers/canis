#include <Canis/AssetPipeline.hpp>
#include <Canis/LightingBake.hpp>
#include <Canis/VisibilityBake.hpp>
#include <Canis/UnityImport.hpp>
#include <Canis/Yaml.hpp>
#include <stb_image.h>
#include <iostream>
#include <fstream>
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
        Check(sky.size()==6 && sky["top"]["rotation"].as<int>(0)==180 &&
                  sky["bottom"]["rotation"].as<int>(0)==180 && sky["right"]["rotation"].as<int>(0)==0,
              "Unity skybox pole orientation does not match the converted side faces");
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
            Check(model.primitives.size()==2 && model.primitives[0].material==0 &&
                      model.primitives[1].material==1,
                  "Unity FBX submesh material slots were lost");
            bool firstTriangle=false, secondTriangle=false;
            for (auto v : model.primitives[0].vertices)
                firstTriangle |= glm::distance(v.position,Vector3(-6,-6,0))<.001f;
            for (auto v : model.primitives[1].vertices)
                secondTriangle |= glm::distance(v.position,Vector3(0,0,0))<.001f;
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
                  "FBX mirror and prefab rotation placed the floor outside its authored tile");
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
        auto imageFixture=Pipeline::ReadGltf(source);
        imageFixture.json["images"]=Pipeline::Json::array({{{"uri","source.png"}}});
        std::filesystem::copy_file(fixtures/"dark.png",directory/"source.png");
        auto relocated=Pipeline::Flatten(imageFixture);
        const auto relocatedPath=directory/"relocated"/"mesh.gltf";
        Pipeline::WriteGltf(relocated,relocatedPath);
        const auto relocatedJson=Pipeline::ReadGltf(relocatedPath).json;
        Check(std::filesystem::exists(relocatedPath.parent_path()/relocatedJson["images"][0]["uri"].get<std::string>()),
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
        Check(canceled, "Native lighting cancel request ignored");
        VisibilityBakeOptions visibility;
        visibility.minimum = {-1, -1, -1};
        visibility.dimensions = {1, 1, 1};
        BakeStaticVisibility(source, directory / "visibility.gltf", visibility);
        Check(std::filesystem::exists(directory / "visibility.gltf.visibility"),
              "Native visibility sidecar missing");
        std::filesystem::remove_all(directory);
        std::cout << "CANIS_NATIVE_BAKE_PASS lighting, color/geometry roundtrip, cancellation and "
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
