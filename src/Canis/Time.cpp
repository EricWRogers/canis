#include "Time.hpp"

namespace Canis
{
    Time::Time() {}
    Time::~Time() {}

    void Time::init(float _targetFPS)
    {
        setTargetFPS(_targetFPS);
        previousTime = std::chrono::high_resolution_clock::now();
    }

    void Time::setTargetFPS(float _targetFPS)
    {
        targetFPS = _targetFPS;
    }

    float Time::startFrame()
    {
        currentTime = std::chrono::high_resolution_clock::now();
        nanoSecondsDeltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - previousTime).count();
        deltaTime = nanoSecondsDeltaTime / 1000000000.0f;
        previousTime = currentTime;

        return deltaTime;
    }

    float Time::endFrame()
    {
        calculateFPS();

        float frameTicks = std::chrono::duration_cast<std::chrono::nanoseconds>
            (std::chrono::high_resolution_clock::now() - previousTime).count();
        
        if (1000000000.0f / targetFPS > frameTicks)
        {
            float sleepTime = 1000000000.0f / targetFPS - frameTicks;
            std::chrono::steady_clock::time_point beforeSleep;
            std::chrono::steady_clock::time_point afterSleep;

            beforeSleep = std::chrono::high_resolution_clock::now();

            std::this_thread::sleep_for(
                std::chrono::nanoseconds(static_cast<unsigned int>(sleepTime))
            );

            afterSleep = std::chrono::high_resolution_clock::now();

            unsigned int nanoS = std::chrono::duration_cast<std::chrono::nanoseconds>(afterSleep - beforeSleep).count();

            //Log("sleep : " + std::to_string(sleepTime) + " chrono nano : " + std::to_string(nanoS));
        }

        return fps;
    }

    void Time::calculateFPS()
    {
        frameTime = std::chrono::duration_cast<std::chrono::nanoseconds>
            (std::chrono::high_resolution_clock::now() - previousTime).count();
        
        frameTimes[currentFrame % NUM_SAMPLES] = frameTime;

        int count;
        currentFrame++;
        if (currentFrame < NUM_SAMPLES)
        {
            count = currentFrame;
        }
        else
        {
            count = NUM_SAMPLES;
        }

        float frameTimeAverage = 0;
        for (int i = 0; i < count; i++)
        {
            frameTimeAverage += frameTimes[i];
        }

        frameTimeAverage /= count;

        if (frameTimeAverage > 0)
        {
            fps = 1000000000.0f / frameTimeAverage;
        }
        else
        {
            fps = 60.0f;
        }
    }
} // end of Canis namespace