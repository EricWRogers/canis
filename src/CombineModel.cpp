#include <Canis/CombineModel.hpp>
#include <Canis/App.hpp>
#include <Canis/AssetManager.hpp>
#include <Canis/Components.hpp>
#include <Canis/ConfigHelper.hpp>
#include <Canis/Debug.hpp>
#include <Canis/Editor.hpp>
#include <Canis/Scene.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include <glm/gtc/quaternion.hpp>

namespace Canis
{
namespace
{
    constexpr uint32_t CacheVersion = 1;
    constexpr uint32_t MaxCacheItems = 100000000;
    using Primitive = ModelAsset::PrimitiveBuild3D;

    uint64_t HashBytes(uint64_t hash, const void* data, size_t size)
    {
        const auto* bytes = static_cast<const unsigned char*>(data);
        for (size_t i = 0; i < size; ++i) { hash ^= bytes[i]; hash *= 1099511628211ULL; }
        return hash;
    }
    uint64_t HashText(uint64_t hash, const std::string& text)
    {
        hash = HashBytes(hash, text.data(), text.size());
        const char separator = '\0';
        return HashBytes(hash, &separator, 1);
    }
    std::string Hex(uint64_t value)
    {
        std::ostringstream out; out << std::hex << std::setw(16) << std::setfill('0') << value;
        return out.str();
    }
    std::string FileHash(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file) return {};
        uint64_t hash = 14695981039346656037ULL;
        std::array<char, 65536> bytes{};
        while (file)
        {
            file.read(bytes.data(), bytes.size());
            hash = HashBytes(hash, bytes.data(), static_cast<size_t>(file.gcount()));
        }
        return file.eof() ? Hex(hash) : std::string{};
    }
    std::string Canonical(const YAML::Node& node)
    {
        if (!node || node.IsNull()) return "null";
        if (node.IsScalar()) return "s" + YAML::Dump(node);
        std::string out = node.IsSequence() ? "[" : "{";
        if (node.IsSequence())
            for (auto item : node) out += Canonical(item) + ",";
        else
        {
            std::map<std::string, YAML::Node> fields;
            for (auto item : node) fields.emplace(item.first.as<std::string>(), item.second);
            for (const auto& [key, value] : fields) out += key + ":" + Canonical(value) + ",";
        }
        return out + (node.IsSequence() ? "]" : "}");
    }
    std::string ResolveReference(const YAML::Node& reference)
    {
        if (!reference || reference.IsNull()) return {};
        if (reference.IsMap())
        {
            if (reference["uuid"])
            {
                const auto path = AssetManager::GetPath(UUID(reference["uuid"].as<uint64_t>(0)));
                if (path != "Path was not found in AssetLibrary") return path;
            }
            return reference["path"].as<std::string>("");
        }
        if (!reference.IsScalar()) return {};
        const auto value = reference.as<std::string>("");
        if (value.empty()) return {};
        if (std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= '0' && c <= '9'; }))
        {
            try
            {
                const auto path = AssetManager::GetPath(UUID(std::stoull(value)));
                if (path != "Path was not found in AssetLibrary") return path;
            }
            catch (...) {}
            return {};
        }
        return value;
    }
    std::string ModelPath(const YAML::Node& model)
    {
        if (model["ModelAsset"]) return ResolveReference(model["ModelAsset"]);
        if (model["modelUUID"]) return ResolveReference(model["modelUUID"]);
        return model["modelPath"].as<std::string>("");
    }
    void CollectReferences(const YAML::Node& node, const std::set<uint64_t>& ids,
                           std::set<uint64_t>& referenced)
    {
        if (!node) return;
        if (node.IsScalar())
        {
            const auto value = node.as<std::string>("");
            if (value.empty() || !std::all_of(value.begin(), value.end(), [](unsigned char c) { return c >= '0' && c <= '9'; })) return;
            try { const auto id = std::stoull(value); if (ids.contains(id)) referenced.insert(id); }
            catch (...) {}
        }
        else if (node.IsSequence()) for (auto item : node) CollectReferences(item, ids, referenced);
        else if (node.IsMap()) for (auto item : node) CollectReferences(item.second, ids, referenced);
    }
    bool Plain(const YAML::Node& node, bool allowModel)
    {
        for (auto field : node)
        {
            const auto key = field.first.as<std::string>();
            if (key == "Entity" || key == "Name" || key == "Active" || key == "EditorLocked" ||
                key == "Tag" || key == "Canis::Transform") continue;
            if (allowModel && (key == "Canis::Model" || key == "Canis::Material")) continue;
            return false;
        }
        const auto tag = node["Tag"].as<std::string>("");
        return tag.empty() || tag == "None";
    }
    Matrix4 LocalMatrix(const YAML::Node& node)
    {
        const auto transform = node["Canis::Transform"];
        return glm::translate(Matrix4(1), transform["position"].as<Vector3>(Vector3(0))) *
            glm::mat4_cast(glm::normalize(transform["rotation"].as<Quaternion>(Quaternion(Vector3(0))))) *
            glm::scale(Matrix4(1), transform["scale"].as<Vector3>(Vector3(1)));
    }
    bool IsOpaqueMaterial(const YAML::Node& material)
    {
        if (!material) return true;
        if (material["color"].as<Color>(Color(1)).a < .999f) return false;
        const auto check = [](const YAML::Node& reference)
        {
            const auto path = ResolveReference(reference);
            if (path.empty()) return true;
            try
            {
                const auto document = YAML::LoadFile(path);
                auto shader = ResolveReference(document["shader"]);
                if (shader.ends_with(".vs") || shader.ends_with(".fs")) shader.resize(shader.size() - 3);
                if (!shader.empty() && std::filesystem::path(shader).filename() != "model3d")
                {
                    std::ifstream source(shader + ".vs");
                    const std::string text((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
                    if (text.find("CANIS_STATIC_BATCHING") == std::string::npos) return false;
                }
                // Per-entity and material custom uniforms can describe vertex
                // displacement. Keep these objects on their ordinary path.
                return document["color"].as<Color>(Color(1)).a >= .999f &&
                    (!document["uniforms"] || document["uniforms"].size() == 0);
            }
            catch (...) { return false; }
        };
        if (!check(material["MaterialAsset"])) return false;
        for (const auto* key : {"MaterialSlots", "materialSlots", "materialAssets", "slotMaterials"})
            if (material[key] && material[key].IsSequence())
                for (auto reference : material[key]) if (!check(reference)) return false;
        return (!material["uniforms"] || material["uniforms"].size() == 0) &&
            (!material["UniformOverrides"] || material["UniformOverrides"].size() == 0);
    }
    void AddDependency(const std::filesystem::path& input, std::set<std::string>& dependencies)
    {
        const auto path = input.lexically_normal();
        const auto value = path.generic_string();
        if (value.empty() || !dependencies.insert(value).second) return;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
        {
            if (std::filesystem::is_regular_file(value + ".vs", ec)) AddDependency(value + ".vs", dependencies);
            if (std::filesystem::is_regular_file(value + ".fs", ec)) AddDependency(value + ".fs", dependencies);
            return;
        }
        const auto extension = path.extension().string();
        if (extension == ".vs") AddDependency(path.parent_path() / (path.stem().string() + ".fs"), dependencies);
        if (extension == ".fs") AddDependency(path.parent_path() / (path.stem().string() + ".vs"), dependencies);
        if (extension != ".meta") AddDependency(value + ".meta", dependencies);
        if (extension == ".obj" || extension == ".mtl")
        {
            std::ifstream stream(path); std::string line;
            while (std::getline(stream, line))
            {
                std::istringstream fields(line); std::string key, rest;
                fields >> key; std::getline(fields >> std::ws, rest);
                if (key == "mtllib" || key.starts_with("map_") || key == "bump" || key == "norm")
                    AddDependency(path.parent_path() / rest, dependencies);
            }
        }
        else if (extension == ".material" || extension == ".gltf" || extension == ".meta")
        {
            try
            {
                const auto walk = [&](auto&& self, const YAML::Node& node) -> void
                {
                    if (!node) return;
                    if (node.IsMap())
                    {
                        const auto resolved = ResolveReference(node);
                        if (!resolved.empty() && std::filesystem::is_regular_file(resolved, ec)) AddDependency(resolved, dependencies);
                        for (auto field : node) self(self, field.second);
                    }
                    else if (node.IsSequence()) for (auto item : node) self(self, item);
                    else if (node.IsScalar())
                    {
                        const auto value = ResolveReference(node);
                        if (value.empty()) return;
                        if (std::filesystem::is_regular_file(value, ec)) AddDependency(value, dependencies);
                        else if (!value.starts_with("data:") && std::filesystem::is_regular_file(path.parent_path() / value, ec))
                            AddDependency(path.parent_path() / value, dependencies);
                    }
                };
                walk(walk, YAML::LoadFile(value));
            }
            catch (...) {}
        }
    }
    void CollectAssetDependencies(const YAML::Node& node, std::set<std::string>& dependencies)
    {
        if (!node) return;
        std::error_code ec;
        if (node.IsMap())
        {
            const auto path = ResolveReference(node);
            if (!path.empty()) AddDependency(path, dependencies);
            for (auto field : node) CollectAssetDependencies(field.second, dependencies);
        }
        else if (node.IsSequence()) for (auto item : node) CollectAssetDependencies(item, dependencies);
        else if (node.IsScalar())
        {
            const auto path = ResolveReference(node);
            if (!path.empty() && std::filesystem::is_regular_file(path, ec)) AddDependency(path, dependencies);
        }
    }
    struct Source
    {
        uint64_t id;
        YAML::Node node;
        Matrix4 matrix;
        std::string path;
    };
    struct Plan
    {
        std::vector<Source> sources;
        std::set<uint64_t> removed;
        std::string fingerprint;
        int skipped = 0;
    };
    bool MakePlan(const YAML::Node& entities, uint64_t rootId, float cellSize,
                  Plan& plan, std::string& error)
    {
        if (!entities.IsSequence()) { error = "Scene entities must be a sequence."; return false; }
        if (!std::isfinite(cellSize) || cellSize < 0) { error = "Cell size must be finite and nonnegative."; return false; }
        std::map<uint64_t, YAML::Node> nodes;
        std::map<uint64_t, std::vector<uint64_t>> children;
        std::set<uint64_t> ids, referenced;
        for (auto node : entities)
        {
            const auto id = node["Entity"].as<uint64_t>(0);
            if (!id || !ids.insert(id).second) { error = "Invalid or duplicate entity UUID."; return false; }
            nodes.emplace(id, node);
        }
        if (!nodes.contains(rootId) || !nodes.at(rootId)["Canis::Transform"])
        { error = "CombineModel requires a root Transform."; return false; }
        for (const auto& [id, node] : nodes)
        {
            const auto transform = node["Canis::Transform"];
            if (transform) children[transform["parent"].as<uint64_t>(0)].push_back(id);
            for (auto field : node)
            {
                const auto key = field.first.as<std::string>();
                if (key != "Canis::Transform" && key != "Entity" && key != "Canis::Model" &&
                    key != "Canis::Material" && key != "Canis::CombineModel")
                    CollectReferences(field.second, ids, referenced);
            }
        }
        for (auto& [parent, values] : children) std::sort(values.begin(), values.end());
        uint64_t hash = HashText(14695981039346656037ULL, "CombineModel-v1");
        hash = HashText(hash, std::to_string(rootId));
        hash = HashText(hash, std::to_string(cellSize));
        std::set<std::string> dependencies;
        std::set<uint64_t> visiting, visited;
        const auto traverse = [&](auto&& self, uint64_t id, const Matrix4& matrix, bool unsafe) -> bool
        {
            if (!visiting.insert(id).second) { error = "Transform hierarchy contains a cycle."; return false; }
            if (!visited.insert(id).second) { error = "Transform hierarchy has multiple parents."; return false; }
            const auto& node = nodes.at(id);
            const auto transform = node["Canis::Transform"];
            hash = HashText(hash, Canonical(node));
            hash = HashText(hash, referenced.contains(id) ? "referenced" : "unreferenced");
            const bool root = id == rootId;
            const bool active = node["Active"].as<bool>(true) && transform["active"].as<bool>(true);
            bool blockChildren = unsafe || !active || referenced.contains(id);
            if (!root && !Plain(node, true)) blockChildren = true;
            // Combined collision derives its shape from the original models.
            // Removing those render objects would silently remove collision.
            if (node["Canis::CombineMeshCollider"] || node["Canis::Rigidbody"] ||
                node["Canis::ModelAnimation"] || (!root && node["Canis::CombineModel"])) blockChildren = true;
            const auto local = root ? matrix : matrix * LocalMatrix(node);
            const auto model = node["Canis::Model"];
            bool accepted = false;
            if (!root && model && !blockChildren && Plain(node, true) &&
                model["static"].as<bool>(false) && !model["bakedVisibility"].as<bool>(false) &&
                model["color"].as<Color>(Color(1)).a >= .999f && IsOpaqueMaterial(node["Canis::Material"]))
            {
                const auto path = ModelPath(model);
                if (!path.empty() && !path.ends_with(".cmodel"))
                {
                    plan.sources.push_back({id, node, local, path});
                    CollectAssetDependencies(model, dependencies);
                    CollectAssetDependencies(node["Canis::Material"], dependencies);
                    plan.removed.insert(id);
                    accepted = true;
                }
            }
            if (!root && !accepted) ++plan.skipped;
            for (auto child : children[id])
                if (!self(self, child, local, blockChildren)) return false;
            // Empty authoring-only groups can disappear too. A preserved child
            // always preserves every ancestor that defines its transform.
            if (!root && !model && Plain(node, false) && !blockChildren)
            {
                bool allRemoved = true;
                for (auto child : children[id]) allRemoved &= plan.removed.contains(child);
                if (allRemoved) plan.removed.insert(id);
            }
            if (model && plan.removed.contains(id))
                for (auto child : children[id]) if (!plan.removed.contains(child))
                {
                    // Keep this model's Transform for retained descendants;
                    // root preprocessing must not duplicate its renderer.
                    error = "An otherwise eligible model has a retained child. Move its render mesh to a separate leaf before combining.";
                    return false;
                }
            visiting.erase(id);
            return true;
        };
        if (!traverse(traverse, rootId, Matrix4(1), false)) return false;
        if (plan.sources.empty()) { error = "No eligible static render-only child models."; return false; }
        for (const auto& path : dependencies)
        {
            hash = HashText(hash, path);
            std::error_code ec;
            if (std::filesystem::is_regular_file(path, ec))
            {
                hash = HashText(hash, std::to_string(std::filesystem::file_size(path, ec)));
                hash = HashText(hash, std::to_string(std::filesystem::last_write_time(path, ec).time_since_epoch().count()));
            }
            else hash = HashText(hash, "missing");
        }
        plan.fingerprint = Hex(hash);
        return true;
    }

    template<typename T> bool Write(std::ostream& stream, const T& value)
    { stream.write(reinterpret_cast<const char*>(&value), sizeof(T)); return bool(stream); }
    template<typename T> bool Read(std::istream& stream, T& value)
    { stream.read(reinterpret_cast<char*>(&value), sizeof(T)); return bool(stream); }
    bool WriteString(std::ostream& stream, const std::string& value)
    {
        if (value.size() > std::numeric_limits<uint32_t>::max()) return false;
        return Write(stream, uint32_t(value.size())) && bool(stream.write(value.data(), value.size()));
    }
    bool ReadString(std::istream& stream, std::string& value)
    {
        uint32_t size;
        if (!Read(stream, size) || size > 1024 * 1024) return false;
        value.resize(size); return bool(stream.read(value.data(), size));
    }
    bool WriteVector3(std::ostream& stream, const Vector3& value)
    { return Write(stream, value.x) && Write(stream, value.y) && Write(stream, value.z); }
    bool ReadVector3(std::istream& stream, Vector3& value)
    { return Read(stream, value.x) && Read(stream, value.y) && Read(stream, value.z); }
    bool WriteColor(std::ostream& stream, const Color& value)
    { return Write(stream, value.r) && Write(stream, value.g) && Write(stream, value.b) && Write(stream, value.a); }
    bool ReadColor(std::istream& stream, Color& value)
    { return Read(stream, value.r) && Read(stream, value.g) && Read(stream, value.b) && Read(stream, value.a); }
    std::string TexturePath(int id)
    {
        if (id < 0) return {};
        auto path = AssetManager::GetPath(id);
        return path == "Path was not found in AssetLibrary" ? std::string{} : path;
    }
    bool WriteGeometry(const std::string& path, const std::vector<Primitive>& primitives,
                       const std::vector<std::string>& slotNames)
    {
        const auto pending = path + ".tmp";
        std::ofstream stream(pending, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream.write("CANISCM1", 8);
        Write(stream, CacheVersion); Write(stream, uint32_t(slotNames.size()));
        for (const auto& name : slotNames) WriteString(stream, name);
        Write(stream, uint32_t(primitives.size()));
        for (const auto& primitive : primitives)
        {
            Write(stream, primitive.materialSlot);
            WriteString(stream, TexturePath(primitive.textureId));
            WriteString(stream, TexturePath(primitive.normalTextureId));
            WriteString(stream, TexturePath(primitive.emissionTextureId));
            WriteColor(stream, primitive.baseColor); WriteVector3(stream, primitive.emissionFactor);
            Write(stream, uint8_t(primitive.bakedLighting));
            Write(stream, uint32_t(primitive.vertices.size()));
            Write(stream, uint32_t(primitive.indices.size()));
            for (const auto& vertex : primitive.vertices)
            {
                WriteVector3(stream, vertex.position); WriteVector3(stream, vertex.normal);
                Write(stream, vertex.uv.x); Write(stream, vertex.uv.y); WriteColor(stream, vertex.color);
            }
            for (auto index : primitive.indices) Write(stream, index);
            Write(stream, uint32_t(primitive.lightingScenarios.size()));
            for (const auto& [name, values] : primitive.lightingScenarios)
            {
                WriteString(stream, name);
                Write(stream, uint32_t(values.size()));
                for (const auto& value : values) WriteVector3(stream, value);
            }
        }
        stream.flush(); const bool good = bool(stream); stream.close();
        if (!good) { std::filesystem::remove(pending); return false; }
        std::error_code ec; std::filesystem::rename(pending, path, ec);
        if (ec) { std::filesystem::remove(pending); return false; }
        return true;
    }
    bool Compatible(const Primitive& a, const Primitive& b)
    {
        return a.materialSlot == b.materialSlot && a.textureId == b.textureId &&
            a.normalTextureId == b.normalTextureId && a.emissionTextureId == b.emissionTextureId &&
            a.baseColor == b.baseColor && a.emissionFactor == b.emissionFactor && a.bakedLighting == b.bakedLighting;
    }
    void AppendPrimitive(std::vector<Primitive>& groups, Primitive primitive)
    {
        auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& group) { return Compatible(group, primitive); });
        if (found == groups.end()) { groups.push_back(std::move(primitive)); return; }
        auto& group = *found;
        const auto first = static_cast<uint32_t>(group.vertices.size());
        for (const auto& [name, values] : primitive.lightingScenarios)
            if (!group.lightingScenarios.contains(name))
            {
                auto& output = group.lightingScenarios[name];
                for (const auto& vertex : group.vertices) output.emplace_back(vertex.color);
            }
        for (auto& [name, output] : group.lightingScenarios)
        {
            const auto source = primitive.lightingScenarios.find(name);
            for (size_t i = 0; i < primitive.vertices.size(); ++i)
                output.push_back(source != primitive.lightingScenarios.end() ? source->second[i] : Vector3(primitive.vertices[i].color));
        }
        group.vertices.insert(group.vertices.end(), primitive.vertices.begin(), primitive.vertices.end());
        for (auto index : primitive.indices) group.indices.push_back(first + index);
    }
}

