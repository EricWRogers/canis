#pragma once

namespace Canis
{
    struct SpriteAnimationComponent
    {
        unsigned short int animationId;
        float countDown = 0.0f;
        unsigned short int index;
        bool flipX = false;
        bool flipY = false;
        bool redraw = true;
        float speed = 1.0f;
    };
} // end of Canis namespace