#include <Canis/AssetPipeline.hpp>
#include <Canis/Asset.hpp>
#include <algorithm>
#include <cctype>
#include <Canis/Math.hpp>
#include <fstream>
#include <cstring>
#include <stdexcept>
#include <yaml-cpp/yaml.h>
namespace Canis::Pipeline
{
    static std::vector<uint8_t> Bytes(const std::filesystem::path& p)
    {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            throw std::runtime_error("Cannot read " + p.string());
        return {std::istreambuf_iterator<char>(f), {}};
    }
    Gltf ReadGltf(const std::filesystem::path& p)
    {
        Gltf s;
        s.path = p;
        auto data = Bytes(p);
        // Parse JSON as character text; libc++ has no char_traits<unsigned char>.
        if (data.size() > 12 && std::memcmp(data.data(), "glTF", 4) == 0)
        {
            size_t cursor = 12;
            while (cursor + 8 <= data.size())
            {
                uint32_t length, type;
                std::memcpy(&length, data.data() + cursor, 4);
                std::memcpy(&type, data.data() + cursor + 4, 4);
                cursor += 8;
                if (cursor + length > data.size())
                    throw std::runtime_error("Invalid GLB chunk");
                if (type == 0x4e4f534a)
                    s.json = Json::parse(std::string(data.begin() + cursor, data.begin() + cursor + length));
                if (type == 0x004e4942)
                    s.buffers.emplace_back(data.begin() + cursor, data.begin() + cursor + length);
                cursor += length;
            }
        }
        else
            s.json = Json::parse(std::string(data.begin(), data.end()));
        for (auto b : s.json.value("buffers", Json::array()))
            if (b.contains("uri"))
            {
                std::string uri = b["uri"];
                if (uri.starts_with("data:"))
                    throw std::runtime_error("Externalize data URI buffers before baking");
                s.buffers.push_back(Bytes(p.parent_path() / uri));
            }
        return s;
    }
    static int Components(const std::string& t)
    {
        return t == "SCALAR" ? 1 : t == "VEC2" ? 2 : t == "VEC3" ? 3 : t == "VEC4" ? 4 : t == "MAT4" ? 16 : 0;
    }
    std::vector<float> Attribute(const Gltf& s, int id)
    {
        const auto& a = s.json.at("accessors").at(id);
        const auto& v = s.json.at("bufferViews").at(a.at("bufferView").get<int>());
        int n = Components(a.at("type")), kind = a.at("componentType"),
            size = kind == 5126 || kind == 5125   ? 4
                   : kind == 5123 || kind == 5122 ? 2
                                                  : 1;
        size_t count = a.at("count"), offset = v.value("byteOffset", 0) + a.value("byteOffset", 0),
               stride = v.value("byteStride", size * n);
        const auto& b = s.buffers.at(v.value("buffer", 0));
        if (!n || offset + ((count ? count - 1 : 0) * stride) + n * size > b.size())
            throw std::runtime_error("Invalid glTF accessor range");
        std::vector<float> out(count * n);
        bool normalized = a.value("normalized", false);
        for (size_t i = 0; i < count; i++)
            for (int c = 0; c < n; c++)
            {
                const uint8_t* p = b.data() + offset + i * stride + c * size;
                float value = 0;
                if (kind == 5126)
                    std::memcpy(&value, p, 4);
                else if (kind == 5125)
                {
                    uint32_t q;
                    std::memcpy(&q, p, 4);
                    value = float(q);
                }
                else if (kind == 5123)
                {
                    uint16_t q;
                    std::memcpy(&q, p, 2);
                    value = normalized ? q / 65535.f : float(q);
                }
                else if (kind == 5122)
                {
                    int16_t q;
                    std::memcpy(&q, p, 2);
                    value = normalized ? std::max(-1.f, q / 32767.f) : float(q);
                }
                else if (kind == 5121)
                    value = normalized ? *p / 255.f : float(*p);
                else
                    throw std::runtime_error("Unsupported accessor component");
                out[i * n + c] = value;
            }
        return out;
    }
    std::vector<uint32_t> Indices(const Gltf& s, int id)
    {
        auto values = Attribute(s, id);
        return {values.begin(), values.end()};
    }
    Matrix4 NodeMatrix(const Json& n)
    {
        Matrix4 m(1);
        if (n.contains("matrix"))
        {
            for (int i = 0; i < 16; i++)
                m[i / 4][i % 4] = n["matrix"][i];
            return m;
        }
        Vector3 p(0), scale(1);
        Quaternion q(1, 0, 0, 0);
        if (n.contains("translation"))
            for (int i = 0; i < 3; i++)
                p[i] = n["translation"][i];
        if (n.contains("scale"))
            for (int i = 0; i < 3; i++)
                scale[i] = n["scale"][i];
        if (n.contains("rotation"))
        {
            auto r = n["rotation"];
            q = Quaternion(r[3], r[0], r[1], r[2]);
        }
        return glm::translate(m, p) * glm::mat4_cast(glm::normalize(q)) * glm::scale(m, scale);
    }
    Mesh Flatten(const Gltf& s, int selected, bool apply)
    {
        if (s.json.contains("skins") && !s.json["skins"].empty())
            throw std::runtime_error("Skinned meshes require the animation renderer");
        Mesh out;
        out.materials = s.json.value("materials", Json::array());
        out.images = s.json.value("images", Json::array());
        out.textures = s.json.value("textures", Json::array());
        out.samplers = s.json.value("samplers", Json::array());
        for (auto& img : out.images)
            if (img.contains("uri") && !img["uri"].get<std::string>().starts_with("data:"))
                img["uri"] =
                    std::filesystem::absolute(s.path.parent_path() / img["uri"].get<std::string>()).lexically_normal().string();
        auto visit = [&](auto&& self, int id, Matrix4 parent) -> void
        {
            const auto& node = s.json["nodes"][id];
            Matrix4 matrix = parent * NodeMatrix(node);
            if (node.contains("mesh") && (selected < 0 || selected == id))
            {
                auto transform = apply ? matrix : Matrix4(1);
                auto normal = glm::transpose(glm::inverse(glm::mat3(transform)));
                for (auto part : s.json["meshes"][node["mesh"].get<int>()]["primitives"])
                {
                    if (part.value("mode", 4) != 4)
                        continue;
                    Primitive p;
                    p.material = part.value("material", -1);
                    auto attr = part["attributes"];
                    auto positions = Attribute(s, attr["POSITION"]);
                    std::vector<float> normals, uv, colors;
                    if (attr.contains("NORMAL"))
                        normals = Attribute(s, attr["NORMAL"]);
                    if (attr.contains("TEXCOORD_0"))
                        uv = Attribute(s, attr["TEXCOORD_0"]);
                    if (attr.contains("COLOR_0"))
                        colors = Attribute(s, attr["COLOR_0"]);
                    size_t count = positions.size() / 3;
                    int cn = colors.empty() ? 0 : int(colors.size() / count);
                    p.vertices.resize(count);
                    for (size_t i = 0; i < count; i++)
                    {
                        auto& v = p.vertices[i];
                        v.position = Vector3(transform * Vector4(positions[i * 3], positions[i * 3 + 1],
                                                                 positions[i * 3 + 2], 1));
                        if (!normals.empty())
                            v.normal = glm::normalize(
                                normal * Vector3(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]));
                        if (!uv.empty())
                            v.uv = {uv[i * 2], uv[i * 2 + 1]};
                        if (cn >= 3)
                            v.color = {colors[i * cn], colors[i * cn + 1], colors[i * cn + 2],
                                       cn == 4 ? colors[i * cn + 3] : 1};
                    }
                    if (part.contains("indices"))
                        p.indices = Indices(s, part["indices"]);
                    else
                        for (uint32_t i = 0; i < count; i++)
                            p.indices.push_back(i);
                    if (glm::determinant(transform) < 0)
                        for (size_t i = 0; i + 2 < p.indices.size(); i += 3)
                            std::swap(p.indices[i + 1], p.indices[i + 2]);
                    out.primitives.push_back(std::move(p));
                }
            }
            for (auto child : node.value("children", Json::array()))
                self(self, child, matrix);
        };
        for (auto root : s.json["scenes"][s.json.value("scene", 0)]["nodes"])
            visit(visit, root, Matrix4(1));
        return out;
    }
    Mesh ReadMesh(const std::filesystem::path& path, int node, bool applyNode)
    {
        try
        {
            auto extension = path.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            if (extension == ".gltf" || extension == ".glb")
                return Flatten(ReadGltf(path), node, applyNode);
            if (extension != ".obj")
                throw std::runtime_error("Unsupported static model format: " + extension);

            std::vector<ModelAsset::PrimitiveBuild3D> primitives;
            std::vector<std::string> slots;
            if (!ModelAsset::ReadOBJGeometry(path.string(), primitives, slots))
                throw std::runtime_error("Cannot load OBJ triangle geometry");
            Mesh mesh;
            // OBJ has no selectable scene nodes; material names define its submesh slots.
            for (const auto& slot : slots)
                mesh.materials.push_back({{"name", slot}});
            if (slots.empty())
                mesh.materials.push_back(Json::object());
            if (node >= 0)
                return mesh;
            for (const auto& source : primitives)
            {
                Primitive primitive;
                primitive.material = std::max(0, source.materialSlot);
                primitive.indices.assign(source.indices.begin(), source.indices.end());
                for (const auto& v : source.vertices)
                    primitive.vertices.push_back({v.position, v.normal, v.uv, v.color});
                mesh.primitives.push_back(std::move(primitive));
            }
            return mesh;
        }
        catch (const std::exception& error)
        {
            throw std::runtime_error("Cannot import static model '" + path.string() + "': " + error.what());
        }
    }
    void WriteGltf(const Mesh& mesh, const std::filesystem::path& path, bool baked)
    {
        std::filesystem::create_directories(path.parent_path());
        Json g = {{"asset", {{"version", "2.0"}, {"generator", "Canis native asset pipeline"}}},
                  {"scene", 0},
                  {"scenes", Json::array({{{"nodes", Json::array({0})}}})},
                  {"nodes", Json::array({{{"mesh", 0}}})},
                  {"meshes", Json::array({{{"primitives", Json::array()}}})},
                  {"accessors", Json::array()},
                  {"bufferViews", Json::array()}};
        g["materials"] = mesh.materials;
        g["images"] = mesh.images;
        g["textures"] = mesh.textures;
        g["samplers"] = mesh.samplers;
        for (auto& img : g["images"])
            if (img.contains("uri"))
            {
                std::filesystem::path p = img["uri"].get<std::string>();
                if (p.is_absolute())
                    img["uri"] = std::filesystem::relative(p, path.parent_path()).generic_string();
            }
        std::vector<uint8_t> binary;
        auto add = [&](const void* data, size_t bytes, size_t count, int components, bool indices = false)
        {
            while (binary.size() % 4)
                binary.push_back(0);
            size_t offset = binary.size();
            const auto* begin = static_cast<const uint8_t*>(data);
            binary.insert(binary.end(), begin, begin + bytes);
            g["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offset}, {"byteLength", bytes}});
            Json a = {{"bufferView", g["bufferViews"].size() - 1},
                      {"componentType", indices ? 5125 : 5126},
                      {"count", count},
                      {"type", components == 1 ? "SCALAR" : "VEC" + std::to_string(components)}};
            if (components == 3 && !indices)
            {
                const float* values = static_cast<const float*>(data);
                Vector3 lo(FLT_MAX), hi(-FLT_MAX);
                for (size_t i = 0; i < count; i++)
                {
                    Vector3 v(values[i * 3], values[i * 3 + 1], values[i * 3 + 2]);
                    lo = glm::min(lo, v);
                    hi = glm::max(hi, v);
                }
                a["min"] = {lo.x, lo.y, lo.z};
                a["max"] = {hi.x, hi.y, hi.z};
            }
            g["accessors"].push_back(a);
            return int(g["accessors"].size() - 1);
        };
        for (const auto& p : mesh.primitives)
        {
            if (p.vertices.empty() || p.indices.empty())
                continue;
            std::vector<float> positions, normals, uv, colors;
            for (auto v : p.vertices)
            {
                positions.insert(positions.end(), {v.position.x, v.position.y, v.position.z});
                normals.insert(normals.end(), {v.normal.x, v.normal.y, v.normal.z});
                uv.insert(uv.end(), {v.uv.x, v.uv.y});
                colors.insert(colors.end(), {v.color.x, v.color.y, v.color.z, v.color.w});
            }
            Json a = {{"POSITION", add(positions.data(), positions.size() * 4, p.vertices.size(), 3)},
                      {"NORMAL", add(normals.data(), normals.size() * 4, p.vertices.size(), 3)},
                      {"TEXCOORD_0", add(uv.data(), uv.size() * 4, p.vertices.size(), 2)},
                      {"COLOR_0", add(colors.data(), colors.size() * 4, p.vertices.size(), 4)}};
            Json part = {{"attributes", a},
                         {"indices", add(p.indices.data(), p.indices.size() * 4, p.indices.size(), 1, true)}};
            if (p.material >= 0)
                part["material"] = p.material;
            if (baked)
                part["extras"] = {{"canisBakedLighting", true}};
            g["meshes"][0]["primitives"].push_back(part);
        }
        auto bin = path;
        bin.replace_extension(".bin");
        g["buffers"] = Json::array({{{"uri", bin.filename().string()}, {"byteLength", binary.size()}}});
        std::ofstream bf(bin, std::ios::binary);
        bf.write(reinterpret_cast<const char*>(binary.data()), binary.size());
        std::ofstream f(path);
        f << g.dump();
        if (!f || !bf)
            throw std::runtime_error("Failed to write " + path.string());
    }
    uint64_t AssetUUID(const std::string& key)
    {
        uint64_t h = 1469598103934665603ULL;
        for (unsigned char c : key)
        {
            h ^= c;
            h *= 1099511628211ULL;
        }
        return h ? h : 1;
    }
    void WriteMeta(const std::filesystem::path& p, const char* kind)
    {
        const auto metadataPath = p.string() + ".meta";
        // Texture import and slicing are user-authored, and survive reimport.
        // Refuse malformed metadata rather than replacing those settings.
        if (std::string(kind) == "TEXTURE" && std::filesystem::exists(metadataPath))
        {
            auto metadata = YAML::LoadFile(metadataPath);
            if (!metadata.IsMap())
                throw std::runtime_error("Texture metadata must be a map: " + metadataPath);
            metadata["FileType"] = kind;
            if (!metadata["UUID"]) metadata["UUID"] = AssetUUID(p.generic_string());
            metadata["name"] = p.stem().string();
            metadata["extension"] = p.extension().string().substr(1);
            metadata["size"] = std::filesystem::file_size(p);
            metadata["modified"] = 0;
            std::ofstream out(metadataPath);
            out << metadata;
            if (!out) throw std::runtime_error("Cannot write " + metadataPath);
            return;
        }
        std::ofstream out(metadataPath);
        out << "FileType: " << kind << "\nUUID: " << AssetUUID(p.generic_string())
            << "\nname: " << p.stem().string() << "\nextension: " << p.extension().string().substr(1)
            << "\nsize: " << std::filesystem::file_size(p) << "\nmodified: 0\n";
        if (!out) throw std::runtime_error("Cannot write " + metadataPath);
    }
} // namespace Canis::Pipeline
