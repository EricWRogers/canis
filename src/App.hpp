#include <iostream>
#include <SDL.h>
#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Engine/Engine.hpp"
#include "Engine/Debug.hpp"
#include "Engine/Window.hpp"
#include "Engine/Shader.hpp"
#include "Engine/IOManager.hpp"
#include "Engine/Data/GLTexture.hpp"


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
    void FixedUpdate(float _delta_time);
    void InputUpdate();

    AppState appState = AppState::OFF;

    Engine::Window window;

    Engine::Shader shader;

    // move out to external class
    unsigned int vertexShader;
    unsigned int shaderProgram;
    unsigned int VBO, VAO, EBO;

    Engine::GLTexture texture1 = {};
    Engine::GLTexture texture2 = {};

    int imageWidth, imageHeight, nrChannels;

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