#include <Canis/App.hpp>

#include <Canis/Canis.hpp>
#include <Canis/Debug.hpp>
#include <Canis/PlayerPrefs.hpp>

#include <SDL_mixer.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <iostream>

namespace Canis
{
    App::App(const std::string &_organization, const std::string &_app)
    {
        Init();
        PlayerPrefs::Init(_organization, _app);
        PlayerPrefs::LoadFromFile();

        // set default audio
        GetProjectConfig().volume = PlayerPrefs::GetInt("master_volume", 128) / 128.0f;
    }

    App::~App()
    {
        PlayerPrefs::SaveToFile();
    }

    void App::AddDecodeSystem(std::function<bool(const std::string &_name, Canis::Scene *scene)> _func)
    {
        sceneManager.decodeSystem.push_back(_func);
    }

    void App::AddDecodeRenderSystem(std::function<bool(const std::string &_name, Canis::Scene *scene)> _func)
    {
        sceneManager.decodeRenderSystem.push_back(_func);
    }

    void App::AddDecodeComponent(std::function<void(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)> _func)
    {
        sceneManager.decodeEntity.push_back(_func);
    }

    void App::AddDecodeScriptableEntity(std::function<bool(const std::string &_name, Canis::Entity &_entity)> _func)
    {
        sceneManager.decodeScriptableEntity.push_back(_func);
    }

    void App::AddEncodeComponent(std::function<void(YAML::Emitter &_out, Canis::Entity &_entity)> _func)
    {
        sceneManager.encodeEntity.push_back(_func);
    }

    void App::AddEncodeScriptableEntity(std::function<bool(YAML::Emitter &_out, Canis::ScriptableEntity* _scriptableEntity)> _func)
    {
        sceneManager.encodeScriptableEntity.push_back(_func);
    }

    void App::AddScene(Scene *_scene)
    {
        sceneManager.Add(_scene);
    }

    void App::AddSplashScene(Scene *_scene)
    {
        sceneManager.AddSplashScene(_scene);
    }

    void App::Run( std::string _windowName, std::string _sceneName)
    {
        unsigned int windowFlags = 0;

        // windowFlags |= Canis::WindowFlags::FULLSCREEN;
        // windowFlags |= Canis::WindowFlags::BORDERLESS;

        if (GetProjectConfig().fullscreen)
            window.CreateFullScreen(_windowName);
        else
            window.Create(_windowName, GetProjectConfig().width, GetProjectConfig().heigth, windowFlags);
        
        //Initialize SDL_mixer
        /*if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) < 0 )
        {
            printf( "SDL_mixer could not initialize! SDL_mixer Error: %s\n", Mix_GetError() );
        }*/
        
        if(!GetProjectConfig().overrideSeed)
            GetProjectConfig().seed = std::time(NULL);

        seed = GetProjectConfig().seed;

        srand(seed);
        Canis::Log("seed : " + std::to_string(seed));

        if(GetProjectConfig().useFrameLimit)
            time.Init(GetProjectConfig().frameLimit+0.0f);
        else
            time.Init(100000);

        camera.override_camera = false;

        sceneManager.window = &window;
        sceneManager.inputManager = &inputManager;
        sceneManager.time = &time;
        sceneManager.camera = &camera;
        sceneManager.seed = seed;


        if (sceneManager.IsSplashScene(_sceneName))
        {
            sceneManager.PreLoad(_sceneName);
        }
        else
        {
            sceneManager.PreLoadAll();
        }

        // load first scene
        sceneManager.ForceLoad(_sceneName);

        appState = AppState::ON;

        #ifdef __EMSCRIPTEN__
        Loop();
        #else
        while(appState == AppState::ON)
        {
            Loop();
        }
        #endif
    }
    
    void App::Loop()
    {
        deltaTime = time.StartFrame();

        sceneManager.SetDeltaTime(deltaTime);

        sceneManager.Update();

        sceneManager.Draw();

        // Get SDL to swap our buffer
        window.SwapBuffer();

        sceneManager.LateUpdate();
        
        if (!inputManager.Update(window.GetScreenWidth(), window.GetScreenHeight(), (void*)&window))
            appState = AppState::OFF;
        
        sceneManager.InputUpdate();

        window.fps = time.EndFrame(); 

        if (sceneManager.running == false)
            appState = AppState::OFF;
    }
}