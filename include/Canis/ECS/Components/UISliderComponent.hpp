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
        float targetValue = 0.0f;
        float timeToMoveFullBar = 2.0f;
        float _time = 0.0f;
    };

    inline void SetUISliderTarget(UISliderComponent& _slider, float _target)
    {
        _slider.targetValue = _target;
        _slider._time = _slider.value*_slider.timeToMoveFullBar;
    }

    inline void SetUISliderTarget(UISliderComponent& _slider, float _target, float _timeToMoveFullBar)
    {
        _slider.timeToMoveFullBar = _timeToMoveFullBar;
        _slider.targetValue = _target;
        _slider._time = _slider.value*_slider.timeToMoveFullBar;
    }
} // end of Canis namespace