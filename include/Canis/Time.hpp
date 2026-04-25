#pragma once

namespace Canis::Time {
  // init engine time system
  void Init(float _targetFPS);
  // clean up engine time system
  void Quit();

  // call when you start working on your frame
  // return deltaTime
  float StartFrame();

  // reset frame timing after long pauses like sleep/resume
  void ResetFrameClock();

  // call when you finish your frame
  // return fps
  float EndFrame();

  // game code

  // set target fps
  void SetTargetFPS(float _targetFPS);

  // set time scale used by StartFrame() / DeltaTime()
  void SetTimeScale(float _timeScale);

  // get deltaTime of last frame
  float DeltaTime();

  // get unscaled deltaTime of last frame
  float UnscaledDeltaTime();

  // get current time scale
  float GetTimeScale();

  // get average fps
  float FPS();

  // number of milliseconds that have the start canis
  unsigned long long TimeSinceLaunch();
} // namespace Canis
