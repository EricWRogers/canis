#pragma once
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <string>

#include "Sprite.hpp"
#include "GLSLProgram.hpp"
#include "Debug.h"
#include "Window.hpp"
#include "GLTexture.h"
#include "Window.hpp"
#include "Camera2D.hpp"
#include "SpriteBatch.hpp"
#include "ResourceManager.hpp"

namespace canis
{

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
        void calculateFPS();

        Window _window;
        int _screenWidth, _screenHeight;
        GameState _gameState;

        std::vector<Sprite *> _sprite;

        GLSLProgram _colorProgram;

        SpriteBatch _spriteBatch;

        Camera2D _camera;

        GLTexture _texture;

        float _fps;
        float _maxFPS;
        float _frameTime;
        float _time;

        //const int NUM_SDL_SCANCODES = 512;
        //bool _keys[NUM_SDL_SCANCODES];
        //bool _prevKeys[NUM_SDL_SCANCODES];
    };

} // end of canis namespace