#pragma once
#include <string>
#include <Canis/Data/GLTexture.hpp>

namespace Canis
{
    struct SceneAssetHandle
    {
        std::string path = "";
    };

    struct TextureHandle
    {
        int id = -1;
        GLTexture texture;
    };

    struct MeshHandle
    {
        int id = -1;
    };
}
