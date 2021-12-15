#include "Canis/Canis.h"
#include "Canis/Debug.h"
#include "Canis/Window.hpp"

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

    GameState _game_state = GameState.PLAY;
};