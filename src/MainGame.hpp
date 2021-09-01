#pragma once
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <string>

#include "Canis/Canis.h"
#include "Canis/Sprite.hpp"
#include "Canis/GLSLProgram.hpp"
#include "Canis/Debug.h"
#include "Canis/Window.hpp"
#include "Canis/GLTexture.h"
#include "Canis/Window.hpp"
#include "Canis/Camera2D.hpp"
#include "Canis/SpriteBatch.hpp"
#include "Canis/ResourceManager.hpp"
#include "Canis/InputManager.hpp"
#include "Canis/Timing.hpp"

enum class GameState
{
    PLAY,
    EXIT
};

class MainGame
{
public:
    MainGame();
    ~MainGame();

    void run();

private:
    void initSystem();
    void initShaders();
    void gameLoop();
    void processInput();
    void drawGame();

    Canis::Window _window;
    int _screenWidth, _screenHeight;
    GameState _gameState;

    std::vector<Canis::Sprite *> _sprites;

    Canis::GLSLProgram _colorProgram;

    Canis::SpriteBatch _spriteBatch;

    Canis::Camera2D _camera;

    Canis::GLTexture _texture;

    Canis::InputManager _inputManager;

    Canis::FpsLimiter _fpsLimiter;

    float _fps;
    float _maxFPS;
    float _time;
};
