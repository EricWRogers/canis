#pragma once
#include <Canis/Math.hpp>
#include <Canis/External/tinygltf/json.hpp>
#include <filesystem>
#include <vector>
#include <cstdint>
namespace Canis::Pipeline
{
    using Json = nlohmann::json;
    struct Vertex
    {
        Vector3 position{0}, normal{0, 1, 0};
        Vector2 uv{0};
        Vector4 color{1};
    };
    struct Primitive
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        int material = -1;
    };
    struct Mesh
    {
        std::vector<Primitive> primitives;
        Json materials = Json::array(), images = Json::array(), textures = Json::array(),
             samplers = Json::array();
    };
    struct Gltf
    {
        Json json;
        std::vector<std::vector<uint8_t>> buffers;
        std::filesystem::path path;
    };
    Gltf ReadGltf(const std::filesystem::path& _path);
    std::vector<float> Attribute(const Gltf& _source, int _accessor);
    std::vector<uint32_t> Indices(const Gltf& _source, int _accessor);
    Matrix4 NodeMatrix(const Json& _node);
    Mesh Flatten(const Gltf& _source, int _node = -1, bool _applyNode = true);
    // CPU-only static geometry import for all native model formats (OBJ, glTF, GLB).
    Mesh ReadMesh(const std::filesystem::path& _path, int _node = -1, bool _applyNode = true);
    void WriteGltf(const Mesh& _mesh, const std::filesystem::path& _path, bool _bakedLighting = false);
    uint64_t AssetUUID(const std::string& _key);
    void WriteMeta(const std::filesystem::path& _path, const char* _kind);
} // namespace Canis::Pipeline
