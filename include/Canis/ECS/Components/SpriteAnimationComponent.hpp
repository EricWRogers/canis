#pragma once

namespace Canis
{
    struct SpriteAnimationComponent
    {
        unsigned short int animationId = 0u;
        float countDown = 0.0f;
        unsigned short int index = 0u;
        bool flipX = false;
        bool flipY = false;
        bool redraw = true;
        float speed = 1.0f;

        void Play(std::string _path)
        {
            speed = 1.0f;
            animationId = Canis::AssetManager::LoadSpriteAnimation(_path);
            index = 0;
            redraw = true;
        }

        void Pause()
        {
            speed = 0.0f;
        }
    };
} // end of Canis namespace