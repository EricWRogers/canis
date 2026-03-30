#pragma once

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_timer.h>
#if !defined(__EMSCRIPTEN__)
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_loadso.h>
#endif

#include <Canis/Debug.hpp>

namespace Canis
{
    class App;

#if defined(__EMSCRIPTEN__)
    extern "C"
    {
        void *GameInit(void *_app);
        void GameUpdate(void *_app, float _deltaTime, void *_data);
        void GameShutdown(void *_app, void *_data);
    }
#endif

    struct GameCodeObject
    {
#if !defined(__EMSCRIPTEN__)
        SDL_SharedObject *sharedObjectHandle = nullptr;
        SDL_PathInfo pathInfo = {};
#endif
        const char *path = nullptr;
        void *gameData = nullptr;
        void *(*GameInitFunction)(void *) = nullptr;
        void (*GameUpdateFunction)(void *, float, void *) = nullptr;
        void (*GameShutdownFunction)(void *, void *) = nullptr;
#if !defined(__EMSCRIPTEN__)
        SDL_PathInfo _lastPathInfo = {};
        Uint64 _lastFileCheck = 0;
#endif
    };

    static GameCodeObject GameCodeObjectInit(const char *_path)
    {
        GameCodeObject gco = {};
        gco.path = _path;

#if defined(__EMSCRIPTEN__)
        gco.GameInitFunction = &GameInit;
        gco.GameUpdateFunction = &GameUpdate;
        gco.GameShutdownFunction = &GameShutdown;
        return gco;
#else
        gco.sharedObjectHandle = SDL_LoadObject(_path);
        if (gco.sharedObjectHandle == nullptr)
        {
            Debug::Log("Error loading shared object: %s", SDL_GetError());
            return gco;
        }

        if (!SDL_GetPathInfo(_path, &gco.pathInfo))
            Debug::Log("Error loading shared object path info: %s", SDL_GetError());

        gco._lastFileCheck = SDL_GetTicksNS();
        gco._lastPathInfo = gco.pathInfo;

        const char *gameInitName = "GameInit";
        gco.GameInitFunction = (void *(*)(void *))SDL_LoadFunction(gco.sharedObjectHandle, gameInitName);
        if (gco.GameInitFunction == nullptr)
        {
            Debug::Log("Error loading function '%s': %s", gameInitName, SDL_GetError());
            SDL_UnloadObject(gco.sharedObjectHandle);
            gco.sharedObjectHandle = nullptr;
            return gco;
        }

        const char *gameUpdateName = "GameUpdate";
        gco.GameUpdateFunction = (void (*)(void *, float, void *))SDL_LoadFunction(gco.sharedObjectHandle, gameUpdateName);
        if (gco.GameUpdateFunction == nullptr)
        {
            Debug::Log("Error loading function '%s': %s", gameUpdateName, SDL_GetError());
            SDL_UnloadObject(gco.sharedObjectHandle);
            gco.sharedObjectHandle = nullptr;
            return gco;
        }

        const char *gameShutdownName = "GameShutdown";
        gco.GameShutdownFunction = (void (*)(void *, void *))SDL_LoadFunction(gco.sharedObjectHandle, gameShutdownName);
        if (gco.GameShutdownFunction == nullptr)
        {
            Debug::Log("Error loading function '%s': %s", gameShutdownName, SDL_GetError());
            SDL_UnloadObject(gco.sharedObjectHandle);
            gco.sharedObjectHandle = nullptr;
            return gco;
        }

        return gco;
#endif
    }

    static void GameCodeObjectInitFunction(GameCodeObject *_gameCodeObject, Canis::App *_app)
    {
        if (_gameCodeObject != nullptr && _gameCodeObject->GameInitFunction != nullptr)
            _gameCodeObject->gameData = _gameCodeObject->GameInitFunction((void *)_app);
    }

    static void GameCodeObjectUpdateFunction(GameCodeObject *_gameCodeObject, Canis::App *_app, float _deltaTime)
    {
        if (_gameCodeObject != nullptr && _gameCodeObject->GameUpdateFunction != nullptr)
            _gameCodeObject->GameUpdateFunction((void *)_app, _deltaTime, _gameCodeObject->gameData);
    }

    static void GameCodeObjectShutdownFunction(GameCodeObject *_gameCodeObject, Canis::App *_app)
    {
        if (_gameCodeObject != nullptr && _gameCodeObject->GameShutdownFunction != nullptr)
            _gameCodeObject->GameShutdownFunction((void *)_app, _gameCodeObject->gameData);
    }

    static void GameCodeObjectWatchFile(GameCodeObject *_gameCodeObject, Canis::App *_app)
    {
#if defined(__EMSCRIPTEN__)
        (void)_gameCodeObject;
        (void)_app;
#else
        GameCodeObjectShutdownFunction(_gameCodeObject, _app);
        SDL_UnloadObject(_gameCodeObject->sharedObjectHandle);
        *_gameCodeObject = GameCodeObjectInit(_gameCodeObject->path);
        GameCodeObjectInitFunction(_gameCodeObject, _app);
#endif
    }

    static void GameCodeObjectDestroy(GameCodeObject *_gameCodeObject)
    {
#if !defined(__EMSCRIPTEN__)
        if (_gameCodeObject != nullptr && _gameCodeObject->sharedObjectHandle != nullptr)
        {
            SDL_UnloadObject(_gameCodeObject->sharedObjectHandle);
            _gameCodeObject->sharedObjectHandle = nullptr;
        }
#else
        (void)_gameCodeObject;
#endif
    }
}
