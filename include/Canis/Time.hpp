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
        high_resolution_clock::time_point currentTime;
        high_resolution_clock::time_point previousTime;

        unsigned int nanoSecondsDeltaTime;
        
        double fps;
        double maxFPS;
        double frameTime;
        double deltaTime;
        double prevTicks;
        double carryOverFrameDelay = 0.0f;

        unsigned int startTicks;

    public:
        Time();
        ~Time();

        void Init(float _targetFPS);
        void SetTargetFPS(float _targetFPS);
        float StartFrame();
        void CalculateFPS();

        float EndFrame();
    };
} // end of Canis namespace