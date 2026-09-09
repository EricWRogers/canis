#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <Canis/Asset.hpp>
#include <Canis/Components.hpp>

namespace Canis
{
    struct BlockoutMeshData
    {
        std::vector<ModelAsset::RenderVertex3D> vertices = {};
        std::vector<u32> indices = {};
        std::vector<BlockoutFace> faces = {};
        Vector3 boundsMin = Vector3(0.0f);
        Vector3 boundsMax = Vector3(0.0f);
    };

    bool BuildBlockoutMesh(
        const BlockoutShape &_shape,
        BlockoutMeshData &_mesh,
        std::string *_error = nullptr);
    bool ExportBlockoutObj(
        const BlockoutShape &_shape,
        const std::filesystem::path &_path,
        std::string *_error = nullptr);
    bool RebuildBlockoutEntity(Entity &_entity, bool _collision = true);
    bool ConvertBlockoutToEditable(BlockoutShape &_shape, std::string &_error);
    bool ValidateBlockoutTopology(const EditableBlockoutMesh &_mesh, std::string &_error);
    bool ExtrudeBlockoutFaces(EditableBlockoutMesh &_mesh, const std::vector<u32> &_faces,
        float _distance, std::string &_error);
    bool CutBlockoutLoop(EditableBlockoutMesh &_mesh, u32 _a, u32 _b, int _cuts, std::string &_error);
    bool DissolveBlockoutEdge(EditableBlockoutMesh &_mesh, u32 _a, u32 _b, std::string &_error);
}
