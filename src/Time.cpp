#include <Canis/Time.hpp>
#include <SDL.h>

namespace Canis
{
    Time::Time()
    {
    }

    Time::~Time()
    {
    }

    void Time::Init(float _targetFPS)
    {
        SetTargetFPS(_targetFPS);
        previousTime = high_resolution_clock::now();
    }

    void Time::SetTargetFPS(float _targetFPS)
    {
        maxFPS = _targetFPS;
    }

    float Time::StartFrame()
    {
        startTicks = SDL_GetTicks();

        currentTime = high_resolution_clock::now();
        nanoSecondsDeltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - previousTime).count();
        deltaTime = nanoSecondsDeltaTime / 1000000000.0;
        previousTime = currentTime;

        return deltaTime;
    }

    void Time::CalculateFPS()
    {
        static const int NUM_SAMPLES = 60;
        static double frameTimes[NUM_SAMPLES];
        static int currentFrame = 0;

        double currentTicks;
        currentTicks = SDL_GetTicks();

        frameTime = currentTicks - prevTicks;
        frameTimes[currentFrame % NUM_SAMPLES] = deltaTime * 1000;

        prevTicks = currentTicks;

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

        double frameTimeAverage = 0;
        for (int i = 0; i < count; i++)
        {
            frameTimeAverage += frameTimes[i];
        }

        frameTimeAverage /= count;

        if (frameTimeAverage > 0)
        {
            fps = 1000.0f / frameTimeAverage;
        }
        else
        {
            fps = 60.0f;
        }
    }

    float Time::EndFrame()
    {
        CalculateFPS();
        
        double frameTicks = SDL_GetTicks() - startTicks;

        if ( (1000.0f / maxFPS) > frameTicks)
        {
            SDL_Delay((1000.0f / maxFPS) - frameTicks + carryOverFrameDelay);
        }

        carryOverFrameDelay = ((1000.0f / maxFPS) - frameTicks + carryOverFrameDelay) - ((int)((1000.0f / maxFPS) - frameTicks + carryOverFrameDelay));

        //Log("frame delay: " + std::to_string(carryOverFrameDelay));

        return fps;
    }
} // end of Canis namespace