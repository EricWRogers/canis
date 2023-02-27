#pragma once

#include <chrono>

#include "Debug.hpp"

#ifdef __linux__
using namespace std::chrono::_V2;
#elif _WIN32
using namespace std::chrono;
#else
using namespace std::chrono;
#endif

namespace Canis
{
    class Time
    {
    private:
        high_resolution_clock::time_point _currentTime;
        high_resolution_clock::time_point _previousTime;

        unsigned int _nanoSecondsDeltaTime;
        
        float _fps;
        float _maxFPS;
        float _frameTime;
        float _deltaTime;

        unsigned int _startTicks;

    public:
        Time();
        ~Time();

        void init(float targetFPS);
        void setTargetFPS(float targetFPS);
        float startFrame();
        void calculateFPS();

        float endFrame();
    };
} // end of Canis namespace