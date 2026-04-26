#pragma once
#include <string>

#include <Canis/UUID.hpp>
#include <Canis/Data/GLTexture.hpp>

namespace Canis
{
    struct AudioAssetHandle
    {
        UUID uuid = UUID(0);
        std::string path = "";

        bool Empty() const
        {
            return uuid == UUID(0) && path.empty();
        }
    };

    struct SceneAssetHandle
    {
        UUID uuid = UUID(0);
        std::string path = "";

        bool Empty() const
        {
            return uuid == UUID(0) && path.empty();
        }
    };

    struct ShaderGraphAssetHandle
    {
        UUID uuid = UUID(0);
        std::string path = "";

        bool Empty() const
        {
            return uuid == UUID(0) && path.empty();
        }
    };

    struct AnimationClipAssetHandle
    {
        UUID uuid = UUID(0);
        std::string path = "";

        bool Empty() const
        {
            return uuid == UUID(0) && path.empty();
        }
    };

    struct AnimatorControllerAssetHandle
    {
        UUID uuid = UUID(0);
        std::string path = "";

        bool Empty() const
        {
            return uuid == UUID(0) && path.empty();
        }
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
