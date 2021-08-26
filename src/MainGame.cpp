#include "MainGame.hpp"

MainGame::MainGame() : _screenWidth(800), _screenHeight(600), _time(0), _gameState(GameState::PLAY), _maxFPS(10000.0f)
{
}

MainGame::~MainGame()
{
}

void MainGame::run()
{
    initSystem();

    // Init Sprites
    /*_sprites.push_back(new Sprite());
        _sprites.back()->init(-1.0f, -1.0f, 1.0f, 1.0, "bin/Textures/jimmyJump_pack/PNG/CharacterRight_Standing.png");

        _sprites.push_back(new Sprite());
        _sprites.back()->init(0.0f, -1.0f, 1.0f, 1.0, "bin/Textures/jimmyJump_pack/PNG/CharacterRight_Standing.png");
        */

    _texture = Canis::ResourceManager::getTexture("bin/Textures/jimmyJump_pack/PNG/CharacterRight_Standing.png");

    gameLoop();
}

void MainGame::initSystem()
{
    Canis::Init();

    _window.create("Game Engine", _screenWidth, _screenHeight, 0);

    _camera.init(_screenWidth, _screenHeight);

    _gameState = GameState::PLAY;

    initShaders();

    _spriteBatch.init();
}

void MainGame::initShaders()
{
    _colorProgram.compileShaders("bin/shaders/ColorShader.vert", "bin/shaders/ColorShader.frag");
    _colorProgram.addAttribute("vertexPosition");
    _colorProgram.addAttribute("vertexColor");
    _colorProgram.addAttribute("vertexUV");
    _colorProgram.linkShaders();
}

void MainGame::gameLoop()
{
    while (_gameState == GameState::PLAY)
    {
        float startTicks = SDL_GetTicks();

        processInput();
        _camera.update();
        _time += 0.01;
        drawGame();
        calculateFPS();

        static int frameCounter = 0;
        frameCounter++;
        if (frameCounter == 100)
        {
            std::cout << _fps << std::endl;
            frameCounter = 0;
        }

        float frameTicks = SDL_GetTicks() - startTicks;
        if (1000.0f / _maxFPS > frameTicks)
        {
            SDL_Delay(1000.0f / _maxFPS - frameTicks);
        }
    }
}

void MainGame::processInput()
{
    const float CAMERA_SPEED = 10.0f;
    const float SCALE_SPEED = 0.1f;
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            _gameState = GameState::EXIT;
            break;
        case SDL_MOUSEMOTION:
            // This event is only called when mouse is moved
            //std::cout << event.motion.x << " " << event.motion.y << std::endl;
            break;
        case SDL_KEYUP:
            //_keys[event.key.keysym.scancode] = false;
            //_prevKeys[event.key.keysym.scancode] = true;
            break;
        case SDL_KEYDOWN:
            //_keys[event.key.keysym.scancode] = true;
            //_prevKeys[event.key.keysym.scancode] = false;
            switch (event.key.keysym.sym)
            {
            case SDLK_ESCAPE:
                _gameState = GameState::EXIT;
                break;
            case SDLK_w:
                _camera.setPosition(_camera.getPosition() + glm::vec2(0.0f, 10.0f));
                break;
            case SDLK_s:
                _camera.setPosition(_camera.getPosition() + glm::vec2(0.0f, -10.0f));
                break;
            case SDLK_a:
                _camera.setPosition(_camera.getPosition() + glm::vec2(-10.0f, 0.0f));
                break;
            case SDLK_d:
                _camera.setPosition(_camera.getPosition() + glm::vec2(10.0f, 0.0f));
                break;
            case SDLK_q:
                _camera.setScale(_camera.getScale() + SCALE_SPEED);
                break;
            case SDLK_e:
                _camera.setScale(_camera.getScale() - SCALE_SPEED);
                break;
            }
            break;
        }
    }
}

void MainGame::drawGame()
{

    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    _colorProgram.use();
    glActiveTexture(GL_TEXTURE0);

    GLint textureLocation = _colorProgram.getUniformLocation("mySampler");
    glUniform1i(textureLocation, 0);

    GLuint timeLocation = _colorProgram.getUniformLocation("time");
    glUniform1f(timeLocation, _time);

    // Set the camera matrix
    GLuint pLocation = _colorProgram.getUniformLocation("P");

    glm::mat4 cameraMatrix = _camera.getCameraMatrix();
    glUniformMatrix4fv(pLocation, 1, GL_FALSE, &(cameraMatrix[0][0]));

    /*for (int i = 0; i < _sprites.size(); i++)
        {
            _sprites[i]->draw();
        }*/

    _spriteBatch.begin();

    glm::vec4 pos(0.0f, 0.0f, 50.0f, 50.0f);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);

    Canis::Color color;
    color.r = 255;
    color.g = 255;
    color.b = 255;
    color.a = 255;

    for (int y_mul = 0; y_mul < 12; y_mul++)
    {
        for (int x_mul = 0; x_mul < 16; x_mul++)
        {
            _spriteBatch.draw(pos + glm::vec4(50.0f * x_mul, 50.0f * y_mul, 0.0f, 0.0f), uv, _texture.id, 0.0f, color);
        }
    }

    _spriteBatch.end();

    _spriteBatch.renderBatch();

    glBindTexture(GL_TEXTURE_2D, 0);

    _colorProgram.unUse();

    _window.swapBuffer();
}

void MainGame::calculateFPS()
{
    static const int NUM_SAMPLES = 100;
    static float frameTimes[NUM_SAMPLES];
    static int currentFrame = 0;

    static float prevTicks = SDL_GetTicks();

    float currentTicks;
    currentTicks = SDL_GetTicks();

    _frameTime = currentTicks - prevTicks;
    frameTimes[currentFrame % NUM_SAMPLES] = _frameTime;

    prevTicks = currentTicks;

    int count;
    currentFrame++;
    if (currentFrame < NUM_SAMPLES)
    {
        count = currentFrame;
    }
    else
    {
        count = NUM_SAMPLES;
    }

    float frameTimeAverage = 0;
    for (int i = 0; i < count; i++)
    {
        frameTimeAverage += frameTimes[i];
    }

    frameTimeAverage /= count;

    if (frameTimeAverage > 0)
    {
        _fps = 1000.0f / frameTimeAverage;
    }
    else
    {
        _fps = 60.0f;
    }
}
