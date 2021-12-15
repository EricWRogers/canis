#include "Game.hpp"

Game::Game() {}
Game::~Game() {}

void Game::Run() {
    Canis::Init();

    Canis::Window window;

    Load();

    window.Spawn("Canis Engine Demo", 1280, 720, 0);

    Loop();
}

void Game::Load() {

}

void Game::Loop() {
    while(_game_state == GameState.PLAY) {
        
    }
}

void Game::Update() {}

void Game::Draw() {}

void Game::LateUpdate() {}

void Game::FixedUpdate(float _delta_time) {}

void Game::InputUpdate() {}