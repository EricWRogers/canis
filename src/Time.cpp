#include <Canis/Time.hpp>

namespace Canis
{
    Time::Time()
    {
    }

    Time::~Time()
    {
    }

    void Time::init(float maxFPS)
    {
        setTargetFPS(maxFPS);
        _previousTime = high_resolution_clock::now();
    }

    void Time::setTargetFPS(float maxFPS)
    {
        _maxFPS = maxFPS;
    }

    float Time::startFrame()
    {
        _startTicks = SDL_GetTicks();

        _currentTime = high_resolution_clock::now();
        _nanoSecondsDeltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(_currentTime - _previousTime).count();
        _deltaTime = _nanoSecondsDeltaTime / 1000000000.0f;
        _previousTime = _currentTime;

        return _deltaTime;
    }

    void Time::calculateFPS()
    {
        static const int NUM_SAMPLES = 5000;
        static float frameTimes[NUM_SAMPLES];
        static int currentFrame = 0;

        static float prevTicks = SDL_GetTicks();

        float currentTicks;
        currentTicks = SDL_GetTicks();

        _frameTime = currentTicks - prevTicks;
        frameTimes[currentFrame % NUM_SAMPLES] = _frameTime;

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
            _fps = 1000.0f / frameTimeAverage;
        }
        else
        {
            _fps = 60.0f;
        }
    }

    float Time::endFrame()
    {
        calculateFPS();
        
        float frameTicks = SDL_GetTicks() - _startTicks;
        if (1000.0f / _maxFPS > frameTicks)
        {
            SDL_Delay(1000.0f / _maxFPS - frameTicks);
        }

        return _fps;
    }
} // end of Canis namespace