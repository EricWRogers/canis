#include <iostream>
#include <SDL.h>
#include <math.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Canis/Canis.hpp"
#include "Canis/Debug.hpp"
#include "Canis/Time.hpp"
#include "Canis/Window.hpp"
#include "Canis/Shader.hpp"
#include "Canis/Camera.hpp"
#include "Canis/IOManager.hpp"
#include "Canis/InputManager.hpp"
#include "Canis/Data/GLTexture.hpp"
#include "Canis/ECS/ECS.hpp"
#include "Canis/ECS/ECSSystem.hpp"
#include "Canis/ECS/ECSComponent.hpp"

#ifdef __linux__
using namespace std::chrono::_V2;
#elif _WIN32
using namespace std::chrono;
#else

#endif


enum AppState
{
    ON,
    OFF
};

class App
{
public:
    App();
    ~App();

    void Run();

private:
    void Load();
    void Loop();
    void Update();
    void Draw();
    void LateUpdate();
    void FixedUpdate(float dt);
    void InputUpdate();

    AppState appState = AppState::OFF;

    Canis::Window window;

    Canis::Shader shader;

    Canis::Time time;

    Canis::InputManager inputManager;

    Canis::Camera camera = Canis::Camera(glm::vec3(0.0f, 0.0f, 3.0f));

    ECS ecs;

    EntityHandle entity;

    // move out to external class
    unsigned int vertexShader;
    unsigned int shaderProgram;
    unsigned int VBO, VAO, EBO;

    Canis::GLTexture texture1 = {};
    Canis::GLTexture texture2 = {};

    float lastXMousePos;
    float lastYMousePos;
    
    bool firstMouseMove = true;

    high_resolution_clock::time_point currentTime;
    high_resolution_clock::time_point previousTime;
    double deltaTime;

    

    const char *vertexShaderSource = "#version 330 core\n"
                                     "layout (location = 0) in vec3 aPos;\n"
                                     "layout (location = 1) in vec3 aColor;\n"
                                     "out vec3 ourColor;\n"
                                     "void main()\n"
                                     "{\n"
                                     "   gl_Position = vec4(aPos, 1.0);\n"
                                     "   ourColor = aColor;\n"
                                     "}\0";

    const char *fragmentShaderSource = "#version 330 core\n"
                                       "out vec4 FragColor;\n"
                                       "in vec3 ourColor;\n"
                                       "void main()\n"
                                       "{\n"
                                       "   FragColor = vec4(ourColor, 1.0);\n"
                                       "}\n\0";
    
    
};