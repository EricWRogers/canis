#include <Canis/Blockout.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>

namespace
{
    bool ValidateShape(int _type)
    {
        Canis::BlockoutShape shape = {};
        shape.type = _type;
        shape.size = Canis::Vector3(3.0f, 2.0f, 4.0f);
        shape.sides = 12;
        shape.stepCount = 7;

        Canis::BlockoutMeshData mesh = {};
        std::string error = {};
        if (!Canis::BuildBlockoutMesh(shape, mesh, &error))
        {
            std::cerr << "Shape " << _type << " failed: " << error << '\n';
            return false;
        }
        if (mesh.vertices.empty() || mesh.indices.empty() || (mesh.indices.size() % 3u) != 0u)
            return false;
        for (u32 index : mesh.indices)
        {
            if (index >= mesh.vertices.size())
                return false;
        }
        for (const Canis::ModelAsset::RenderVertex3D &vertex : mesh.vertices)
        {
            if (!std::isfinite(vertex.position.x) || !std::isfinite(vertex.position.y) ||
                !std::isfinite(vertex.position.z) || !std::isfinite(vertex.normal.x) ||
                !std::isfinite(vertex.normal.y) || !std::isfinite(vertex.normal.z) ||
                !std::isfinite(vertex.uv.x) || !std::isfinite(vertex.uv.y))
                return false;
            const float normalLength = glm::length(vertex.normal);
            if (normalLength < 0.99f || normalLength > 1.01f)
                return false;
        }
        return mesh.boundsMin.x <= mesh.boundsMax.x && mesh.boundsMin.y <= mesh.boundsMax.y &&
               mesh.boundsMin.z <= mesh.boundsMax.z;
    }
} // namespace

