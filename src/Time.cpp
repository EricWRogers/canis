#include <Canis/Debug.hpp>
#include <Canis/Time.hpp>
#include <SDL3/SDL_timer.h>

namespace Canis::Time
{
    struct TimeData
    {
        float deltaTime = 0.0f;
        float fps = 0.0f;
        float targetFPS = 120.0f;
        Uint64 startFrameTicks = 0;
        Uint64 nanoSecondsDeltaTime = 0;
        Uint64 frameTime = 0;
        Uint64 prevTicks = 0;
        double carryOverFrameDelay = 0.0f;
    };

    TimeData *timeData = NULL;

    void Init(float _targetFPS)
    {
        if (timeData == NULL)
        {
            timeData = new TimeData();
            timeData->targetFPS = _targetFPS;
            timeData->startFrameTicks = SDL_GetTicksNS();
        }
        else
        {
            Debug::FatalError("You called Canis::Time::Init more the once!");
        }
    }

    void Quit()
    {
        if (timeData)
        {
            delete timeData;
            timeData = NULL;
        }
        else
        {
            Debug::FatalError("Canis::Time::Quit called with Time not initialized");
        }
    }

    float StartFrame()
    {
        if (timeData)
        {
            Uint64 currectTicks = SDL_GetTicksNS();
            timeData->nanoSecondsDeltaTime = currectTicks - timeData->startFrameTicks;
            timeData->deltaTime = timeData->nanoSecondsDeltaTime / 1000000000.0;
            timeData->startFrameTicks = SDL_GetTicksNS();
            return timeData->deltaTime;
        }
        else
        {
            // error
            return 0.0f;
        }
    }

    void CalculateFPS()
    {
        if (timeData)
        {
            static const int NUM_SAMPLES = 60;
            static double frameTimes[NUM_SAMPLES];
            static int currentFrame = 0;

            Uint64 currentTicks;
            currentTicks = SDL_GetTicksNS();

            timeData->frameTime = currentTicks - timeData->startFrameTicks;
            frameTimes[currentFrame % NUM_SAMPLES] = timeData->deltaTime * 1000;

            timeData->prevTicks = currentTicks;

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
                timeData->fps = 1000.0f / frameTimeAverage;
            }
            else
            {
                timeData->fps = 60.0f;
            }
        }
    }

    float EndFrame()
    {
        if (timeData)
        {
            CalculateFPS();

            double frameTicks = SDL_GetTicksNS() - timeData->startFrameTicks;

            if ((1000000000.0f / timeData->targetFPS) > frameTicks)
            {
                SDL_DelayNS((1000000000.0 / timeData->targetFPS) - frameTicks +
                            timeData->carryOverFrameDelay);
            }

            timeData->carryOverFrameDelay =
                ((1000000000.0 / timeData->targetFPS) - frameTicks +
                 timeData->carryOverFrameDelay) -
                ((int)((1000000000.0 / timeData->targetFPS) - frameTicks +
                       timeData->carryOverFrameDelay));

            return timeData->fps;
        }
        else
        {
            // error
            return 0.0f;
        }
    }

    void SetTargetFPS(float _targetFPS)
    {
        if (timeData)
        {
            timeData->targetFPS = _targetFPS;
        }
        else
        {
            // error
        }
    }

    float DeltaTime()
    {
        if (timeData)
        {
            return timeData->deltaTime;
        }
        else
        {
            // error
            return 0.0f;
        }
    }

    float FPS()
    {
        if (timeData)
        {
            return timeData->fps;
        }
        else
        {
            // error
            return 0.0f;
        }
    }

    unsigned long long TimeSinceLaunch()
    {
        return SDL_GetTicks();
    }
} // namespace Canis::Time
