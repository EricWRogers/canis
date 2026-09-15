#pragma once
#include <Canis/VR/VRMath.hpp>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace Canis::VR
{
    enum class FoveationMode { Off, Fixed, EyeTracked };
    // Conservative full-rate center, 2x2 middle, 4x4 peripheral shading. Values
    // are palette indices, never pixel colors. Hardware uses complete tile coverage.
    inline std::vector<uint8_t> ShadingRateMap(int width, int height, int tileWidth, int tileHeight, Vector2 center)
    {
        if (width <= 0 || height <= 0 || tileWidth <= 0 || tileHeight <= 0 ||
            width > 16384 || height > 16384 || tileWidth > 16384 || tileHeight > 16384 || !std::isfinite(center.x) || !std::isfinite(center.y))
            throw std::invalid_argument("Invalid shading-rate map dimensions/center");
        const int columns = (width + tileWidth - 1) / tileWidth;
        const int rows = (height + tileHeight - 1) / tileHeight;
        std::vector<uint8_t> map(static_cast<size_t>(columns) * rows);
        const float margin = glm::length(Vector2(float(tileWidth)/width, float(tileHeight)/height));
        for (int y = 0; y < rows; ++y)
            for (int x = 0; x < columns; ++x)
            {
                Vector2 uv((x + 0.5f)*tileWidth/width, (y + 0.5f)*tileHeight/height);
                float distance = glm::length((uv - center)*2.0f);
                map[y*columns+x] = distance <= 0.45f+margin ? 0 : (distance <= 0.8f+margin ? 1 : 2);
            }
        return map;
    }
    inline bool GazeCenter(const Pose& gaze, const Pose& eye, const Matrix4& projection, Vector2& center)
    {
        if (!gaze.valid || !eye.valid) return false;
        Vector3 direction = glm::inverse(eye.orientation) * (gaze.orientation * Vector3(0,0,-1));
        if (direction.z >= -0.001f) return false;
        const Vector4 clip = projection * Vector4(direction, 0);
        center = Vector2(clip)/clip.w * 0.5f + 0.5f;
        return std::isfinite(center.x) && std::isfinite(center.y) &&
            center.x >= 0 && center.x <= 1 && center.y >= 0 && center.y <= 1;
    }
}
