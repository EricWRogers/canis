#pragma once

namespace Canis
{
    struct RectLayout;
    enum class RectAnchorAxis { Custom, Min, Center, Max, Stretch };

    // Without modifiers, compensate offsets so the rendered rectangle stays put.
    void ApplyRectAnchorPreset(RectLayout& rect, RectAnchorAxis horizontal, RectAnchorAxis vertical,
                               bool setPivot = false, bool setPosition = false);
    bool DrawRectLayoutAnchorPresets(RectLayout& rect);
}
