#pragma once
#include <SDL.h>
#include <chrono>
#include <thread>

#include "Debug.hpp"

namespace Canis
{
    class Time
    {
        private:
            unsigned int NUM_SAMPLES = 100;
            
            unsigned int nanoSecondsDeltaTime;
            unsigned int currentFrame = 0;

            float fps;
            float targetFPS;
            float frameTime;
            float deltaTime;

            std::chrono::steady_clock::time_point currentTime;
            std::chrono::steady_clock::time_point previousTime;

            float frameTimes[100];



            void calculateFPS();
        
        public:
            Time();
            ~Time();

            void init(float _targetFPS);
            void setTargetFPS(float _targetFPS);
            
            float startFrame();
            
            float endFrame();
    };
} // end of Canis namespace