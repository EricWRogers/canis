#pragma once

namespace Canis
{
    struct UISliderKnobComponent
    {
        void (*OnChanged)(float) = nullptr;
        Entity slider;
        bool grabbed = false;
        float value = 1.0f;
    };
} // end of Canis namespace