#pragma once

#include <string>

namespace Canis
{
    enum class TextureFilter { Legacy, Point, Bilinear, Trilinear };
    enum class TextureWrap { Repeat, Clamp, Mirror };

    struct TextureImportSettings
    {
        // Assets without import metadata retain the engine's original sampler.
        TextureFilter filter = TextureFilter::Legacy;
        TextureWrap wrapU = TextureWrap::Repeat;
        TextureWrap wrapV = TextureWrap::Repeat;
        bool mipmaps = true;
        bool alphaBleed = false;
        float mipBias = 0.0f;

        bool operator==(const TextureImportSettings &) const = default;
    };

    TextureImportSettings LoadTextureImportSettings(const std::string &assetPath);
    // Merges only TextureImport fields into <assetPath>.meta. Source pixels and
    // unrelated metadata (including UUID and future import fields) are retained.
    bool SaveTextureImportSettings(const std::string &assetPath,
                                   const TextureImportSettings &settings,
                                   std::string *error = nullptr);

    struct DecodedImage;
    // Extends edge RGB into fully transparent pixels before filtering/mipmap
    // generation. Alpha and every nontransparent source pixel remain unchanged.
    void ApplyTextureAlphaBleed(DecodedImage &image, int maxDistance = 8);
}
