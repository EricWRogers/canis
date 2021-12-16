#pragma once
#include <SDL2/SDL.h>

namespace Canis
{
    class Limiter
    {
        public:
            Limiter();
            ~Limiter();

            void Init(float _target_frame_rate);
            void SetTargetFPS(float _target_frame_rate);
            void Begin();
            void CalculateFPS();

            float End();
        private:
            float fps;
            float max_fps;
            float frame_time;

            unsigned int start_ticks;
    };
} // end of Canis namespace