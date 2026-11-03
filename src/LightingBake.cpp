#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/AssetPipeline.hpp>
#include <Canis/Components.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/LightingBake.hpp>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <numeric>
#include <set>
#include <stb_image.h>
#include <thread>
#include <unordered_map>
namespace Canis
{
    namespace
    {
        using Pipeline::Json;
        struct Triangle
        {
            Vector3 a, b, c, normal, albedo{.5f}, emission{0}, minimum, maximum;
        };
        struct RayHit
        {
            int triangle = -1;
            float distance = FLT_MAX;
            Vector3 point{0};
        };
        class BakeBVH
        {
            struct Node
            {
                Vector3 low, high;
                int left = -1, right = -1, start = 0, count = 0;
            };
            std::vector<Node> nodes;
            std::vector<int> order;
            int Build(int start, int count)
            {
                Node n;
                n.low = Vector3(FLT_MAX);
                n.high = Vector3(-FLT_MAX);
                n.start = start;
                n.count = count;
                for (int i = start; i < start + count; i++)
                {
                    const auto& t = triangles[order[i]];
                    n.low = glm::min(n.low, t.minimum);
                    n.high = glm::max(n.high, t.maximum);
                }
                int id = nodes.size();
                nodes.push_back(n);
                if (count <= 8)
                    return id;
                auto size = n.high - n.low;
                int axis = size.x > size.y && size.x > size.z ? 0 : size.y > size.z ? 1 : 2;
                int mid = start + count / 2;
                std::nth_element(order.begin() + start, order.begin() + mid, order.begin() + start + count,
                                 [&](int a, int b)
                                 {
                                     return triangles[a].minimum[axis] + triangles[a].maximum[axis] <
                                            triangles[b].minimum[axis] + triangles[b].maximum[axis];
                                 });
                nodes[id].left = Build(start, count / 2);
                nodes[id].right = Build(mid, count - count / 2);
                nodes[id].count = 0;
                return id;
            }
            static bool Box(Vector3 low, Vector3 high, Vector3 origin, Vector3 direction, float distance)
            {
                float near = 0, far = distance;
                for (int a = 0; a < 3; a++)
                {
                    if (std::abs(direction[a]) < 1e-10f)
                    {
                        if (origin[a] < low[a] || origin[a] > high[a])
                            return false;
                        continue;
                    }
                    float x = (low[a] - origin[a]) / direction[a], y = (high[a] - origin[a]) / direction[a];
                    if (x > y)
                        std::swap(x, y);
                    near = std::max(near, x);
                    far = std::min(far, y);
                    if (near > far)
                        return false;
                }
                return true;
            }

