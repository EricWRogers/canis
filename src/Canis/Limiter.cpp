#include "Limiter.hpp"

namespace Canis
{
    Limiter::Limiter()
    {
    }

    Limiter::~Limiter()
    {
    }

    void Limiter::Init(float _target_frame_rate)
    {
        SetTargetFPS(_target_frame_rate);
    }

    void Limiter::SetTargetFPS(float _target_frame_rate)
    {
        max_fps = _target_frame_rate;
    }

    void Limiter::Begin()
    {
        start_ticks = SDL_GetTicks();
    }

    void Limiter::CalculateFPS()
    {
        static const int NUM_SAMPLES = 10000;
        static float frameTimes[NUM_SAMPLES];
        static int currentFrame = 0;

        static float prevTicks = SDL_GetTicks();

        float currentTicks;
        currentTicks = SDL_GetTicks();

        frame_time = currentTicks - prevTicks;
        frameTimes[currentFrame % NUM_SAMPLES] = frame_time;

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

        float frameTimeAverage = 0;
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

    float Limiter::End()
    {
        CalculateFPS();
        
        float frameTicks = SDL_GetTicks() - start_ticks;
        if (1000.0f / max_fps > frameTicks)
        {
            SDL_Delay(1000.0f / max_fps - frameTicks);
        }

        return fps;
    }
} // end of Canis namespace