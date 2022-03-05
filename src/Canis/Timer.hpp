#pragma once
#include <SDL.h>

#include "Debug.hpp"

namespace Canis
{
    class Timer
    {
    private:
        float _fps;
        float _maxFPS;
        float _frameTime;

        unsigned int _startTicks;

    public:
        Timer();
        ~Timer();

        void init(float targetFPS);
        void setTargetFPS(float targetFPS);
        void begin();
        void calculateFPS();

        float end();
    };
} // end of Canis namespace