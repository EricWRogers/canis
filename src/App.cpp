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
        fflush(stdout);
        fflush(stderr);
        
        Init();

        if (GetProjectConfig().logToFile)
        {
            SetLogTarget("canis.log");
        }

        PlayerPrefs::Init(_organization, _app);
        PlayerPrefs::LoadFromFile();

        // set default audio
        GetProjectConfig().volume = PlayerPrefs::GetInt("master_volume", 128) / 128.0f;
        GetProjectConfig().musicVolume = PlayerPrefs::GetInt("music_volume", 128) / 128.0f;
        GetProjectConfig().sfxVolume = PlayerPrefs::GetInt("sfx_volume", 128) / 128.0f;
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

    void App::AddDecodeComponent(std::function<bool(YAML::Node &_n, Canis::Entity &_entity, Canis::SceneManager *_sceneManager)> _func)
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

    void App::Run(std::string _windowName)
    {
        // load project.canis
        std::ifstream file;
        file.open("assets/project.canis");

        if (!file.is_open())
        {
            FatalError("Can not open assets/project.canis");
        }

        std::string word;
        std::string firstScene;

        while(file >> word)
        {
            if (word == "splash_scene")
            {
                if (file >> word)
                {
                    YAML::Node root = YAML::LoadFile(word);
                    std::string name = root["Scene"].as<std::string>();

                    AddSplashScene(new Scene(name, word));
                }
            }
            else if (word == "scene")
            {
                if (file >> word)
                {
                    YAML::Node root = YAML::LoadFile(word);
                    std::string name = root["Scene"].as<std::string>();

                    AddScene(new Scene(name, word));
                }
            }
            else if (word == "run_scene")
            {
                if (file >> word)
                {
                    firstScene = word;
                }
            }
        }

        file.close();

        if (firstScene == "")
        {
            FatalError("run_scene not found in project.canis");
        }

        Run(_windowName, firstScene);
    }

    void App::Run( std::string _windowName, std::string _sceneName)
    {
        unsigned int windowFlags = 0;

        // windowFlags |= Canis::WindowFlags::FULLSCREEN;
        if (GetProjectConfig().borderless)
            windowFlags |= Canis::WindowFlags::BORDERLESS;
        if (GetProjectConfig().resizeable)
            windowFlags |= Canis::WindowFlags::RESIZEABLE;

        if (GetProjectConfig().fullscreen)
            window.CreateFullScreen(_windowName);
        else
            window.Create(_windowName, GetProjectConfig().width, GetProjectConfig().heigth, windowFlags);
        
        // Set VSync
        window.SetVSync(PlayerPrefs::GetBool("vsync", GetProjectConfig().vsync));
        
        //Initialize SDL_mixer
        int result = Mix_Init(MIX_INIT_OGG | MIX_INIT_MP3);
        if ( (result & MIX_INIT_OGG) == false) {
            FatalError("Failed to initialize OGG support.");
        }
        if ( (result & MIX_INIT_MP3) == false) {
            FatalError("Failed to initialize MP3 support.");
        }
        
        if( Mix_OpenAudio( 44100, MIX_DEFAULT_FORMAT, 2, 4096 ) < 0 ) {
            FatalError("Mix_OpenAudio " + std::string(Mix_GetError()));
        }
        //
        
        // seed
        if(!GetProjectConfig().overrideSeed)
            GetProjectConfig().seed = std::time(NULL);

        seed = GetProjectConfig().seed;

        srand(seed);
        Canis::Log("seed : " + std::to_string(seed));
        //

        // fps
        int fps = 0;
        if(GetProjectConfig().useFrameLimit)
            fps = GetProjectConfig().frameLimit;
        else
            fps = 100000;
        
        time.Init(PlayerPrefs::GetInt("fps_cap", fps)+0.0f);
        //

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