bool ReadCombinedModelGeometry(const std::string& path,
                               std::vector<ModelAsset::PrimitiveBuild3D>& output,
                               std::vector<std::string>& materialSlotNames)
{
    std::ifstream stream(path, std::ios::binary);
    std::error_code fileError;
    const auto fileBytes = std::filesystem::file_size(path, fileError);
    if (fileError) return false;
    std::array<char, 8> magic{}; uint32_t version, count;
    if (!stream.read(magic.data(), magic.size()) || std::memcmp(magic.data(), "CANISCM1", 8) ||
        !Read(stream, version) || version != CacheVersion || !Read(stream, count) || count > 100000) return false;
    std::vector<std::string> slots(count);
    for (auto& slot : slots) if (!ReadString(stream, slot)) return false;
    if (!Read(stream, count) || count > 100000) return false;
    std::vector<Primitive> primitives(count);
    std::vector<std::array<std::string, 3>> texturePaths(count);
    for (size_t p = 0; p < primitives.size(); ++p)
    {
        auto& primitive = primitives[p]; uint32_t vertices, indices, scenarios; uint8_t baked;
        if (!Read(stream, primitive.materialSlot)) return false;
        for (auto& texture : texturePaths[p]) if (!ReadString(stream, texture)) return false;
        if (!ReadColor(stream, primitive.baseColor) || !ReadVector3(stream, primitive.emissionFactor) ||
            !Read(stream, baked) || !Read(stream, vertices) || !Read(stream, indices) ||
            !vertices || vertices > MaxCacheItems || indices > MaxCacheItems || indices % 3) return false;
        const auto position = stream.tellg();
        if (position < 0 || static_cast<uint64_t>(position) > fileBytes ||
            uint64_t(vertices) * sizeof(float) * 12 + uint64_t(indices) * sizeof(uint32_t) >
                fileBytes - static_cast<uint64_t>(position)) return false;
        primitive.bakedLighting = baked != 0;
        primitive.vertices.resize(vertices); primitive.indices.resize(indices);
        for (auto& vertex : primitive.vertices)
        {
            if (!ReadVector3(stream, vertex.position) || !ReadVector3(stream, vertex.normal) ||
                !Read(stream, vertex.uv.x) || !Read(stream, vertex.uv.y) || !ReadColor(stream, vertex.color)) return false;
            for (int i = 0; i < 3; ++i)
                if (!std::isfinite(vertex.position[i]) || !std::isfinite(vertex.normal[i])) return false;
        }
        for (auto& index : primitive.indices) if (!Read(stream, index) || index >= vertices) return false;
        if (!Read(stream, scenarios) || scenarios > 1024) return false;
        for (uint32_t i = 0; i < scenarios; ++i)
        {
            std::string name; uint32_t values;
            if (!ReadString(stream, name) || !Read(stream, values) || values != vertices) return false;
            auto& channel = primitive.lightingScenarios[name]; channel.resize(values);
            for (auto& value : channel) if (!ReadVector3(stream, value)) return false;
        }
    }
    if (stream.peek() != std::char_traits<char>::eof()) return false;
    // Do not create GL texture resources until the complete cache has validated.
    for (size_t i = 0; i < primitives.size(); ++i)
    {
        const auto load = [&](int index) { return texturePaths[i][index].empty() ? -1 : AssetManager::LoadTexture(texturePaths[i][index]); };
        primitives[i].textureId = load(0); primitives[i].normalTextureId = load(1); primitives[i].emissionTextureId = load(2);
    }
    output = std::move(primitives); materialSlotNames = std::move(slots);
    return true;
}