int main()
{
    auto require = [](bool ok, const std::string &message) {
        if (!ok)
        {
            std::cerr << message << '\n';
            std::exit(1);
        }
    };
    auto closed = [](const Canis::EditableBlockoutMesh &mesh) {
        std::map<std::pair<u32, u32>, int> edges;
        for (const auto &f : mesh.faces)
            for (size_t i = 0; i < f.vertices.size(); ++i)
                ++edges[std::minmax(f.vertices[i], f.vertices[(i + 1) % f.vertices.size()])];
        for (auto [edge, count] : edges)
            if (count != 2)
                return false;
        return !edges.empty();
    };
    std::string topologyError;
    Canis::BlockoutShape editable;
    require(Canis::ConvertBlockoutToEditable(editable, topologyError), topologyError);
    require(editable.editMesh.vertices.size() == 8 && editable.editMesh.faces.size() == 6,
            "Primitive conversion must preserve six quads.");
    require(closed(editable.editMesh), "Converted box must be closed.");
    auto movedCorner = editable.editMesh;
    movedCorner.vertices[0] += Canis::Vector3(0.1f, 0.15f, 0.1f);
    require(Canis::ValidateBlockoutTopology(movedCorner, topologyError), "Single vertex movement must work.");
    const auto edge = editable.editMesh.faces[0].vertices;
    require(Canis::CutBlockoutLoop(editable.editMesh, edge[0], edge[1], 2, topologyError), topologyError);
    require(closed(editable.editMesh), "Loop cut introduced cracks.");
    Canis::BlockoutMeshData cutRender;
    require(Canis::BuildBlockoutMesh(editable, cutRender, &topologyError), topologyError);
    require(cutRender.boundsMin == Canis::Vector3(-0.5f, 0, -0.5f) &&
                cutRender.boundsMax == Canis::Vector3(0.5f, 1, 0.5f),
            "Cuts must preserve bounds.");
    require(Canis::ExtrudeBlockoutFaces(editable.editMesh, {0}, 1.5f, topologyError), topologyError);
    require(closed(editable.editMesh), "Extruding a cut section introduced cracks.");
    require(Canis::BuildBlockoutMesh(editable, cutRender, &topologyError), topologyError);
    require(std::abs(cutRender.boundsMax.y - 2.5f) < 0.001f, "Extruding top section must increase height.");
    for (const auto &v : cutRender.vertices)
        require(std::isfinite(v.normal.x) && std::isfinite(v.normal.y), "Invalid edited normal.");
    auto original = editable.editMesh;
    require(!Canis::CutBlockoutLoop(editable.editMesh, 99999, 99998, 1, topologyError),
            "Invalid edge should be rejected.");
    require(editable.editMesh.vertices == original.vertices &&
                editable.editMesh.faces.size() == original.faces.size(),
            "Failed edits must be atomic.");
    Canis::BlockoutShape region;
    require(Canis::ConvertBlockoutToEditable(region, topologyError), topologyError);
    auto ringEdge = region.editMesh.faces[0].vertices;
    require(Canis::CutBlockoutLoop(region.editMesh, ringEdge[0], ringEdge[1], 1, topologyError),
            topologyError);
    std::vector<u32> top;
    for (u32 i = 0; i < region.editMesh.faces.size(); ++i)
    {
        bool atTop = true;
        for (u32 v : region.editMesh.faces[i].vertices)
            atTop &= std::abs(region.editMesh.vertices[v].y - 1) < 0.001f;
        if (atTop && !region.editMesh.faces[i].vertices.empty())
            top.push_back(i);
    }
    require(top.size() == 2, "Expected two top sections.");
    auto dissolved = region.editMesh;
    for (size_t i = 0; i < dissolved.faces[top[0]].vertices.size(); ++i)
    {
        auto a = dissolved.faces[top[0]].vertices[i], b = dissolved.faces[top[0]].vertices[(i + 1) % 4];
        auto &v = dissolved.faces[top[1]].vertices;
        if (std::find(v.begin(), v.end(), a) != v.end() && std::find(v.begin(), v.end(), b) != v.end())
        {
            require(Canis::DissolveBlockoutEdge(dissolved, a, b, topologyError), topologyError);
            break;
        }
    }
    require(closed(dissolved), "Dissolve broke neighboring topology.");
    require(Canis::ExtrudeBlockoutFaces(region.editMesh, top, 1, topologyError), topologyError);
    require(closed(region.editMesh), "Region extrusion must not create internal walls.");
    constexpr std::array<int, 5> types = {
        Canis::BlockoutShapeType::BOX,      Canis::BlockoutShapeType::PLANE,  Canis::BlockoutShapeType::RAMP,
        Canis::BlockoutShapeType::CYLINDER, Canis::BlockoutShapeType::STAIRS,
    };
    for (const int type : types)
    {
        if (!ValidateShape(type))
        {
            std::cerr << "Blockout geometry validation failed for type " << type << '\n';
            return 1;
        }
        Canis::BlockoutShape source;
        source.type = type;
        require(Canis::ConvertBlockoutToEditable(source, topologyError),
                "Convert shape " + std::to_string(type) + ": " + topologyError);
        require(closed(source.editMesh),
                "Primitive conversion left open boundaries: " + std::to_string(type));
    }

    Canis::BlockoutShape editedBox = {};
    editedBox.type = Canis::BlockoutShapeType::BOX;
    editedBox.size = Canis::Vector3(2.0f, 3.0f, 4.0f);
    editedBox.loopCutsX = 1;
    editedBox.loopCutsY = 2;
    editedBox.loopCutsZ = 3;
    editedBox.extrudeNegative = Canis::Vector3(0.5f, 1.0f, 1.5f);
    editedBox.extrudePositive = Canis::Vector3(2.0f, 2.5f, 3.0f);
    Canis::BlockoutMeshData editedMesh = {};
    std::string editedError = {};
    if (!Canis::BuildBlockoutMesh(editedBox, editedMesh, &editedError))
    {
        std::cerr << "Edited box failed: " << editedError << '\n';
        return 1;
    }
    if (editedMesh.vertices.size() != 208u || editedMesh.indices.size() != 312u ||
        editedMesh.boundsMin != Canis::Vector3(-1.5f, -1.0f, -3.5f) ||
        editedMesh.boundsMax != Canis::Vector3(3.0f, 5.5f, 5.0f))
    {
        std::cerr << "Loop cuts or face extrusion generated unexpected topology or bounds.\n";
        return 1;
    }

    Canis::BlockoutShape exportShape = {};
    exportShape = editable;
    const std::filesystem::path exportPath =
        std::filesystem::temp_directory_path() / "canis_blockout_geometry_test.obj";
    std::string exportError = {};
    if (!Canis::ExportBlockoutObj(exportShape, exportPath, &exportError))
    {
        std::cerr << "OBJ export failed: " << exportError << '\n';
        return 1;
    }
    std::ifstream exported(exportPath);
    const std::string contents((std::istreambuf_iterator<char>(exported)), std::istreambuf_iterator<char>());
    std::error_code removeError = {};
    std::filesystem::remove(exportPath, removeError);
    if (contents.find("v ") == std::string::npos || contents.find("f ") == std::string::npos)
    {
        std::cerr << "OBJ export did not contain vertices and faces.\n";
        return 1;
    }
    return 0;
}
