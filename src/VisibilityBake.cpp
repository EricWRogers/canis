#include <Canis/VisibilityBake.hpp>
#include <Canis/AssetPipeline.hpp>
#include <zlib.h>
#include <map>
#include <fstream>
#include <tuple>
#include <algorithm>
namespace Canis
{
    bool TriangleOccludesCell(Vector3 eyeMin, Vector3 eyeMax, Vector3 lo, Vector3 hi, Vector3 a, Vector3 b,
                              Vector3 c, float epsilon)
    {
        auto normal = glm::cross(b - a, c - a);
        float len = glm::length(normal);
        if (len < 1e-7f)
            return false;
        normal /= len;
        Vector3 eyes[8];
        bool allPositive = true, allNegative = true;
        for (int i = 0; i < 8; i++)
        {
            eyes[i] = {i & 1 ? eyeMax.x : eyeMin.x, i & 2 ? eyeMax.y : eyeMin.y, i & 4 ? eyeMax.z : eyeMin.z};
            float d = glm::dot(eyes[i] - a, normal);
            allPositive &= d > epsilon;
            allNegative &= d < -epsilon;
        }
        if (!allPositive && !allNegative)
            return false;
        if (allNegative)
            normal = -normal;
        auto center = (lo + hi) * .5f, extent = (hi - lo) * .5f;
        if (glm::dot(center - a, normal) + glm::dot(extent, glm::abs(normal)) >= -epsilon)
            return false;
        for (auto eye : eyes)
            for (auto edge : std::array<std::array<Vector3, 3>, 3>{{{{a, b, c}}, {{b, c, a}}, {{c, a, b}}}})
            {
                auto side = glm::cross(edge[0] - eye, edge[1] - eye);
                float length = glm::length(side);
                if (length < 1e-7f)
                    return false;
                side /= length;
                if (glm::dot(side, edge[2] - eye) < 0)
                    side = -side;
                if (glm::dot(center - eye, side) - glm::dot(extent, glm::abs(side)) <= epsilon)
                    return false;
            }
        return true;
    }
    static uint32_t FileCRC(std::filesystem::path p, uint32_t seed = 0)
    {
        std::ifstream f(p, std::ios::binary);
        char bytes[65536];
        auto crc = seed;
        while (f)
        {
            f.read(bytes, sizeof(bytes));
            crc = crc32(crc, reinterpret_cast<const Bytef*>(bytes), f.gcount());
        }
        return crc;
    }
    void BakeStaticVisibility(const std::filesystem::path& source, const std::filesystem::path& output,
                              const VisibilityBakeOptions& options)
    {
        if (options.chunkSize <= 0 || options.cellSize <= 0 ||
            glm::any(glm::lessThanEqual(options.dimensions, glm::ivec3(0))))
            throw std::runtime_error("Invalid native visibility bake settings");
        auto gltf = Pipeline::ReadGltf(source);
        if (gltf.json.contains("animations") && !gltf.json["animations"].empty())
            throw std::runtime_error("Animated meshes cannot be baked as static visibility");
        auto mesh = Pipeline::Flatten(gltf);
        using Key = std::tuple<int, int, int, int>;
        std::map<Key, Pipeline::Primitive> parts;
        struct Face
        {
            Vector3 a, b, c;
            float area;
        };
        std::vector<Face> faces;
        int retainedPrimitive = 0;
        for (auto& p : mesh.primitives)
        {
            bool opaque = p.material < 0 ||
                          mesh.materials[p.material].value("alphaMode", std::string("OPAQUE")) == "OPAQUE";
            if (!opaque)
            {
                parts[{p.material, INT_MAX, retainedPrimitive++, 0}] = p;
                continue;
            }
            for (size_t i = 0; i + 2 < p.indices.size(); i += 3)
            {
                auto a = p.vertices.at(p.indices[i]), b = p.vertices.at(p.indices[i + 1]),
                     c = p.vertices.at(p.indices[i + 2]);
                auto center = glm::floor((a.position + b.position + c.position) / (3 * options.chunkSize));
                auto& out = parts[{p.material, int(center.x), int(center.y), int(center.z)}];
                out.material = p.material;
                for (auto v : {a, b, c})
                {
                    out.indices.push_back(out.vertices.size());
                    out.vertices.push_back(v);
                }
                faces.push_back({a.position, b.position, c.position,
                                 glm::length(glm::cross(b.position - a.position, c.position - a.position))});
            }
        }
        mesh.primitives.clear();
        std::vector<std::pair<Vector3, Vector3>> bounds;
        for (auto& [key, p] : parts)
        {
            Vector3 lo(FLT_MAX), hi(-FLT_MAX);
            for (auto v : p.vertices)
            {
                lo = glm::min(lo, v.position);
                hi = glm::max(hi, v.position);
            }
            bounds.emplace_back(lo, hi);
            mesh.primitives.push_back(std::move(p));
        }
        Pipeline::WriteGltf(mesh, output);
        Pipeline::WriteMeta(output, "MODEL");
        uint32_t sourceCrc = FileCRC(source);
        for (auto b : gltf.json["buffers"])
            if (b.contains("uri"))
                sourceCrc = FileCRC(source.parent_path() / b["uri"].get<std::string>(), sourceCrc);
        auto bin = output;
        bin.replace_extension(".bin");
        auto crc = FileCRC(bin, FileCRC(output));
        Pipeline::Json data = {
            {"version", 1},
            {"sourceCrc32", sourceCrc},
            {"crc32", crc},
            {"minimum", {options.minimum.x, options.minimum.y, options.minimum.z}},
            {"dimensions", {options.dimensions.x, options.dimensions.y, options.dimensions.z}},
            {"cellSize", options.cellSize},
            {"bounds", Pipeline::Json::array()},
            {"hidden", Pipeline::Json::array()}};
        for (auto [lo, hi] : bounds)
            data["bounds"].push_back({{lo.x, lo.y, lo.z}, {hi.x, hi.y, hi.z}});
        std::sort(faces.begin(), faces.end(), [](auto a, auto b) { return a.area > b.area; });
        if (faces.size() > size_t(options.occluders))
            faces.resize(options.occluders);
        for (int x = 0; x < options.dimensions.x; x++)
            for (int y = 0; y < options.dimensions.y; y++)
                for (int z = 0; z < options.dimensions.z; z++)
                {
                    Vector3 eye = options.minimum + Vector3(x, y, z) * options.cellSize;
                    Pipeline::Json hidden = Pipeline::Json::array();
                    for (size_t i = 0; i < bounds.size(); i++)
                        for (const auto& face : faces)
                            if (TriangleOccludesCell(eye, eye + Vector3(options.cellSize), bounds[i].first,
                                                     bounds[i].second, face.a, face.b, face.c))
                            {
                                hidden.push_back(i);
                                break;
                            }
                    data["hidden"].push_back(hidden);
                }
        std::ofstream(output.string() + ".visibility") << data.dump();
    }
} // namespace Canis
