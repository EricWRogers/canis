#include <Canis/AssetManager.hpp>
#include <Canis/AssetPipeline.hpp>
#include <Canis/LightingProbe.hpp>
#include <Canis/LightingScenario.hpp>
#include <Canis/Yaml.hpp>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace Canis
{
    YAML::Node SerializeLightingScenario(const LightingScenario& s)
    {
        YAML::Node n;
        n["Id"] = s.id;
        n["Name"] = s.name;
        n["AmbientLight"] = s.ambientLight;
        n["AmbientIntensity"] = s.ambientIntensity;
        n["SunEnabled"] = s.sunEnabled;
        n["SunDirection"] = s.sunDirection;
        n["SunColor"] = s.sunColor;
        n["SunIntensity"] = s.sunIntensity;
        n["SkyboxUUID"] = s.skyboxUUID;
        n["SkyboxPath"] = s.skyboxPath;
        n["SkyboxIntensity"] = s.skyboxIntensity;
        n["ManifestPath"] = s.manifestPath;
        n["SourceFingerprint"] = s.sourceFingerprint;
        n["SettingsFingerprint"] = s.settingsFingerprint;
        n["ProbeCachePaths"] = s.probeCachePaths;
        return n;
    }
    LightingScenario DeserializeLightingScenario(const YAML::Node& n)
    {
        LightingScenario s;
        if (!n || !n.IsMap())
            return s;
        s.id = n["Id"].as<std::string>("");
        s.name = n["Name"].as<std::string>("Lighting scenario");
        s.ambientLight = n["AmbientLight"].as<Vector4>(s.ambientLight);
        s.ambientIntensity = std::max(0.f, n["AmbientIntensity"].as<float>(s.ambientIntensity));
        s.sunEnabled = n["SunEnabled"].as<bool>(s.sunEnabled);
        s.sunDirection = n["SunDirection"].as<Vector3>(s.sunDirection);
        s.sunColor = n["SunColor"].as<Vector4>(s.sunColor);
        s.sunIntensity = std::max(0.f, n["SunIntensity"].as<float>(s.sunIntensity));
        s.skyboxUUID = n["SkyboxUUID"].as<uint64_t>(0);
        s.skyboxPath = n["SkyboxPath"].as<std::string>("");
        s.skyboxIntensity = std::max(0.f, n["SkyboxIntensity"].as<float>(1));
        s.manifestPath = n["ManifestPath"].as<std::string>("");
        s.sourceFingerprint = n["SourceFingerprint"].as<std::string>("");
        s.settingsFingerprint = n["SettingsFingerprint"].as<std::string>("");
        s.probeCachePaths = n["ProbeCachePaths"].as<std::vector<std::string>>(std::vector<std::string>{});
        return s;
    }
    namespace
    {
        // FNV-1a over canonical data and file contents: timestamps alone miss external
        // edits.
        struct Fingerprint
        {
            uint64_t hash = 1469598103934665603ULL;
            std::set<std::string> files;
            void Add(const std::string& value)
            {
                for (unsigned char byte : value)
                {
                    hash ^= byte;
                    hash *= 1099511628211ULL;
                }
                hash ^= 0xff;
                hash *= 1099511628211ULL;
            }
            std::string Result() const
            {
                std::ostringstream result;
                result << std::hex << std::setfill('0') << std::setw(16) << hash;
                return result.str();
            }
            void File(std::filesystem::path path)
            {
                path = path.lexically_normal();
                if (!files.insert(path.generic_string()).second)
                    return;
                Add(path.generic_string());
                std::ifstream stream(path, std::ios::binary);
                if (!stream)
                {
                    Add("missing");
                    return;
                }
                char block[16384];
                while (stream)
                {
                    stream.read(block, sizeof block);
                    for (std::streamsize i = 0; i < stream.gcount(); ++i)
                    {
                        hash ^= static_cast<unsigned char>(block[i]);
                        hash *= 1099511628211ULL;
                    }
                }
                if (path.extension() == ".gltf")
                {
                    const auto gltf = Pipeline::ReadGltf(path);
                    for (const char* category : {"buffers", "images"})
                        if (gltf.json.contains(category))
                            for (const auto& item : gltf.json[category])
                                if (item.contains("uri"))
                                {
                                    auto uri = item["uri"].get<std::string>();
                                    if (!uri.starts_with("data:"))
                                        File(path.parent_path() / uri);
                                }
                }
                else if (path.extension() == ".material" || path.extension() == ".skybox")
                    References(YAML::LoadFile(path.string()));
            }
            void References(const YAML::Node& n)
            {
                if (!n)
                    return;
                if (n.IsMap())
                {
                    if (n["path"] && n["path"].IsScalar())
                        File(n["path"].as<std::string>());
                    else if (n["uuid"] && n["uuid"].IsScalar())
                    {
                        const auto p = AssetManager::GetPath(UUID(n["uuid"].as<uint64_t>()));
                        if (p != "Path was not found in AssetLibrary")
                            File(p);
                    }
                    for (const auto& item : n)
                        References(item.second);
                }
                else if (n.IsSequence())
                    for (const auto& item : n)
                        References(item);
            }
        };
        std::string CanonicalYaml(const YAML::Node& n, const std::string& field = "")
        {
            if (!n || n.IsNull())
                return "null;";
            if (n.IsMap())
            {
                std::map<std::string, YAML::Node> sorted;
                for (const auto& item : n)
                    sorted[item.first.as<std::string>()] = item.second;
                std::string result = "{";
                for (const auto& [key, value] : sorted)
                    result += std::to_string(key.size()) + ":" + key + CanonicalYaml(value, key);
                return result + "}";
            }
            if (n.IsSequence())
            {
                std::string result = "[";
                for (const auto& item : n)
                    result += CanonicalYaml(item, field);
                return result + "]";
            }
            const auto text = n.Scalar();
            if (text == "true" || text == "True" || text == "TRUE")
                return "true;";
            if (text == "false" || text == "False" || text == "FALSE")
                return "false;";
            // Numeric ID strings must retain all 64 bits; physical values are engine floats.
            if (field != "Entity" && field != "parent" && field != "uuid" && field != "Id" &&
                field != "SkyboxUUID" && !text.empty())
            {
                char* end = nullptr;
                float number = std::strtof(text.c_str(), &end);
                if (end == text.c_str() + text.size() && std::isfinite(number))
                {
                    if (number == 0)
                        number = 0; // Normalize negative zero after Euler conversion.
                    std::ostringstream scalar;
                    scalar << std::hexfloat << number;
                    return "float:" + scalar.str() + ";";
                }
            }
            return "text:" + std::to_string(text.size()) + ":" + text + ";";
        }
        YAML::Node CanonicalAsset(const YAML::Node& asset)
        {
            if (!asset || asset.IsNull())
                return YAML::Node();
            if (asset.IsScalar())
            {
                YAML::Node result;
                result["path"] =
                    std::filesystem::path(asset.as<std::string>()).lexically_normal().generic_string();
                return result;
            }
            std::string path = asset["path"].as<std::string>("");
            if (path.empty() && asset["uuid"])
            {
                path = AssetManager::GetPath(UUID(asset["uuid"].as<uint64_t>()));
                if (path == "Path was not found in AssetLibrary")
                    path.clear();
            }
            YAML::Node result;
            if (!path.empty())
                result["path"] = std::filesystem::path(path).lexically_normal().generic_string();
            else if (asset["uuid"])
                result["uuid"] = asset["uuid"].as<uint64_t>();
            return result;
        }
        YAML::Node CanonicalTransform(const YAML::Node& source)
        {
            YAML::Node result;
            result["active"] = source["active"].as<bool>(true);
            result["parent"] = source["parent"].as<uint64_t>(0);
            const auto matrix = glm::translate(Matrix4(1), source["position"].as<Vector3>(Vector3(0))) *
                                glm::mat4_cast(source["rotation"].as<Quaternion>(Quaternion(1, 0, 0, 0))) *
                                glm::scale(Matrix4(1), source["scale"].as<Vector3>(Vector3(1)));
            for (int c = 0; c < 4; ++c)
                for (int r = 0; r < 4; ++r)
                    // Quaternion/Euler save roundtrips differ by a few float ULPs. This is 0.01 mm
                    // for translation; source geometry/buffer bytes are still hashed exactly.
                    result["matrix"].push_back(float(std::round(double(matrix[c][r]) * 100000.0) / 100000.0));
            return result;
        }
        YAML::Node CanonicalUniforms(const YAML::Node& source)
        {
            YAML::Node result(YAML::NodeType::Map);
            if (!source || !source.IsMap())
                return result;
            for (const auto& entry : source)
            {
                const auto uniform = static_cast<const YAML::Node&>(entry.second);
                std::string type = uniform.IsMap() ? uniform["type"].as<std::string>("") : "";
                YAML::Node value;
                if (uniform.IsMap())
                {
                    if (uniform["value"])
                        value = uniform["value"];
                    else if (uniform["data"])
                        value = uniform["data"];
                    else if (uniform["texture"])
                        value = uniform["texture"];
                    else
                        value = uniform;
                }
                else
                    value = uniform;
                if (type == "vec2")
                    type = "vector2";
                if (type == "vec3")
                    type = "vector3";
                if (type == "vec4")
                    type = "vector4";
                if (type.empty())
                {
                    if (value.IsScalar())
                        type = "float";
                    else if (value.IsSequence() && value.size() >= 2 && value.size() <= 4)
                        type = "vector" + std::to_string(value.size());
                    else if (value.IsMap() && (value["path"] || value["uuid"]))
                        type = "texture";
                }
                if (type.empty())
                    continue;
                auto output = result[entry.first.as<std::string>()];
                output["type"] = type;
                output["value"] = type == "texture" ? CanonicalAsset(value) : YAML::Clone(value);
            }
            return result;
        }
        bool ModelFilesAvailable(const std::filesystem::path& path)
        {
            if (!std::filesystem::is_regular_file(path))
                return false;
            if (path.extension() != ".gltf")
                return true;
            try
            {
                std::ifstream file(path);
                const auto document = Pipeline::Json::parse(file);
                if (document.contains("buffers"))
                    for (const auto& buffer : document["buffers"])
                        if (buffer.contains("uri"))
                        {
                            const auto uri = buffer["uri"].get<std::string>();
                            if (!uri.starts_with("data:") &&
                                !std::filesystem::is_regular_file(path.parent_path() / uri))
                                return false;
                        }
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
        YAML::Node ScenarioAuthoringNode(const LightingScenario& scenario)
        {
            auto n = SerializeLightingScenario(scenario);
            for (const char* key :
                 {"Name", "ManifestPath", "SourceFingerprint", "SettingsFingerprint", "ProbeCachePaths"})
                n.remove(key);
            return n;
        }
    } // namespace
    std::string LightingScenarioAuthoringFingerprint(const LightingScenario& scenario)
    {
        Fingerprint fingerprint;
        fingerprint.Add(CanonicalYaml(ScenarioAuthoringNode(scenario)));
        return fingerprint.Result();
    }
    std::string LightingScenarioSettingsFingerprint(const LightingScenario& scenario)
    {
        Fingerprint fingerprint;
        fingerprint.Add(CanonicalYaml(ScenarioAuthoringNode(scenario)));
        if (!scenario.skyboxPath.empty())
            fingerprint.File(scenario.skyboxPath);
        else if (scenario.skyboxUUID)
        {
            const auto path = AssetManager::GetPath(UUID(scenario.skyboxUUID));
            if (path != "Path was not found in AssetLibrary")
                fingerprint.File(path);
        }
        return fingerprint.Result();
    }
    std::string LightingScenarioSourceFingerprint(const YAML::Node& input)
    {
        const auto restored = RestoreLightingScenarioSources(input);
        Fingerprint fingerprint;
        fingerprint.Add("Canis lighting scenarios geometry v1");
        std::map<uint64_t, YAML::Node> nodes;
        for (const auto& n : restored["Entities"])
            nodes[n["Entity"].as<uint64_t>()] = static_cast<const YAML::Node&>(n);
        std::set<uint64_t> parents;
        std::vector<YAML::Node> selected;
        for (const auto& [id, node] : nodes)
        {
            const auto model = node["Canis::Model"];
            const bool isStatic =
                model && model["static"].as<bool>(false) && !node["Canis::ModelAnimation"] &&
                (!node["Canis::Rigidbody"] || node["Canis::Rigidbody"]["motionType"].as<int>(0) == 0);
            YAML::Node item;
            item["Entity"] = id;
            if (isStatic)
            {
                // Explicit defaults and UUID/path aliases must hash identically after
                // save/reopen.
                auto canonicalModel = item["Canis::Model"];
                canonicalModel["static"] = true;
                canonicalModel["ModelAsset"] = CanonicalAsset(model["ModelAsset"]);
                canonicalModel["nodeIndex"] = model["nodeIndex"].as<int>(-1);
                canonicalModel["applyNodeTransform"] = model["applyNodeTransform"].as<bool>(true);
                canonicalModel["color"] = model["color"].as<Vector4>(Vector4(1));
                canonicalModel["castShadow"] = model["castShadow"].as<bool>(true);
                canonicalModel["bakedVisibility"] = model["bakedVisibility"].as<bool>(false);
                if (const auto material = node["Canis::Material"])
                {
                    auto canonical = item["Canis::Material"];
                    canonical["color"] = material["color"].as<Vector4>(Vector4(1));
                    if (material["MaterialAsset"])
                        canonical["MaterialAsset"] = CanonicalAsset(material["MaterialAsset"]);
                    if (const auto slots = material["MaterialSlots"]; slots && slots.size())
                        for (const auto& slot : slots)
                            canonical["MaterialSlots"].push_back(CanonicalAsset(slot));
                    if (const auto overrides = material["UniformOverrides"]; overrides && overrides.size())
                        canonical["UniformOverrides"] = CanonicalUniforms(overrides);
                }
            }
            bool hasLight = false;
            for (const char* key : {"Canis::PointLight", "Canis::SpotLight"})
                if (node[key] && node[key]["mode"].as<int>(1) != 0)
                {
                    const auto source = node[key];
                    auto light = item[key];
                    light["mode"] = source["mode"].as<int>(1);
                    light["enabled"] = source["enabled"].as<bool>(true);
                    if (const auto color = source["color"]; color && color.IsSequence() && color.size() >= 3)
                        light["color"] =
                            Vector3(color[0].as<float>(), color[1].as<float>(), color[2].as<float>());
                    else
                        light["color"] = Vector3(1);
                    light["intensity"] = source["intensity"].as<float>(1);
                    light["range"] =
                        source["range"].as<float>(std::string(key) == "Canis::SpotLight" ? 35 : 10);
                    if (std::string(key) == "Canis::SpotLight")
                    {
                        light["spotAngle"] = source["spotAngle"].as<float>(55);
                        light["innerAngle"] = source["innerAngle"].as<float>(35);
                    }
                    hasLight = true;
                }
            if (hasLight && node["Canis::BakeLight"])
            {
                auto extra = item["Canis::BakeLight"];
                const auto source = node["Canis::BakeLight"];
                extra["type"] = source["type"].as<int>(2);
                extra["spotAngle"] = source["spotAngle"].as<float>(30);
                extra["innerAngle"] = source["innerAngle"].as<float>(20);
                extra["enabled"] = source["enabled"].as<bool>(true);
            }
            if (!isStatic && !hasLight)
                continue;
            item["Active"] = node["Active"].as<bool>(true);
            item["Canis::Transform"] = CanonicalTransform(node["Canis::Transform"]);
            selected.push_back(item);
            uint64_t parent = node["Canis::Transform"]["parent"].as<uint64_t>(0);
            int depth = 0;
            while (parent && nodes.contains(parent) && depth++ < 128)
            {
                parents.insert(parent);
                parent = nodes.at(parent)["Canis::Transform"]["parent"].as<uint64_t>(0);
            }
            if (depth >= 128)
                throw std::runtime_error("Bake transform cycle");
        }
        for (auto id : parents)
        {
            YAML::Node parent;
            parent["Entity"] = id;
            parent["Active"] = nodes.at(id)["Active"].as<bool>(true);
            parent["Canis::Transform"] = CanonicalTransform(nodes.at(id)["Canis::Transform"]);
            fingerprint.Add(CanonicalYaml(parent));
        }
        for (const auto& item : selected)
        {
            fingerprint.Add(CanonicalYaml(item));
            fingerprint.References(item);
        }
        if (const auto probes = input["Environment"]["LightingProbes"])
            for (const auto& probe : probes)
            {
                LightingProbe defaults;
                defaults.enabled = probe["Enabled"].as<bool>(true);
                defaults.position = probe["Position"].as<Vector3>(defaults.position);
                defaults.halfExtents = probe["HalfExtents"].as<Vector3>(defaults.halfExtents);
                defaults.blendDistance = probe["BlendDistance"].as<float>(defaults.blendDistance);
                defaults.diffuseIntensity = probe["DiffuseIntensity"].as<float>(defaults.diffuseIntensity);
                defaults.specularIntensity = probe["SpecularIntensity"].as<float>(defaults.specularIntensity);
                defaults.nearClip = probe["NearClip"].as<float>(defaults.nearClip);
                defaults.farClip = probe["FarClip"].as<float>(defaults.farClip);
                defaults.Sanitize();
                YAML::Node copy;
                copy["Enabled"] = defaults.enabled;
                copy["Position"] = defaults.position;
                copy["HalfExtents"] = defaults.halfExtents;
                copy["BlendDistance"] = defaults.blendDistance;
                copy["DiffuseIntensity"] = defaults.diffuseIntensity;
                copy["SpecularIntensity"] = defaults.specularIntensity;
                copy["NearClip"] = defaults.nearClip;
                copy["FarClip"] = defaults.farClip;
                fingerprint.Add(CanonicalYaml(copy));
            }
        return fingerprint.Result();
    }
    YAML::Node RestoreLightingScenarioSources(const YAML::Node& input, bool missingOnly)
    {
        auto result = YAML::Clone(input);
        std::map<uint64_t, YAML::Node> sources;
        std::map<uint64_t, std::string> generatedPaths;
        std::map<uint64_t, uint64_t> generatedUUIDs;
        const auto record = [&](const YAML::Node& models)
        {
            if (!models || !models.IsSequence())
                return;
            for (const auto& model : models)
                if (model["Source"])
                {
                    const auto id = model["Entity"].as<uint64_t>();
                    sources[id] = model["Source"];
                    generatedPaths[id] = model["Path"].as<std::string>("");
                    generatedUUIDs[id] =
                        model["GeneratedUUID"].as<uint64_t>(Pipeline::AssetUUID(generatedPaths[id]));
                }
        };
        record(input["Environment"]["LightingScenarioSources"]);
        const auto scenarios = input["Environment"]["LightingScenarios"];
        if (sources.empty() && scenarios && scenarios.IsSequence())
            for (const auto& scenario : scenarios)
            {
                const auto path = scenario["ManifestPath"].as<std::string>("");
                if (path.empty() || !std::filesystem::exists(path))
                    continue;
                const auto manifest = YAML::LoadFile(path);
                record(manifest["Models"]);
                break;
            }
        for (auto node : result["Entities"])
        {
            auto model = node["Canis::Model"];
            if (!model)
                continue;
            YAML::Node source;
            if (model["LightingScenarioSource"])
            {
                const auto currentPath = model["ModelAsset"]["path"].as<std::string>("");
                if (missingOnly && !currentPath.empty() && ModelFilesAvailable(currentPath))
                    continue;
                source = model["LightingScenarioSource"];
            }
            else
            {
                const auto id = node["Entity"].as<uint64_t>();
                std::string currentPath = model["ModelAsset"]["path"].as<std::string>("");
                if (currentPath.empty() && model["ModelAsset"]["uuid"])
                    currentPath = AssetManager::GetPath(UUID(model["ModelAsset"]["uuid"].as<uint64_t>()));
                if (sources.contains(id))
                {
                    const auto uuid = model["ModelAsset"]["uuid"].as<uint64_t>(0);
                    const bool generated =
                        std::filesystem::path(currentPath).lexically_normal() ==
                            std::filesystem::path(generatedPaths[id]).lexically_normal() ||
                        ((currentPath.empty() || currentPath == "Path was not found in AssetLibrary") &&
                         uuid == generatedUUIDs[id]);
                    if (generated && (!missingOnly || !ModelFilesAvailable(generatedPaths[id])))
                        source = sources.at(id);
                }
            }
            if (!source || !source["ModelAsset"])
                continue;
            model["ModelAsset"] = YAML::Clone(source["ModelAsset"]);
            model["nodeIndex"] = source["nodeIndex"].as<int>(-1);
            model["applyNodeTransform"] = source["applyNodeTransform"].as<bool>(true);
            model.remove("LightingScenarioSource");
        }
        return result;
    }
    bool LightingScenarioDataAvailable(const LightingScenario& scenario, std::string& error,
                                       bool requireProbes)
    {
        error.clear();
        const auto unavailable = [&](const std::string& reason)
        {
            error = reason;
            return false;
        };
        try
        {
            if (scenario.id.empty() || scenario.manifestPath.empty() ||
                !std::filesystem::is_regular_file(scenario.manifestPath))
                return unavailable("Not baked. Bake this lighting scenario.");
            if (scenario.settingsFingerprint.empty() ||
                scenario.settingsFingerprint != LightingScenarioSettingsFingerprint(scenario))
                return unavailable("Lighting settings changed. Bake this scenario again.");
            const auto manifest = YAML::LoadFile(scenario.manifestPath);
            if (manifest["Version"].as<int>(0) != 1 || scenario.sourceFingerprint.empty() ||
                manifest["SourceFingerprint"].as<std::string>("") != scenario.sourceFingerprint)
                return unavailable("Baked lighting metadata does not match this scenario. Bake again.");
            bool listed = false;
            if (const auto definitions = manifest["Scenarios"]; definitions && definitions.IsSequence())
                for (const auto& definition : definitions)
                    if (definition["Id"].as<std::string>("") == scenario.id &&
                        definition["SettingsFingerprint"].as<std::string>("") ==
                            scenario.settingsFingerprint &&
                        definition["SourceFingerprint"].as<std::string>("") == scenario.sourceFingerprint)
                        listed = true;
            if (!listed)
                return unavailable("This scenario has no channel in the saved bake. Bake it again.");
            const auto models = manifest["Models"];
            if (!models || !models.IsSequence() || models.size() == 0)
                return unavailable("Shared baked geometry is missing. Bake the scene again.");
            for (const auto& model : models)
            {
                const auto path = model["Path"].as<std::string>("");
                if (path.empty() || !std::filesystem::is_regular_file(path))
                    return unavailable("Shared baked geometry is missing: " + path);
                if (std::filesystem::path(path).extension() == ".gltf")
                {
                    std::ifstream input(path);
                    const auto json = Pipeline::Json::parse(input);
                    if (json.contains("buffers"))
                        for (const auto& buffer : json["buffers"])
                            if (buffer.contains("uri"))
                            {
                                const auto uri = buffer["uri"].get<std::string>();
                                if (!uri.starts_with("data:") &&
                                    !std::filesystem::is_regular_file(
                                        std::filesystem::path(path).parent_path() / uri))
                                    return unavailable(
                                        "Shared baked geometry buffer is missing. Bake the scene again.");
                            }
                }
            }
            if (requireProbes)
            {
                const auto count = manifest["ProbeCount"].as<size_t>(scenario.probeCachePaths.size());
                const auto enabled = manifest["ProbeEnabled"];
                for (size_t i = 0; i < count; ++i)
                {
                    if (enabled && i < enabled.size() && !enabled[i].as<bool>(true))
                        continue;
                    if (i >= scenario.probeCachePaths.size() || scenario.probeCachePaths[i].empty() ||
                        !std::filesystem::is_regular_file(scenario.probeCachePaths[i]))
                        return unavailable(
                            "Reflection probes are not prepared. Prepare lighting scenarios before play.");
                }
            }
            return true;
        }
        catch (const std::exception& exception)
        {
            return unavailable("Cannot read baked lighting data: " + std::string(exception.what()));
        }
    }
} // namespace Canis
