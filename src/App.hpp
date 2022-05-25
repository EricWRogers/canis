#include <iostream>
#include <SDL.h>
#include <math.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "Canis/Canis.hpp"
#include "Canis/Debug.hpp"
#include "Canis/Math.hpp"
#include "Canis/Time.hpp"
#include "Canis/Window.hpp"
#include "Canis/Shader.hpp"
#include "Canis/Camera.hpp"
#include "Canis/IOManager.hpp"
#include "Canis/InputManager.hpp"
#include "Canis/Data/GLTexture.hpp"
#include "Canis/Data/Vertex.hpp"
#include "Canis/External/entt.hpp"
#include "Canis/GameHelper/AStar.hpp"

#include "Canis/ECS/Systems/RenderCubeSystem.hpp"
#include "Canis/ECS/Systems/RenderTextSystem.hpp"

#include "Canis/ECS/Components/TransformComponent.hpp"
#include "Canis/ECS/Components/ColorComponent.hpp"
#include "Canis/ECS/Components/RectTransformComponent.hpp"
#include "Canis/ECS/Components/TextComponent.hpp"
#include "Canis/ECS/Components/CubeMeshComponent.hpp"

#include "Game/ECS/Systems/CastleSystem.hpp"
#include "Game/ECS/Systems/MoveSlimeSystem.hpp"
#include "Game/ECS/Systems/PortalSystem.hpp"
#include "Game/ECS/Systems/SpikeSystem.hpp"
#include "Game/ECS/Systems/SpikeTowerSystem.hpp"
#include "Game/ECS/Systems/GemMineTowerSystem.hpp"
#include "Game/ECS/Systems/FireTowerSystem.hpp"
#include "Game/ECS/Systems/FireBallSystem.hpp"
#include "Game/ECS/Systems/IceTowerSystem.hpp"
#include "Game/ECS/Systems/SlimeFreezeSystem.hpp"
#include "Game/ECS/Systems/PlacementToolSystem.hpp"
#include "Game/ECS/Systems/RenderTowerSystem.hpp"

#include "Game/ECS/Components/CastleComponent.hpp"
#include "Game/ECS/Components/PortalComponent.hpp"
#include "Game/ECS/Components/SlimeMovementComponent.hpp"
#include "Game/ECS/Components/HealthComponent.hpp"
#include "Game/ECS/Components/SpikeComponent.hpp"
#include "Game/ECS/Components/SpikeTowerComponent.hpp"
#include "Game/ECS/Components/GemMineTowerComponent.hpp"
#include "Game/ECS/Components/FireTowerComponent.hpp"
#include "Game/ECS/Components/FireBallComponent.hpp"
#include "Game/ECS/Components/IceTowerComponent.hpp"
#include "Game/ECS/Components/PlacementToolComponent.hpp"
#include "Game/ECS/Components/TowerMeshComponent.hpp"

#include "Game/Scripts/TileMap.hpp"
#include "Game/Scripts/ScoreSystem.hpp"
#include "Game/Scripts/Wallet.hpp"
#include "Game/Scripts/HUDManager.hpp"

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

    void LoadECS();

    AppState appState = AppState::OFF;

    entt::registry entity_registry;

    Canis::Window window;

    Canis::Shader shader;

    Canis::Shader towerShader;

    Canis::Time time;

    Canis::InputManager inputManager;

    Canis::Camera camera = Canis::Camera(glm::vec3(0.907444f, 23.242630f, 18.476089f),glm::vec3(0.0f, 1.0f, 0.0f),Canis::YAW-17.499863f,Canis::PITCH-68.600151f);

    // move out to external class
    unsigned int VBO, VAO, EBO, 
        fireTowerVAO, fireTowerVBO,
        spikeTowerVAO, spikeTowerVBO,
        goldTowerVAO, goldTowerVBO,
        iceTowerVAO, iceTowerVBO,
        vertexbuffer, normalbuffer,
        uvbuffer;

    Canis::GLTexture texture1 = {};
    Canis::GLTexture texture2 = {};

    Canis::GLTexture colorPaletteTexture = {};

    float lastXMousePos;
    float lastYMousePos;
    
    bool firstMouseMove = true;
    bool mouseLock = false;

    high_resolution_clock::time_point currentTime;
    high_resolution_clock::time_point previousTime;
    double deltaTime;

    HUDManager hudManager;
    Wallet wallet;
    ScoreSystem scoreSystem;

    Canis::AStar aStar;
    
};