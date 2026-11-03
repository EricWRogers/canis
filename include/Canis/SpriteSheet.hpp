#pragma once

#include <Canis/Math.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Canis
{
    // Pixel rectangles use the source image's top-left origin. Pivots use the
    // RectLayout convention: (0,0) is bottom-left, (1,1) is top-right.
    struct SpriteRegion
    {
        std::string name;
        int x = 0, y = 0, width = 1, height = 1;
        Vector2 pivot = Vector2(0.5f);
    };

    struct SpriteSheet
    {
        int imageWidth = 0, imageHeight = 0;
        std::vector<SpriteRegion> regions;
    };

    struct SpriteGrid
    {
        int cellWidth = 32, cellHeight = 32;
        // Positive columns/rows request a count-based grid; otherwise use cell
        // size. A slice only includes complete cells inside the source image.
        int columns = 0, rows = 0;
        int offsetX = 0, offsetY = 0, paddingX = 0, paddingY = 0;
    };

    void NormalizeSpriteSheet(SpriteSheet &sheet);
    // SpriteRenderer's vertex shader flips V. This converts top-left source
    // pixels to its bottom-left UV rectangle without an extra visual flip.
    Vector4 SpriteRegionUV(const SpriteRegion &region, int imageWidth, int imageHeight);
    std::vector<SpriteRegion> SliceSpriteGrid(int imageWidth, int imageHeight,
                                            const SpriteGrid &grid, std::string *error = nullptr);
    bool TrimSpriteRegion(SpriteRegion &region, const std::vector<unsigned char> &rgba,
                          int imageWidth, int imageHeight, std::uint8_t alphaThreshold = 0);

    // Source images are never written. The .meta SpriteSheet map is merged
    // atomically, preserving UUID, TextureImport, and unrelated metadata.
    bool LoadSpriteSheet(const std::string &texturePath, int imageWidth, int imageHeight,
                         SpriteSheet &sheet, std::string *error = nullptr);
    bool SaveSpriteSheet(const std::string &texturePath, const SpriteSheet &sheet,
                         std::string *error = nullptr);
}