          public:
            std::vector<Triangle> triangles;
            void Ready()
            {
                order.resize(triangles.size());
                std::iota(order.begin(), order.end(), 0);
                nodes.reserve(triangles.size() / 4 + 1);
                if (!order.empty())
                    Build(0, order.size());
            }
            RayHit Cast(Vector3 origin, Vector3 direction, float distance) const
            {
                RayHit hit;
                hit.distance = distance;
                if (nodes.empty())
                    return hit;
                int stack[128], top = 0;
                stack[top++] = 0;
                while (top)
                {
                    const auto& n = nodes[stack[--top]];
                    if (!Box(n.low, n.high, origin, direction, hit.distance))
                        continue;
                    if (!n.count)
                    {
                        stack[top++] = n.left;
                        stack[top++] = n.right;
                        continue;
                    }
                    for (int i = n.start; i < n.start + n.count; i++)
                    {
                        int id = order[i];
                        auto& t = triangles[id];
                        auto e1 = t.b - t.a, e2 = t.c - t.a, p = glm::cross(direction, e2);
                        float det = glm::dot(e1, p);
                        if (std::abs(det) < 1e-8f)
                            continue;
                        float inv = 1 / det;
                        auto s = origin - t.a;
                        float u = glm::dot(s, p) * inv;
                        if (u < 0 || u > 1)
                            continue;
                        auto q = glm::cross(s, e1);
                        float v = glm::dot(direction, q) * inv;
                        if (v < 0 || u + v > 1)
                            continue;
                        float d = glm::dot(e2, q) * inv;
                        if (d > .002f && d < hit.distance)
                        {
                            hit.triangle = id;
                            hit.distance = d;
                            hit.point = origin + direction * d;
                        }
                    }
                }
                return hit;
            }
        };
        struct Light
        {
            Vector3 position{0}, direction{0, -1, 0}, color{1};
            float intensity = 1, range = 10, outer = 0, inner = 0;
            int type = 2;
        };
        struct Instance
        {
            uint64_t id;
            Pipeline::Mesh mesh;
            Matrix4 world{1};
            YAML::Node node;
        };
        static std::string Path(YAML::Node n)
        {
            if (n && n["path"])
                return n["path"].as<std::string>();
            if (n && n["uuid"])
            {
                auto p = AssetManager::GetPath(UUID(n["uuid"].as<uint64_t>()));
                if (p != "Path was not found in AssetLibrary")
                    return p;
            }
            return "";
        }
        static Vector3 Color3(YAML::Node n, Vector3 fallback = Vector3(1))
        {
            if (!n)
                return fallback;
            return {n[0].as<float>(), n[1].as<float>(), n[2].as<float>()};
        }
        static Vector3 MaterialColor(const Pipeline::Mesh& m, int id, bool emission,
                                     std::map<std::string, Vector3>& textureAverages)
        {
            if (id < 0 || id >= int(m.materials.size()))
                return emission ? Vector3(0) : Vector3(.5f);
            const auto& mat = m.materials[id];
            Vector3 c(emission ? 0.f : 1.f);
            auto factor = emission ? mat.value("emissiveFactor", Json::array({0, 0, 0}))
                                   : mat.value("pbrMetallicRoughness", Json::object())
                                         .value("baseColorFactor", Json::array({1, 1, 1, 1}));
            for (int i = 0; i < 3; i++)
                c[i] = factor[i];
            const auto surface = emission ? mat : mat.value("pbrMetallicRoughness", Json::object());
            const char* key = emission ? "emissiveTexture" : "baseColorTexture";
            if (surface.contains(key))
            {
                int texture = surface[key].value("index", -1);
                if (texture >= 0 && texture < int(m.textures.size()))
                {
                    int image = m.textures[texture].value("source", -1);
                    if (image >= 0 && image < int(m.images.size()) && m.images[image].contains("uri"))
                    {
                        std::string path = m.images[image]["uri"];
                        if (!textureAverages.contains(path))
                        {
                            int width, height, channels;
                            auto* pixels = stbi_load(path.c_str(), &width, &height, &channels, 3);
                            if (!pixels)
                                throw std::runtime_error("Cannot sample bake texture: " + path);
                            glm::dvec3 sum(0);
                            size_t count = size_t(width) * height;
                            for (size_t pixel = 0; pixel < count; pixel++)
                                for (int channel = 0; channel < 3; channel++)
                                    sum[channel] += std::pow(pixels[pixel * 3 + channel] / 255.0, 2.2);
                            stbi_image_free(pixels);
                            textureAverages[path] = Vector3(sum / double(count));
                        }
                        c *= textureAverages.at(path);
                    }
                }
            }
            return c;
        }
        static Vector3 Direct(Vector3 point, Vector3 normal, const std::vector<Light>& lights,
                              const BakeBVH& bvh)
        {
            Vector3 result(0);
            for (const auto& l : lights)
            {
                auto delta = l.type == 1 ? -l.direction : l.position - point;
                float distance = l.type == 1 ? 10000 : glm::length(delta);
                if (distance < .005f || (l.type != 1 && distance >= l.range))
                    continue;
                auto direction = delta / distance;
                if (l.type == 1)
                    direction = glm::normalize(delta);
                float cosine = std::max(0.f, glm::dot(normal, direction));
                if (cosine <= 0)
                    continue;
                float attenuation = l.type == 1 ? 1
                                                : std::pow(std::max(0.f, 1 - distance / l.range), 2) /
                                                      std::max(1.f, distance * distance);
                if (l.type == 0)
                {
                    float angle = glm::dot(l.direction, -direction);
                    attenuation *=
                        glm::clamp((angle - l.outer) / std::max(.001f, l.inner - l.outer), 0.f, 1.f);
                }
                if (attenuation <= 0)
                    continue;
                if (bvh.Cast(point + normal * .006f, direction, std::max(.001f, distance - .012f)).triangle <
                    0)
                    result += l.color * (l.intensity * attenuation * cosine);
            }
            return result;
        }
        static Pipeline::Vertex Mid(Pipeline::Vertex a, Pipeline::Vertex b)
        {
            Pipeline::Vertex v;
            v.position = (a.position + b.position) * .5f;
            auto n = a.normal + b.normal;
            v.normal = glm::length(n) > .00001f ? glm::normalize(n) : a.normal;
            v.uv = (a.uv + b.uv) * .5f;
            v.color = (a.color + b.color) * .5f;
            return v;
        }
        static void Subdivide(Pipeline::Primitive& out, Pipeline::Vertex a, Pipeline::Vertex b,
                              Pipeline::Vertex c, const Matrix4& world, float spacing, int depth = 0)
        {
            auto aw = Vector3(world * Vector4(a.position, 1)), bw = Vector3(world * Vector4(b.position, 1)),
                 cw = Vector3(world * Vector4(c.position, 1));
            float ab = glm::distance(aw, bw), bc = glm::distance(bw, cw), ca = glm::distance(cw, aw);
            if (std::max({ab, bc, ca}) > spacing && depth < 10)
            {
                if (ab >= bc && ab >= ca)
                {
                    auto m = Mid(a, b);
                    Subdivide(out, a, m, c, world, spacing, depth + 1);
                    Subdivide(out, m, b, c, world, spacing, depth + 1);
                }
                else if (bc >= ca)
                {
                    auto m = Mid(b, c);
                    Subdivide(out, a, b, m, world, spacing, depth + 1);
                    Subdivide(out, a, m, c, world, spacing, depth + 1);
                }
                else
                {
                    auto m = Mid(c, a);
                    Subdivide(out, a, b, m, world, spacing, depth + 1);
                    Subdivide(out, m, b, c, world, spacing, depth + 1);
                }
                return;
            }
            for (auto v : {a, b, c})
            {
                out.indices.push_back(out.vertices.size());
                out.vertices.push_back(v);
            }
        }
        // Subdivision emits triangle corners independently. Reuse identical
        // evaluated samples before writing so baking does not triple vertex
        // bandwidth. Include every scenario channel to retain lighting seams.
        static void CompactBakedSamples(Pipeline::Primitive& primitive,
                                        std::vector<std::vector<float>>& channels)
        {
            struct Hash
            {
                size_t operator()(const std::vector<float>& values) const
                {
                    size_t result = 0;
                    for (float value : values)
                        result ^= std::hash<float>{}(value) + 0x9e3779b9u + (result << 6) + (result >> 2);
                    return result;
                }
            };
            std::unordered_map<std::vector<float>, uint32_t, Hash> samples;
            std::vector<Pipeline::Vertex> compact;
            std::vector<std::vector<float>> compactChannels(channels.size());
            for (auto& index : primitive.indices)
            {
                const auto& v = primitive.vertices[index];
                std::vector<float> key{v.position.x,v.position.y,v.position.z,v.normal.x,v.normal.y,v.normal.z,
                                       v.uv.x,v.uv.y,v.color.x,v.color.y,v.color.z,v.color.w};
                for (const auto& channel : channels)
                    key.insert(key.end(), channel.begin()+index*3, channel.begin()+index*3+3);
                const auto [it, inserted] = samples.emplace(std::move(key), static_cast<uint32_t>(compact.size()));
                if (inserted)
                {
                    compact.push_back(v);
                    for (size_t c=0;c<channels.size();++c)
                        compactChannels[c].insert(compactChannels[c].end(), channels[c].begin()+index*3,
                                                  channels[c].begin()+index*3+3);
                }
                index = it->second;
            }
            primitive.vertices = std::move(compact);
            channels = std::move(compactChannels);
        }
        // Additional irradiance channels use accessors in the same buffer as the shared
        // geometry. They never duplicate positions, indices or collision data.
        static void WriteScenarioChannels(const std::filesystem::path& path,
                                          const std::vector<LightingScenario>& scenarios,
                                          const std::vector<std::vector<std::vector<float>>>& channels,
                                          const std::filesystem::path& previousPath,
                                          const std::vector<std::string>& preservedIds)
        {
            auto source = Pipeline::ReadGltf(path);
            auto& json = source.json;
            auto& binary = source.buffers.at(0);
            auto append = [&](size_t p, const std::string& id, const std::vector<float>& values)
            {
                while (binary.size() % 4)
                    binary.push_back(0);
                const size_t offset = binary.size();
                const auto* bytes = reinterpret_cast<const uint8_t*>(values.data());
                binary.insert(binary.end(), bytes, bytes + values.size() * sizeof(float));
                json["bufferViews"].push_back(
                    {{"buffer", 0}, {"byteOffset", offset}, {"byteLength", values.size() * sizeof(float)}});
                const size_t accessor = json["accessors"].size();
                json["accessors"].push_back({{"bufferView", json["bufferViews"].size() - 1},
                                             {"componentType", 5126},
                                             {"count", values.size() / 3},
                                             {"type", "VEC3"}});
                json["meshes"][0]["primitives"][p]["extras"]["canisLightingScenarios"][id] = accessor;
            };
            for (size_t p = 0; p < channels.size(); ++p)
                for (size_t s = 0; s < scenarios.size(); ++s)
                    append(p, scenarios[s].id, channels[p][s]);
            if (!preservedIds.empty())
            {
                const auto previous = Pipeline::ReadGltf(previousPath);
                const auto& oldPrimitives = previous.json["meshes"][0]["primitives"];
                if (oldPrimitives.size() != channels.size())
                    throw std::runtime_error("Saved scenario geometry changed; bake all scenarios");
                for (size_t p = 0; p < channels.size(); ++p)
                {
                    for (const char* attribute : {"POSITION", "NORMAL", "TEXCOORD_0"})
                        if (Pipeline::Attribute(previous,
                                                oldPrimitives[p]["attributes"][attribute].get<int>()) !=
                            Pipeline::Attribute(
                                source,
                                json["meshes"][0]["primitives"][p]["attributes"][attribute].get<int>()))
                            throw std::runtime_error("Saved scenario topology changed; bake all scenarios");
                    for (const auto& id : preservedIds)
                    {
                        const auto& extras = oldPrimitives[p]["extras"];
                        if (!extras.contains("canisLightingScenarios") ||
                            !extras["canisLightingScenarios"].contains(id))
                            throw std::runtime_error("Saved scenario channel is missing; bake all scenarios");
                        append(
                            p, id,
                            Pipeline::Attribute(previous, extras["canisLightingScenarios"][id].get<int>()));
                    }
                }
            }
            json["buffers"][0]["byteLength"] = binary.size();
            auto binaryPath = path.parent_path() / json["buffers"][0]["uri"].get<std::string>();
            std::ofstream data(binaryPath, std::ios::binary | std::ios::trunc);
            data.write(reinterpret_cast<const char*>(binary.data()), binary.size());
            std::ofstream metadata(path, std::ios::trunc);
            metadata << json.dump();
            data.close();
            metadata.close();
            if (!data || !metadata)
                throw std::runtime_error("Cannot write scenario channels: " + path.string());
        }
    } // namespace
    static LightingBakeResult BakeSceneLightingInternal(
        YAML::Node snapshot, const std::filesystem::path& directory, const LightingBakeOptions& options,
        const std::function<void(const std::string&)>& progress,
        const std::vector<LightingScenario>* scenarios = nullptr,
        const std::map<uint64_t, std::string>& previousPaths = {},
        const std::vector<std::string>& preservedIds = {})
    {
        if (options.cancel && options.cancel->load())
            throw std::runtime_error("Bake canceled");
        // Repeat bakes always start from authored geometry, never from the previous
        // subdivision.
        auto input = RestoreLightingScenarioSources(snapshot);
        LightingBakeResult result;
        result.scene = YAML::Clone(input);
        std::map<uint64_t, YAML::Node> nodes;
        std::map<uint64_t, Matrix4> worlds;
        for (auto n : input["Entities"])
            nodes[n["Entity"].as<uint64_t>()] = static_cast<const YAML::Node&>(n);
        auto world = [&](auto&& self, uint64_t id, int depth = 0) -> Matrix4
        {
            if (depth > 128)
                throw std::runtime_error("Bake transform cycle");
            if (worlds.contains(id))
                return worlds[id];
            auto t = nodes.at(id)["Canis::Transform"];
            auto matrix = glm::translate(Matrix4(1), t["position"].as<Vector3>(Vector3(0))) *
                          glm::mat4_cast(t["rotation"].as<Quaternion>(Quaternion(1, 0, 0, 0))) *
                          glm::scale(Matrix4(1), t["scale"].as<Vector3>(Vector3(1)));
            auto parent = t["parent"].as<uint64_t>(0);
            return worlds[id] = parent ? self(self, parent, depth + 1) * matrix : matrix;
        };
        std::vector<Instance> instances;
        std::vector<Light> lights;
        BakeBVH bvh;
        std::map<std::string, Pipeline::Mesh> meshCache;
        std::map<std::string, Vector3> textureAverages;
        auto active = [&](auto&& self, uint64_t id, int depth = 0) -> bool
        {
            if (depth > 128)
                throw std::runtime_error("Bake transform cycle");
            auto node = nodes.at(id);
            if (!node["Active"].as<bool>(true))
                return false;
            auto parent = node["Canis::Transform"]["parent"].as<uint64_t>(0);
            return !parent || self(self, parent, depth + 1);
        };
        for (auto [id, n] : nodes)
            if (active(active, id))
            {
                if (options.cancel && options.cancel->load())
                    throw std::runtime_error("Bake canceled");
                auto transform = world(world, id);
                if (auto l = n["Canis::PointLight"];
                    l && l["enabled"].as<bool>(true) &&
                    l["mode"].as<int>(LightMode::Mixed) != LightMode::Realtime)
                {
                    Light light;
                    light.position = Vector3(transform[3]);
                    light.color = Color3(l["color"]);
                    light.intensity = l["intensity"].as<float>(1);
                    light.range = l["range"].as<float>(10);
                    light.direction = glm::normalize(Vector3(transform * Vector4(0, 0, -1, 0)));
                    if (auto extra = n["Canis::BakeLight"])
                    {
                        light.type = extra["type"].as<int>(2);
                        light.outer = std::cos(glm::radians(extra["spotAngle"].as<float>(30) * .5f));
                        light.inner = std::cos(glm::radians(extra["innerAngle"].as<float>(20) * .5f));
                    }
                    lights.push_back(light);
                }
                if (auto l = n["Canis::DirectionalLight"];
                    l && l["enabled"].as<bool>(true) &&
                    l["mode"].as<int>(LightMode::Mixed) != LightMode::Realtime)
                {
                    Light light;
                    light.type = 1;
                    light.direction = l["direction"].as<Vector3>();
                    light.color = Color3(l["color"]);
                    light.intensity = l["intensity"].as<float>(1);
                    lights.push_back(light);
                }
                if (auto l = n["Canis::SpotLight"];
                    l && l["enabled"].as<bool>(true) &&
                    l["mode"].as<int>(LightMode::Mixed) != LightMode::Realtime)
                {
                    Light light;
                    light.type = 0;
                    light.position = Vector3(transform[3]);
                    light.direction = glm::normalize(Vector3(transform * Vector4(0, 0, -1, 0)));
                    light.color = Color3(l["color"]);
                    light.intensity = l["intensity"].as<float>(1);
                    light.range = l["range"].as<float>(35);
                    float outer = std::clamp(l["spotAngle"].as<float>(55), .1f, 179.f);
                    float inner = std::clamp(l["innerAngle"].as<float>(35), 0.f, outer);
                    light.outer = std::cos(glm::radians(outer * .5f));
                    light.inner = std::cos(glm::radians(inner * .5f));
                    lights.push_back(light);
                }
                auto model = n["Canis::Model"];
                if (!model || !model["static"].as<bool>(false) || n["Canis::ModelAnimation"])
                    continue;
                if (auto body = n["Canis::Rigidbody"]; body && body["motionType"].as<int>(0) != 0)
                    continue;
                std::string path = Path(model["ModelAsset"]);
                if (path.empty())
                    continue;
                auto key = path + "#" + std::to_string(model["nodeIndex"].as<int>(-1)) + "#" +
                           std::to_string(model["applyNodeTransform"].as<bool>(true));
                if (!meshCache.contains(key))
                    meshCache[key] = Pipeline::ReadMesh(path, model["nodeIndex"].as<int>(-1),
                                                       model["applyNodeTransform"].as<bool>(true));
                Instance instance{id, meshCache[key], transform, n};
                std::erase_if(instance.mesh.primitives, [](const auto& primitive)
                              { return primitive.vertices.empty() || primitive.indices.size() < 3; });
                if (instance.mesh.primitives.empty()) continue;
                auto slots = n["Canis::Material"]["MaterialSlots"];
                auto paths = n["Canis::BakeMaterialPaths"];
                for (size_t i = 0; i < instance.mesh.materials.size(); i++)
                {
                    std::string materialPath;
                    if (paths && i < paths.size())
                        materialPath = paths[i].as<std::string>("");
                    else if (slots && i < slots.size())
                    {
                        materialPath = Path(slots[i]);
                    }
                    if (materialPath.empty())
                        materialPath = Path(n["Canis::Material"]["MaterialAsset"]);
                    if (materialPath.empty())
                        continue;
                    auto m = YAML::LoadFile(materialPath);
                    auto color = m["color"].as<Vector4>(Vector4(1));
                    auto& dst = instance.mesh.materials[i];
                    dst["pbrMetallicRoughness"]["baseColorFactor"] = {color.x, color.y, color.z, color.w};
                    dst["pbrMetallicRoughness"]["roughnessFactor"] = m["roughnessValue"].as<float>(.5f);
                    dst["pbrMetallicRoughness"]["metallicFactor"] = m["metallicValue"].as<float>(0);
                    auto addTexture = [&](const char* source, const char* target, bool pbr)
                    {
                        if (!m[source] || !m[source]["path"])
                            return;
                        auto p = std::filesystem::absolute(m[source]["path"].as<std::string>()).string();
                        instance.mesh.images.push_back({{"uri", p}});
                        instance.mesh.textures.push_back({{"source", instance.mesh.images.size() - 1}});
                        Json ref = {{"index", instance.mesh.textures.size() - 1}};
                        if (pbr)
                            dst["pbrMetallicRoughness"][target] = ref;
                        else
                            dst[target] = ref;
                    };
                    addTexture("albedo", "baseColorTexture", true);
                    addTexture("normal", "normalTexture", false);
                    addTexture("emission", "emissiveTexture", false);
                    const auto emission = m["emissionColor"].as<Vector4>(Vector4(1));
                    const float energy = Max(0.0f, m["emissionIntensity"].as<float>(0));
                    dst["emissiveFactor"] = {
                        std::pow(Max(0.0f, emission.x), 2.2f) * energy,
                        std::pow(Max(0.0f, emission.y), 2.2f) * energy,
                        std::pow(Max(0.0f, emission.z), 2.2f) * energy};
                }
                for (auto& p : instance.mesh.primitives)
                    for (size_t i = 0; i + 2 < p.indices.size(); i += 3)
                    {
                        if (p.material >= 0 && instance.mesh.materials[p.material].value(
                                                   "alphaMode", std::string("OPAQUE")) == "BLEND")
                            continue;
                        auto a = Vector3(transform * Vector4(p.vertices.at(p.indices[i]).position, 1)),
                             b = Vector3(transform * Vector4(p.vertices.at(p.indices[i + 1]).position, 1)),
                             c = Vector3(transform * Vector4(p.vertices.at(p.indices[i + 2]).position, 1));
                        auto normal = glm::cross(b - a, c - a);
                        if (glm::length(normal) < 1e-8f)
                            continue;
                        Triangle t{a,
                                   b,
                                   c,
                                   glm::normalize(normal),
                                   MaterialColor(instance.mesh, p.material, false, textureAverages),
                                   MaterialColor(instance.mesh, p.material, true, textureAverages),
                                   glm::min(a, glm::min(b, c)),
                                   glm::max(a, glm::max(b, c))};
                        bvh.triangles.push_back(t);
                    }
                instances.push_back(std::move(instance));
            }
        result.triangles = bvh.triangles.size();
        result.lights = lights.size();
        bvh.Ready();
        if (progress)
            progress("Native bake: " + std::to_string(instances.size()) + " models, " +
                     std::to_string(lights.size()) + " lights, " + std::to_string(bvh.triangles.size()) +
                     " triangles");
        Vector3 ambient = Color3(input["Environment"]["AmbientLight"], Vector3(.15f)) *
                          input["Environment"]["AmbientLightIntensity"].as<float>(.4f);
        std::vector<std::vector<Light>> scenarioLights;
        std::vector<Vector3> scenarioAmbient;
        if (scenarios)
            for (const auto& scenario : *scenarios)
            {
                auto local = lights;
                std::erase_if(local, [](const Light& light) { return light.type == 1; });
                if (scenario.sunEnabled && scenario.sunIntensity > 0)
                {
                    if (!std::isfinite(glm::length(scenario.sunDirection)) ||
                        glm::length(scenario.sunDirection) < .00001f)
                        throw std::runtime_error("Scenario sun direction must be nonzero: " + scenario.name);
                    Light sun;
                    sun.type = 1;
                    sun.direction = glm::normalize(scenario.sunDirection);
                    sun.color = Vector3(scenario.sunColor);
                    sun.intensity = scenario.sunIntensity;
                    local.push_back(sun);
                }
                scenarioLights.push_back(std::move(local));
                scenarioAmbient.push_back(Vector3(scenario.ambientLight) * scenario.ambientIntensity);
            }
        else
        {
            scenarioLights.push_back(lights);
            scenarioAmbient.push_back(ambient);
        }
        std::atomic<size_t> next{0}, vertices{0};
        std::mutex outputMutex;
        std::exception_ptr failure;
        std::vector<std::pair<uint64_t, std::string>> outputs;
        auto worker = [&]
        {
            try
            {
                while (true)
                {
                    size_t id = next.fetch_add(1);
                    if (id >= instances.size())
                        break;
                    if (options.cancel && options.cancel->load())
                        throw std::runtime_error("Bake canceled");
                    auto& instance = instances[id];
                    auto normalMatrix = glm::transpose(glm::inverse(glm::mat3(instance.world)));
                    std::vector<std::vector<std::vector<float>>> channels;
                    for (auto& p : instance.mesh.primitives)
                    {
                        Pipeline::Primitive subdivided;
                        subdivided.material = p.material;
                        for (size_t i = 0; i + 2 < p.indices.size(); i += 3)
                            Subdivide(subdivided, p.vertices[p.indices[i]], p.vertices[p.indices[i + 1]],
                                      p.vertices[p.indices[i + 2]], instance.world,
                                      std::max(.1f, options.vertexSpacing));
                        p = std::move(subdivided);
                        std::vector<std::vector<float>> unevaluated;
                        CompactBakedSamples(p, unevaluated);
                        channels.emplace_back(scenarioLights.size());
                        for (auto& channel : channels.back())
                            channel.reserve(p.vertices.size() * 3);
                        for (auto& v : p.vertices)
                        {
                            if (options.cancel && options.cancel->load())
                                throw std::runtime_error("Bake canceled");
                            auto point = Vector3(instance.world * Vector4(v.position, 1)),
                                 normal = glm::normalize(normalMatrix * v.normal);
                            int free = 0;
                            auto tangent = glm::normalize(glm::cross(
                                std::abs(normal.y) < .9f ? Vector3(0, 1, 0) : Vector3(1, 0, 0), normal));
                            auto bitangent = glm::cross(normal, tangent);
                            std::vector<std::pair<RayHit, Vector3>> hits;
                            for (int sample = 0; sample < options.indirectSamples; sample++)
                            {
                                float u = (sample + .5f) / options.indirectSamples,
                                      angle = sample * 2.39996323f;
                                auto direction = tangent * (std::sqrt(u) * std::cos(angle)) +
                                                 bitangent * (std::sqrt(u) * std::sin(angle)) +
                                                 normal * std::sqrt(1 - u);
                                auto hit = bvh.Cast(point + normal * .007f, direction,
                                                    options.ambientOcclusionDistance);
                                if (hit.triangle < 0)
                                {
                                    free++;
                                    continue;
                                }
                                auto& tri = bvh.triangles[hit.triangle];
                                auto n = glm::dot(tri.normal, -direction) > 0 ? tri.normal : -tri.normal;
                                hits.emplace_back(hit, n);
                            }
                            float ao = options.indirectSamples ? float(free) / options.indirectSamples : 1;
                            for (size_t s = 0; s < scenarioLights.size(); ++s)
                            {
                                Vector3 light = Direct(point, normal, scenarioLights[s], bvh), indirect(0);
                                for (const auto& [hit, hitNormal] : hits)
                                {
                                    const auto& tri = bvh.triangles[hit.triangle];
                                    indirect += tri.emission +
                                                tri.albedo *
                                                    Direct(hit.point, hitNormal, scenarioLights[s], bvh) *
                                                    .318309886f;
                                }
                                light += scenarioAmbient[s] * (.2f + .8f * ao);
                                if (options.indirectSamples)
                                    light += indirect * (options.indirectIntensity / options.indirectSamples);
                                light = glm::clamp(light, Vector3(0), Vector3(64));
                                if (s == 0)
                                    v.color = Vector4(light, 1);
                                auto& channel = channels.back()[s];
                                channel.insert(channel.end(), {light.x, light.y, light.z});
                            }
                        }
                        CompactBakedSamples(p, channels.back());
                        vertices += p.vertices.size();
                    }
                    auto path = directory / (std::to_string(instance.id) + ".gltf");
                    Pipeline::WriteGltf(instance.mesh, path, true);
                    if (scenarios)
                        WriteScenarioChannels(
                            path, *scenarios, channels,
                            previousPaths.contains(instance.id) ? previousPaths.at(instance.id) : "",
                            preservedIds);
                    Pipeline::WriteMeta(path, "MODEL");
                    std::lock_guard lock(outputMutex);
                    outputs.emplace_back(instance.id, path.generic_string());
                    if (progress && outputs.size() % 25 == 0)
                        progress("Baked " + std::to_string(outputs.size()) + " / " +
                                 std::to_string(instances.size()) + " models");
                }
            }
            catch (...)
            {
                std::lock_guard lock(outputMutex);
                if (!failure)
                    failure = std::current_exception();
                next = instances.size();
            }
        };
        std::vector<std::thread> threads;
        size_t count = std::clamp(std::thread::hardware_concurrency(), 1u, 8u);
        for (size_t i = 0; i < count; i++)
            threads.emplace_back(worker);
        for (auto& thread : threads)
            thread.join();
        if (failure)
            std::rethrow_exception(failure);
        result.vertices = vertices;
        std::map<uint64_t, std::string> paths;
        for (auto [id, path] : outputs)
        {
            paths[id] = path;
            result.models.push_back(path);
        }
        for (auto n : result.scene["Entities"])
        {
            auto id = n["Entity"].as<uint64_t>();
            if (!paths.contains(id))
                continue;
            auto model = n["Canis::Model"];
            if (scenarios)
            {
                YAML::Node original;
                original["ModelAsset"] = YAML::Clone(model["ModelAsset"]);
                original["nodeIndex"] = model["nodeIndex"].as<int>(-1);
                original["applyNodeTransform"] = model["applyNodeTransform"].as<bool>(true);
                model["LightingScenarioSource"] = original;
            }
            if (auto collider = n["Canis::MeshCollider"];
                collider && collider["useAttachedModel"].as<bool>(true))
            {
                collider["useAttachedModel"] = false;
                collider["modelPath"] = Path(model["ModelAsset"]);
            }
            model["ModelAsset"]["path"] = paths[id];
            model["ModelAsset"].remove("uuid");
            model["nodeIndex"] = -1;
            model["applyNodeTransform"] = true;
            n.remove("Canis::BakeMaterialPaths");
        }
        return result;
    }
    LightingBakeResult BakeSceneLighting(YAML::Node input, const std::filesystem::path& directory,
                                         const LightingBakeOptions& options,
                                         const std::function<void(const std::string&)>& progress)
    {
        return BakeSceneLightingInternal(std::move(input), directory, options, progress);
    }
    LightingScenarioBakeResult BakeLightingScenarios(YAML::Node input, const std::filesystem::path& directory,
                                                     const std::vector<LightingScenario>& definitions,
                                                     const LightingBakeOptions& options,
                                                     const std::function<void(const std::string&)>& progress)
    {
        if (definitions.empty())
            throw std::runtime_error("Create a lighting scenario before baking");
        if (options.cancel && options.cancel->load())
            throw std::runtime_error("Bake canceled");
        if (!std::isfinite(options.vertexSpacing) || options.vertexSpacing <= 0 ||
            options.indirectSamples < 0 || options.indirectSamples > 4096 ||
            !std::isfinite(options.indirectIntensity) || options.indirectIntensity < 0 ||
            !std::isfinite(options.ambientOcclusionDistance) || options.ambientOcclusionDistance <= 0)
            throw std::runtime_error("Invalid lighting bake quality settings");
        std::set<std::string> ids;
        std::vector<LightingScenario> selected;
        for (const auto& scenario : definitions)
        {
            if (scenario.id.empty() || !ids.insert(scenario.id).second)
                throw std::runtime_error("Lighting scenario IDs must be nonempty and unique");
            if (options.selectedScenarioIds.empty() ||
                std::find(options.selectedScenarioIds.begin(), options.selectedScenarioIds.end(),
                          scenario.id) != options.selectedScenarioIds.end())
                selected.push_back(scenario);
        }
        for (const auto& selectedId : options.selectedScenarioIds)
            if (!ids.contains(selectedId))
                throw std::runtime_error("Unknown lighting scenario: " + selectedId);
        if (selected.empty())
            throw std::runtime_error("No lighting scenarios selected");
        for (const auto& scenario : selected)
        {
            const auto finiteColor = [](const Vector4& c)
            { return std::isfinite(c.x) && std::isfinite(c.y) && std::isfinite(c.z) && std::isfinite(c.w); };
            if (!finiteColor(scenario.ambientLight) || !finiteColor(scenario.sunColor) ||
                !std::isfinite(scenario.ambientIntensity) || scenario.ambientIntensity < 0 ||
                !std::isfinite(scenario.sunIntensity) || scenario.sunIntensity < 0 ||
                !std::isfinite(scenario.skyboxIntensity) || scenario.skyboxIntensity < 0 ||
                !std::isfinite(scenario.cloudCoverage) || scenario.cloudCoverage < 0 || scenario.cloudCoverage > 1 ||
                !std::isfinite(scenario.cloudDensity) || scenario.cloudDensity < 0)
                throw std::runtime_error("Lighting values must be finite and nonnegative: " + scenario.name);
        }
        const auto fingerprint = LightingScenarioSourceFingerprint(input);
        std::map<uint64_t, std::string> previousPaths;
        for (auto node : input["Entities"])
            if (auto model = node["Canis::Model"]; model && model["static"].as<bool>(false))
                previousPaths[node["Entity"].as<uint64_t>()] = Path(model["ModelAsset"]);
        YAML::Node quality;
        quality["VertexSharingVersion"] = 1;
        quality["VertexSpacing"] = options.vertexSpacing;
        quality["IndirectSamples"] = options.indirectSamples;
        quality["IndirectIntensity"] = options.indirectIntensity;
        quality["AmbientOcclusionDistance"] = options.ambientOcclusionDistance;
        auto sameQuality = [&](const LightingScenario& scenario)
        {
            if (scenario.manifestPath.empty() || !std::filesystem::exists(scenario.manifestPath))
                return false;
            const auto saved = YAML::LoadFile(scenario.manifestPath)["Quality"];
            return saved && saved["VertexSharingVersion"].as<int>(0) == 1 &&
                   saved["VertexSpacing"].as<float>(-1) == options.vertexSpacing &&
                   saved["IndirectSamples"].as<int>(-1) == options.indirectSamples &&
                   saved["IndirectIntensity"].as<float>(-1) == options.indirectIntensity &&
                   saved["AmbientOcclusionDistance"].as<float>(-1) == options.ambientOcclusionDistance;
        };
        std::vector<std::string> preservedIds;
        for (const auto& scenario : definitions)
        {
            const bool evaluating = std::any_of(selected.begin(), selected.end(),
                                                [&](const auto& s) { return s.id == scenario.id; });
            if (!evaluating && scenario.sourceFingerprint == fingerprint &&
                scenario.settingsFingerprint == LightingScenarioSettingsFingerprint(scenario) &&
                sameQuality(scenario))
                preservedIds.push_back(scenario.id);
        }
        static std::atomic<uint64_t> sequence{0};
        const auto token =
            std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()) + "-" +
            std::to_string(sequence.fetch_add(1));
        const auto staging = directory / (".scenario-stage-" + token);
        const auto generation = directory / ("scenario-" + token);
        // Each published generation is immutable. An atomic directory rename commits geometry,
        // channels, provenance and manifest together on every supported filesystem platform.
        const auto manifestPath = generation / "lighting-scenarios.yaml";
        const auto temporaryManifest = staging / "lighting-scenarios.yaml";
        bool committed = false;
        try
        {
            std::filesystem::create_directories(staging);
            auto baked = BakeSceneLightingInternal(input, staging, options, progress, &selected,
                                                   previousPaths, preservedIds);
            if (baked.models.empty())
                throw std::runtime_error("No static models were found to bake");
            LightingScenarioBakeResult result;
            static_cast<LightingBakeResult&>(result) = std::move(baked);
            result.manifestPath = manifestPath.generic_string();
            result.scenarios = definitions;
            YAML::Node manifest;
            manifest["Version"] = 1;
            manifest["SourceFingerprint"] = fingerprint;
            manifest["Quality"] = quality;
            manifest["VertexFormat"] = "RGB32F linear irradiance";
            manifest["Vertices"] = result.vertices;
            manifest["Triangles"] = result.triangles;
            const size_t probeCount =
                input["Environment"]["LightingProbes"] ? input["Environment"]["LightingProbes"].size() : 0;
            manifest["ProbeCount"] = probeCount;
            for (size_t i = 0; i < probeCount; ++i)
                manifest["ProbeEnabled"].push_back(input["Environment"]["LightingProbes"][i]["Enabled"].as<bool>(true));
            for (auto& scenario : result.scenarios)
            {
                const bool evaluating = std::any_of(selected.begin(), selected.end(),
                                                    [&](const auto& s) { return s.id == scenario.id; });
                const bool preserving =
                    std::find(preservedIds.begin(), preservedIds.end(), scenario.id) != preservedIds.end();
                if (!evaluating && !preserving)
                {
                    scenario.manifestPath.clear();
                    scenario.sourceFingerprint.clear();
                    scenario.settingsFingerprint.clear();
                    scenario.probeCachePaths.clear();
                }
                else
                {
                    scenario.manifestPath = result.manifestPath;
                    scenario.sourceFingerprint = fingerprint;
                    scenario.settingsFingerprint = LightingScenarioSettingsFingerprint(scenario);
                    if (evaluating)
                    {
                        scenario.probeCachePaths.clear();
                        const auto safeId = std::to_string(Pipeline::AssetUUID(scenario.id));
                        for (size_t i = 0; i < probeCount; ++i)
                            scenario.probeCachePaths.push_back(
                                (generation / ("probe-" + safeId + "-" + std::to_string(i) + ".probe"))
                                    .generic_string());
                    }
                }
                manifest["Scenarios"].push_back(SerializeLightingScenario(scenario));
            }
            for (auto node : result.scene["Entities"])
            {
                auto model = node["Canis::Model"];
                if (!model || !model["LightingScenarioSource"])
                    continue;
                const auto id = node["Entity"].as<uint64_t>();
                const auto finalPath = generation / (std::to_string(id) + ".gltf");
                model["ModelAsset"]["path"] = finalPath.generic_string();
                YAML::Node entry;
                entry["Entity"] = id;
                entry["Path"] = finalPath.generic_string();
                entry["GeneratedUUID"] = Pipeline::AssetUUID(finalPath.generic_string());
                entry["Source"] = YAML::Clone(model["LightingScenarioSource"]);
                manifest["Models"].push_back(entry);
                model.remove("LightingScenarioSource");
                // UUIDs name the final location even though files are still staged.
                const auto metaPath = staging / (std::to_string(id) + ".gltf.meta");
                auto metadata = YAML::LoadFile(metaPath.string());
                metadata["UUID"] = Pipeline::AssetUUID(finalPath.generic_string());
                std::ofstream metadataFile(metaPath);
                metadataFile << metadata;
                metadataFile.close();
                if (!metadataFile)
                    throw std::runtime_error("Cannot write staged model metadata");
            }
            result.models.clear();
            for (const auto& model : manifest["Models"])
                result.models.push_back(model["Path"].as<std::string>());
            result.scene["Environment"]["LightingScenarios"] = manifest["Scenarios"];
            result.scene["Environment"]["LightingScenarioSources"] = manifest["Models"];
            if (!result.scene["Environment"]["InitialLightingScenario"] ||
                !ids.contains(result.scene["Environment"]["InitialLightingScenario"].as<std::string>()))
                result.scene["Environment"]["InitialLightingScenario"] = definitions.front().id;
            if (options.cancel && options.cancel->load())
                throw std::runtime_error("Bake canceled");
            std::ofstream manifestFile(temporaryManifest);
            manifestFile << manifest;
            manifestFile.close();
            if (!manifestFile)
                throw std::runtime_error("Cannot write lighting scenario manifest");
            if (options.cancel && options.cancel->load())
                throw std::runtime_error("Bake canceled");
            std::filesystem::rename(staging, generation);
            committed = true;
            // A UI callback cannot turn a completed atomic publication into a failed bake.
            if (progress)
                try
                {
                    progress("Lighting scenarios ready: " + std::to_string(selected.size()) + " evaluated, " +
                             std::to_string(preservedIds.size()) + " retained; shared vertices " +
                             std::to_string(result.vertices));
                }
                catch (...)
                {
                }
            return result;
        }
        catch (...)
        {
            std::error_code error;
            std::filesystem::remove_all(staging, error);
            std::filesystem::remove(temporaryManifest, error);
            if (!committed)
                std::filesystem::remove_all(generation, error);
            throw;
        }
    }
    YAML::Node CaptureLightingBakeScene(Scene& scene)
    {
        auto doc = scene.EncodeScene();
        for (auto n : doc["Entities"])
        {
            auto model = n["Canis::Model"];
            if (model && model["ModelAsset"] && !model["ModelAsset"]["path"])
            {
                auto path = Path(model["ModelAsset"]);
                if (!path.empty())
                    model["ModelAsset"]["path"] = path;
            }
            if (auto slots = n["Canis::Material"]["MaterialSlots"])
                for (auto id : slots)
                    n["Canis::BakeMaterialPaths"].push_back(Path(id));
        }
        return doc;
    }
    void ApplyLightingBake(Scene& scene, const LightingBakeResult& result)
    {
        std::set<std::string> bakedPaths(result.models.begin(), result.models.end());
        for (auto node : result.scene["Entities"])
        {
            auto e = scene.FindEntity(UUID(node["Entity"].as<uint64_t>()));
            if (!e || !node["Canis::Model"])
                continue;
            auto path = node["Canis::Model"]["ModelAsset"]["path"];
            if (!path || !bakedPaths.contains(path.as<std::string>()))
                continue;
            e.GetComponent<Model>().modelId = AssetManager::LoadModel(path.as<std::string>());
            e.GetComponent<Model>().nodeIndex = -1;
            e.GetComponent<Model>().applyNodeTransform = true;
            if (auto* collider = e.TryGetComponent<MeshCollider>(); collider && node["Canis::MeshCollider"])
            {
                auto original = node["Canis::MeshCollider"]["modelPath"].as<std::string>("");
                if (!original.empty())
                {
                    collider->useAttachedModel = false;
                    collider->modelPath = original;
                    collider->modelId = AssetManager::LoadModel(original);
                }
            }
        }
    }
    void RegisterBakeLight(App& app)
    {
        static ComponentConf conf;
        DEFAULT_COMPONENT_CONFIG(conf, BakeLight);
        REGISTER_PROPERTY(conf, BakeLight, type);
        REGISTER_PROPERTY(conf, BakeLight, spotAngle);
        REGISTER_PROPERTY(conf, BakeLight, innerAngle);
        REGISTER_PROPERTY(conf, BakeLight, enabled);
        conf.DEFAULT_DRAW_COMPONENT_INSPECTOR(BakeLight);
        app.RegisterComponent(conf);
    }
} // namespace Canis
