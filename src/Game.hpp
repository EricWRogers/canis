#include <string>

#include "Canis/Canis.h"
#include "Canis/Debug.h"
#include "Canis/Window.hpp"
#include "Canis/Limiter.hpp"
#include "Canis/InputManager.hpp"

enum class GameState
{
    PLAY,
    EXIT
};

class Game
{
public:
    Game();
    ~Game();

    void Run();

private:
    void Load();
    void Loop();
    void Update();
    void Draw();
    void LateUpdate();
    void FixedUpdate(float _delta_time);
    void InputUpdate();

    GameState _game_state = GameState::PLAY;

    Canis::InputManager input_manager;

    Canis::Limiter frame_limiter;

    int frame_counter;
    
    float fps;

    float last_fixed_update;
    float current_ticks_since_last_fixed_update;
    float target_fixed_update_ticks;
};