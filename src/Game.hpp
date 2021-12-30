#include <string>

#include "Canis/Canis.h"
#include "Canis/Debug.h"
#include "Canis/Shader.hpp"
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

    Canis::Window window;

    Canis::Shader shader;

    int frame_counter;
    
    float fps;

    float last_fixed_update;
    float current_ticks_since_last_fixed_update;
    float target_fixed_update_ticks;

    // move out to external class
    unsigned int vertexShader;
    unsigned int shaderProgram;
    unsigned int VBO, VAO, EBO;
    unsigned int texture1, texture2;

    const char *vertexShaderSource = "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
    "}\0";

    const char *fragmentShaderSource = "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\0";
};