bool BuildCombineModel(const YAML::Node& entities, uint64_t rootId,
                       const std::string& cachePath, float cellSize, std::string& error)
{
    error.clear();
    try
    {
        if (cachePath.empty()) { error = "Choose a cache manifest path."; return false; }
        Plan plan;
        if (!MakePlan(entities, rootId, cellSize, plan, error)) return false;
        struct Chunk { YAML::Node source; std::vector<Primitive> primitives; std::vector<std::string> slotNames; };
        std::map<std::string, Chunk> chunks;
        int triangles = 0;
        for (const auto& source : plan.sources)
        {
            const auto modelNode = source.node["Canis::Model"];
            auto* model = AssetManager::GetModel(source.path);
            std::vector<Primitive> primitives;
            if (!model || !model->ExportStaticPrimitives(primitives, modelNode["nodeIndex"].as<int>(-1),
                                                         modelNode["applyNodeTransform"].as<bool>(true)))
            { error = "Source is missing, transparent, animated, or deforming: " + source.path; return false; }
            const float determinant = glm::determinant(glm::mat3(source.matrix));
            if (!std::isfinite(determinant) || std::abs(determinant) < 1e-8f)
            { error = "A source Transform has invalid or zero scale."; return false; }
            const auto normalMatrix = glm::transpose(glm::inverse(glm::mat3(source.matrix)));
            Vector3 minimum(std::numeric_limits<float>::max()), maximum(-std::numeric_limits<float>::max());
            for (auto& primitive : primitives)
            {
                if ((primitive.textureId >= 0 && TexturePath(primitive.textureId).empty()) ||
                    (primitive.normalTextureId >= 0 && TexturePath(primitive.normalTextureId).empty()) ||
                    (primitive.emissionTextureId >= 0 && TexturePath(primitive.emissionTextureId).empty()))
                { error = "A source uses a texture without a stable asset path: " + source.path; return false; }
                for (auto& vertex : primitive.vertices)
                {
                    vertex.position = Vector3(source.matrix * Vector4(vertex.position, 1));
                    const auto normal = normalMatrix * vertex.normal;
                    if (glm::dot(normal, normal) > 1e-12f) vertex.normal = glm::normalize(normal);
                    minimum = glm::min(minimum, vertex.position); maximum = glm::max(maximum, vertex.position);
                }
                if (determinant < 0) for (size_t i = 0; i + 2 < primitive.indices.size(); i += 3)
                    std::swap(primitive.indices[i + 1], primitive.indices[i + 2]);
                triangles += static_cast<int>(primitive.indices.size() / 3);
            }
            YAML::Node keyNode;
            if (source.node["Canis::Material"]) keyNode["material"] = source.node["Canis::Material"];
            keyNode["color"] = modelNode["color"].as<Color>(Color(1));
            keyNode["castShadow"] = modelNode["castShadow"].as<bool>(true);
            keyNode["weatherSurface"] = modelNode["weatherSurface"].as<bool>(false);
            std::string key = Canonical(keyNode);
            if (cellSize > 0)
            {
                const auto center = (minimum + maximum) * .5f;
                for (int axis = 0; axis < 3; ++axis) key += ":" + std::to_string(static_cast<int>(std::floor(center[axis] / cellSize)));
            }
            auto& chunk = chunks[key];
            if (!chunk.source || chunk.source.IsNull())
                chunk.source = source.node;
            for (int i = 0; i < model->GetMaterialSlotCount(); ++i)
            {
                if (chunk.slotNames.size() <= static_cast<size_t>(i))
                    chunk.slotNames.resize(static_cast<size_t>(i) + 1);
                const auto name = model->GetMaterialSlotName(i);
                auto& combinedName = chunk.slotNames[static_cast<size_t>(i)];
                if (combinedName.empty()) combinedName = name;
                else if (combinedName != name) combinedName = "Slot " + std::to_string(i);
            }
            for (auto& primitive : primitives) AppendPrimitive(chunk.primitives, std::move(primitive));
        }
        // The first source import may create texture metadata or unpack an
        // embedded image. Fingerprint the completed import, not missing files
        // observed just before it, so a fresh cache is immediately reusable.
        Plan importedPlan;
        if (!MakePlan(entities, rootId, cellSize, importedPlan, error)) return false;
        if (importedPlan.removed != plan.removed)
        { error = "Source hierarchy changed during the combined build."; return false; }
        plan.fingerprint = importedPlan.fingerprint;
        const auto manifestPath = std::filesystem::path(cachePath);
        if (!manifestPath.parent_path().empty()) std::filesystem::create_directories(manifestPath.parent_path());
        YAML::Node manifest;
        manifest["version"] = CacheVersion;
        manifest["fingerprint"] = plan.fingerprint;
        manifest["sourceModels"] = plan.sources.size();
        manifest["triangles"] = triangles;
        manifest["sourceEntityCount"] = plan.removed.size();
        manifest["outputModels"] = chunks.size();
        manifest["skippedEntities"] = plan.skipped;
        manifest["sourceEntities"] = YAML::Node(YAML::NodeType::Sequence);
        for (auto id : plan.removed) manifest["sourceEntities"].push_back(id);
        manifest["replacements"] = YAML::Node(YAML::NodeType::Sequence);
        manifest["geometryFiles"] = YAML::Node(YAML::NodeType::Sequence);
        std::set<uint64_t> used;
        for (auto node : entities) used.insert(node["Entity"].as<uint64_t>(0));
        size_t index = 0;
        for (const auto& [key, chunk] : chunks)
        {
            const auto modelPath = cachePath + "." + plan.fingerprint + "." + std::to_string(index++) + ".cmodel";
            if (!WriteGeometry(modelPath, chunk.primitives, chunk.slotNames))
            { error = "Could not write combined model cache: " + modelPath; return false; }
            YAML::Node geometryFile;
            geometryFile["path"] = modelPath;
            geometryFile["bytes"] = std::filesystem::file_size(modelPath);
            geometryFile["hash"] = FileHash(modelPath);
            manifest["geometryFiles"].push_back(geometryFile);
            uint64_t id = HashText(HashText(14695981039346656037ULL, std::to_string(rootId)), key) | (uint64_t(1) << 63);
            while (!id || used.contains(id)) ++id;
            used.insert(id);
            YAML::Node replacement;
            replacement["Entity"] = id;
            replacement["Name"] = "Combined model " + std::to_string(index);
            replacement["Active"] = true;
            auto transform = replacement["Canis::Transform"];
            transform["active"] = true; transform["position"] = Vector3(0);
            transform["rotation"] = Quaternion(Vector3(0)); transform["scale"] = Vector3(1);
            transform["parent"] = rootId; transform["children"] = YAML::Node(YAML::NodeType::Sequence);
            replacement["Canis::Model"] = YAML::Clone(chunk.source["Canis::Model"]);
            auto modelNode = replacement["Canis::Model"];
            modelNode.remove("modelUUID"); modelNode.remove("modelPath");
            modelNode["ModelAsset"] = YAML::Node(YAML::NodeType::Map);
            modelNode["ModelAsset"]["path"] = modelPath;
            modelNode["nodeIndex"] = -1; modelNode["applyNodeTransform"] = false;
            modelNode["static"] = true; modelNode["bakedVisibility"] = false;
            if (chunk.source["Canis::Material"])
                replacement["Canis::Material"] = YAML::Clone(chunk.source["Canis::Material"]);
            manifest["replacements"].push_back(replacement);
        }
        std::ofstream file(cachePath + ".tmp", std::ios::trunc);
        file << manifest; file.flush();
        if (!file) { error = "Could not write cache manifest."; return false; }
        file.close(); std::filesystem::rename(cachePath + ".tmp", cachePath);
        return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool LoadCombineModelReplacement(const YAML::Node& entities, uint64_t rootId,
                                const std::string& cachePath, float cellSize,
                                YAML::Node& replacements, std::unordered_set<uint64_t>& removed,
                                CombineModelSummary& summary, std::string& error)
{
    error.clear(); replacements = YAML::Node(YAML::NodeType::Sequence); removed.clear(); summary = {};
    try
    {
        Plan plan;
        if (!MakePlan(entities, rootId, cellSize, plan, error)) return false;
        if (cachePath.empty() || !std::filesystem::is_regular_file(cachePath))
        { error = "No combined cache. Build it in the CombineModel Inspector."; return false; }
        const auto manifest = YAML::LoadFile(cachePath);
        if (manifest["version"].as<uint32_t>(0) != CacheVersion ||
            manifest["fingerprint"].as<std::string>("") != plan.fingerprint)
        { error = "Combined cache is stale. Rebuild it in the CombineModel Inspector."; return false; }
        std::set<uint64_t> encoded;
        for (auto id : manifest["sourceEntities"]) encoded.insert(id.as<uint64_t>());
        if (encoded != plan.removed || !manifest["replacements"].IsSequence() ||
            manifest["replacements"].size() == 0)
        { error = "Combined cache source list is invalid."; return false; }
        std::set<uint64_t> replacementIds;
        std::set<uint64_t> authoredIds;
        for (auto entity : entities) authoredIds.insert(entity["Entity"].as<uint64_t>(0));
        std::map<std::string, YAML::Node> geometryFiles;
        if (!manifest["geometryFiles"].IsSequence())
        { error = "Combined cache geometry checksums are missing."; return false; }
        for (auto file : manifest["geometryFiles"])
            geometryFiles.emplace(file["path"].as<std::string>(), file);
        for (auto replacement : manifest["replacements"])
        {
            const auto id = replacement["Entity"].as<uint64_t>(0);
            const auto path = ModelPath(replacement["Canis::Model"]);
            std::ifstream model(path, std::ios::binary); std::array<char, 8> magic{}; uint32_t version;
            if (!id || authoredIds.contains(id) || !replacementIds.insert(id).second ||
                replacement["Canis::Transform"]["parent"].as<uint64_t>(0) != rootId ||
                !model.read(magic.data(), magic.size()) || std::memcmp(magic.data(), "CANISCM1", 8) ||
                !Read(model, version) || version != CacheVersion)
            { error = "Combined cache geometry is missing or invalid."; return false; }
            const auto file = geometryFiles.find(path);
            if (file == geometryFiles.end() || std::filesystem::file_size(path) != file->second["bytes"].as<uint64_t>(0) ||
                FileHash(path) != file->second["hash"].as<std::string>(""))
            { error = "Combined cache geometry is corrupt. Rebuild the combined models."; return false; }
        }
        replacements = YAML::Clone(manifest["replacements"]);
        removed.insert(plan.removed.begin(), plan.removed.end());
        summary.sourceModels = manifest["sourceModels"].as<int>(0);
        summary.combinedModels = static_cast<int>(replacements.size());
        summary.triangleCount = manifest["triangles"].as<int>(0);
        summary.skippedEntities = plan.skipped;
        return true;
    }
    catch (const std::exception& exception) { error = exception.what(); return false; }
}

bool RebuildCombineModel(Entity& entity, std::string& error)
{
    if (!entity.HasComponent<CombineModel>()) { error = "Entity has no CombineModel component."; return false; }
    auto& component = entity.GetComponent<CombineModel>();
    if (component.cachePath.empty()) component.cachePath = "Library/CombinedModels/" + std::to_string(uint64_t(entity.GetUUID())) + "/model.manifest";
    YAML::Node entities(YAML::NodeType::Sequence);
    for (auto* source : entity.scene.GetEntities()) if (source) entities.push_back(entity.scene.EncodeEntity(*source));
    if (!BuildCombineModel(entities, uint64_t(entity.GetUUID()), component.cachePath, component.cellSize, error))
    { component.status = error; return false; }
    YAML::Node replacements; std::unordered_set<uint64_t> removed; CombineModelSummary summary;
    if (!LoadCombineModelReplacement(entities, uint64_t(entity.GetUUID()), component.cachePath, component.cellSize,
                                     replacements, removed, summary, error))
    { component.status = error; return false; }
    component.sourceModels = summary.sourceModels; component.combinedModels = summary.combinedModels;
    component.triangleCount = summary.triangleCount; component.status = "Cache ready. Child models are replaced when Play starts.";
    return true;
}

void RegisterCombineModel(App& app)
{
    static ComponentConf configuration;
    configuration = {};
    REGISTER_PROPERTY(configuration, CombineModel, active);
    REGISTER_PROPERTY(configuration, CombineModel, cachePath);
    REGISTER_PROPERTY(configuration, CombineModel, cellSize);
    DEFAULT_COMPONENT_CONFIG_AND_REQUIRED(configuration, CombineModel, Transform);
    configuration.DrawInspector = [](Editor& editor, Entity& entity, const ScriptConf& definition)
    {
        auto& component = entity.GetComponent<CombineModel>();
        DrawInspectorField(editor, "active", definition.name.c_str(), component.active);
        DrawInspectorField(editor, "cachePath", definition.name.c_str(), component.cachePath);
        DrawInspectorField(editor, "cellSize", definition.name.c_str(), component.cellSize);
        component.cellSize = std::max(0.0f, component.cellSize);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 combines the hierarchy by material. A positive size retains separate cells for frustum culling.");
        ImGui::TextWrapped("Static render-only children stay editable here. Play loads their compiled models instead of creating the child objects.");
        if (ImGui::Button("Build / rebuild combined models"))
        {
            std::string error;
            if (!RebuildCombineModel(entity, error)) Debug::Warning("CombineModel: %s", error.c_str());
        }
        EditorUI::PropertyLabel("source models"); ImGui::Text("%d", component.sourceModels);
        EditorUI::PropertyLabel("combined models"); ImGui::Text("%d", component.combinedModels);
        EditorUI::PropertyLabel("triangles"); ImGui::Text("%d", component.triangleCount);
        if (!component.status.empty()) ImGui::TextWrapped("%s", component.status.c_str());
    };
    app.RegisterComponent(configuration);
}
}
