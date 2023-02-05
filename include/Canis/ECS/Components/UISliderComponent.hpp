#pragma once
#include <glm/glm.hpp>

namespace Canis
{
    struct UISliderComponent
    {
        float maxWidth = 0.0f;
        float minUVX = 0.0f;
        float maxUVX = 0.0f;
        float value = 0.0f; // 0 empty, 1 full
    };
} // end of Canis namespace