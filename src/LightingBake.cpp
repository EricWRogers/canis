#include <Canis/LightingBake.hpp>
#include <Canis/AssetPipeline.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/App.hpp>
#include <stb_image.h>
#include <thread>
#include <mutex>
#include <map>
#include <set>
#include <numeric>
#include <fstream>
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
    } // namespace
    LightingBakeResult BakeSceneLighting(YAML::Node input, const std::filesystem::path& directory,
                                         const LightingBakeOptions& options,
                                         const std::function<void(const std::string&)>& progress)
    {
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
                auto transform = world(world, id);
                if (auto l = n["Canis::PointLight"]; l && l["enabled"].as<bool>(true) &&
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
                if (auto l = n["Canis::DirectionalLight"]; l && l["enabled"].as<bool>(true) &&
                    l["mode"].as<int>(LightMode::Mixed) != LightMode::Realtime)
                {
                    Light light;
                    light.type = 1;
                    light.direction = l["direction"].as<Vector3>();
                    light.color = Color3(l["color"]);
                    light.intensity = l["intensity"].as<float>(1);
                    lights.push_back(light);
                }
                if (auto l = n["Canis::SpotLight"]; l && l["enabled"].as<bool>(true) &&
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
                    meshCache[key] =
                        Pipeline::Flatten(Pipeline::ReadGltf(path), model["nodeIndex"].as<int>(-1),
                                          model["applyNodeTransform"].as<bool>(true));
                Instance instance{id, meshCache[key], transform, n};
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
                    if (m["emission"])
                        dst["emissiveFactor"] = {1, 1, 1};
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
                    for (auto& p : instance.mesh.primitives)
                    {
                        Pipeline::Primitive subdivided;
                        subdivided.material = p.material;
                        for (size_t i = 0; i + 2 < p.indices.size(); i += 3)
                            Subdivide(subdivided, p.vertices[p.indices[i]], p.vertices[p.indices[i + 1]],
                                      p.vertices[p.indices[i + 2]], instance.world,
                                      std::max(.1f, options.vertexSpacing));
                        p = std::move(subdivided);
                        for (auto& v : p.vertices)
                        {
                            auto point = Vector3(instance.world * Vector4(v.position, 1)),
                                 normal = glm::normalize(normalMatrix * v.normal);
                            Vector3 light = Direct(point, normal, lights, bvh), indirect(0);
                            int free = 0;
                            auto tangent = glm::normalize(glm::cross(
                                std::abs(normal.y) < .9f ? Vector3(0, 1, 0) : Vector3(1, 0, 0), normal));
                            auto bitangent = glm::cross(normal, tangent);
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
                                indirect += tri.emission +
                                            tri.albedo * Direct(hit.point, n, lights, bvh) * .318309886f;
                            }
                            float ao = options.indirectSamples ? float(free) / options.indirectSamples : 1;
                            light += ambient * (.2f + .8f * ao);
                            if (options.indirectSamples)
                                light += indirect * (options.indirectIntensity / options.indirectSamples);
                            v.color = Vector4(glm::clamp(light, Vector3(0), Vector3(64)), 1);
                        }
                        vertices += p.vertices.size();
                    }
                    auto path = directory / (std::to_string(instance.id) + ".gltf");
                    Pipeline::WriteGltf(instance.mesh, path, true);
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
