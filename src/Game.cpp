#include "Game.hpp"

Game::Game() {}
Game::~Game() {}

void Game::Run() {
    Canis::Init();

    frame_counter = 0;

    Canis::Window window;

    Load();

    window.Spawn("Canis Engine Demo", 1280, 720, 0);

    target_fixed_update_ticks = (1.0f / 16.0f) * 1000;
    last_fixed_update = SDL_GetTicks();  

    frame_limiter.Init(8000.0f);

    Loop();
}

void Game::Load() {}

void Game::Loop() {
    while(_game_state == GameState::PLAY) {

        frame_limiter.Begin();

        current_ticks_since_last_fixed_update = SDL_GetTicks() - last_fixed_update;

        if (current_ticks_since_last_fixed_update >= target_fixed_update_ticks) {
            FixedUpdate(current_ticks_since_last_fixed_update);
            last_fixed_update = SDL_GetTicks();
        }

        InputUpdate();
        Update();
        Draw();
        LateUpdate();

        fps = frame_limiter.End();

        frame_counter++;
        if (frame_counter == 1000)
        {
            Canis::Log("FPS: " + std::to_string(fps));
            frame_counter = 0;
        }
    }
}

void Game::Update() {}

void Game::Draw() {
    Canis::Log("Draw");
}

void Game::LateUpdate() {}

void Game::FixedUpdate(float _delta_time) {
    Canis::Log("FixedUpdate: " + std::to_string(_delta_time));
}

void Game::InputUpdate() {
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_QUIT:
            _game_state = GameState::EXIT;
            break;
        case SDL_MOUSEMOTION:
            break;
        case SDL_KEYUP:
            input_manager.releasedKey(event.key.keysym.sym);
            break;
        case SDL_KEYDOWN:
            input_manager.pressKey(event.key.keysym.sym);
            break;
        }
    }
}