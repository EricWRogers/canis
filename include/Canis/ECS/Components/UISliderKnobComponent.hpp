#pragma once

namespace Canis
{
    struct UISliderKnobComponent
    {
        std::string eventName = "";
        Entity slider;
        bool grabbed = false;
        float value = 1.0f;

        static void RegisterProperties()
		{
			REGISTER_PROPERTY(Canis::UISliderKnobComponent, eventName, std::string);
            REGISTER_PROPERTY(Canis::UISliderKnobComponent, slider, Entity);
            REGISTER_PROPERTY(Canis::UISliderKnobComponent, grabbed, bool);
            REGISTER_PROPERTY(Canis::UISliderKnobComponent, value, float);
		}
    };
} // end of Canis namespace