#include <iostream>
#include <SDL.h>
#include <math.h>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Canis/Canis.hpp"
#include "Canis/Debug.hpp"
#include "Canis/Time.hpp"
#include "Canis/Window.hpp"
#include "Canis/Shader.hpp"
#include "Canis/Camera.hpp"
#include "Canis/IOManager.hpp"
#include "Canis/InputManager.hpp"
#include "Canis/Data/GLTexture.hpp"
#include "Canis/External/entt.hpp"

#include "Canis/ECS/Systems/RenderCubeSystem.hpp"
#include "Canis/ECS/Systems/RenderTextSystem.hpp"

#include "Canis/ECS/Components/TransformComponent.hpp"
#include "Canis/ECS/Components/ColorComponent.hpp"
#include "Canis/ECS/Components/RectTransformComponent.hpp"
#include "Canis/ECS/Components/TextComponent.hpp"

#include "Game/ECS/Systems/CastleSystem.hpp"
#include "Game/ECS/Systems/MoveSlimeSystem.hpp"
#include "Game/ECS/Systems/PortalSystem.hpp"
#include "Game/ECS/Systems/SpikeSystem.hpp"
#include "Game/ECS/Systems/SpikeTowerSystem.hpp"

#include "Game/ECS/Components/CastleComponent.hpp"
#include "Game/ECS/Components/PortalComponent.hpp"
#include "Game/ECS/Components/SlimeMovementComponent.hpp"
#include "Game/ECS/Components/HealthComponent.hpp"
#include "Game/ECS/Components/SpikeComponent.hpp"
#include "Game/ECS/Components/SpikeTowerComponent.hpp"

#include "Game/Scripts/TileMap.hpp"
#include "Game/Scripts/ScoreSystem.hpp"
#include "Game/Scripts/Wallet.hpp"

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

    AppState appState = AppState::OFF;

    entt::registry entity_registry;

    Canis::Window window;

    Canis::Shader shader;

    Canis::Time time;

    Canis::InputManager inputManager;

    Canis::Camera camera = Canis::Camera(glm::vec3(4.0f, 10.0f, 6.2f),glm::vec3(0.0f, 1.0f, 0.0f),Canis::YAW,Canis::PITCH-80.0f);

    // move out to external class
    unsigned int VBO, VAO, EBO;

    Canis::GLTexture texture1 = {};
    Canis::GLTexture texture2 = {};

    float lastXMousePos;
    float lastYMousePos;
    
    bool firstMouseMove = true;

    high_resolution_clock::time_point currentTime;
    high_resolution_clock::time_point previousTime;
    double deltaTime;

    Wallet wallet;
    ScoreSystem scoreSystem;
    
};