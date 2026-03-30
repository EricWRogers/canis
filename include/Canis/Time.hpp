#pragma once

namespace Canis::Time {
  // init engine time system
  void Init(float _targetFPS);
  // clean up engine time system
  void Quit();

  // call when you start working on your frame
  // return deltaTime
  float StartFrame();

  // call when you finish your frame
  // return fps
  float EndFrame();

  // game code

  // set target fps
  void SetTargetFPS(float _targetFPS);

  // get deltaTime of last frame
  float DeltaTime();

  // get average fps
  float FPS();

  // number of milliseconds that have the start canis
  unsigned long long TimeSinceLaunch();
} // namespace Canis
