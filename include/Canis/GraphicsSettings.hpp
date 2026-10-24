#pragma once
#include <algorithm>
#include <cmath>
#include <utility>

namespace Canis
{
// Runtime policy, independent of authored scenes and project settings.
struct GraphicsSettings
{
    float renderScale = 1;
    int maxRenderHeight = 0;
    int shadowMapSize = 2048; // Zero disables realtime directional shadows.
    bool postEffects = true;
};
GraphicsSettings& GetGraphicsSettings();
inline std::pair<int, int> GraphicsRenderSize(int width, int height, const GraphicsSettings& settings)
{
    width = std::max(width, 1); height = std::max(height, 1);
    float scale = std::isfinite(settings.renderScale) ? std::clamp(settings.renderScale, .5f, 1.f) : 1.f;
    if (settings.maxRenderHeight > 0)
        scale = std::min(scale, float(settings.maxRenderHeight) / height);
    return {std::max(1, int(std::lround(width * scale))), std::max(1, int(std::lround(height * scale)))};
}
}
