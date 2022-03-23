#include "App.hpp"

// set up vertex data (and buffer(s)) and configure vertex attributes
// ------------------------------------------------------------------
float vertices[] = {
    // positions
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, 0.5f, -0.5f,
    0.5f, 0.5f, -0.5f,
    -0.5f, 0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f, -0.5f, 0.5f,
    0.5f, -0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    -0.5f, 0.5f, 0.5f,
    -0.5f, -0.5f, 0.5f,

    -0.5f, 0.5f, 0.5f,
    -0.5f, 0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, 0.5f,
    -0.5f, 0.5f, 0.5f,

    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,

    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, 0.5f,
    0.5f, -0.5f, 0.5f,
    -0.5f, -0.5f, 0.5f,
    -0.5f, -0.5f, -0.5f,

    -0.5f, 0.5f, -0.5f,
    0.5f, 0.5f, -0.5f,
    0.5f, 0.5f, 0.5f,
    0.5f, 0.5f, 0.5f,
    -0.5f, 0.5f, 0.5f,
    -0.5f, 0.5f, -0.5f};

// world space positions of our cubes
glm::vec3 cubePositions[] = {
    glm::vec3(0.0f, 0.0f, 0.0f),
    glm::vec3(2.0f, 5.0f, -15.0f),
    glm::vec3(-1.5f, -2.2f, -2.5f),
    glm::vec3(-3.8f, -2.0f, -12.3f),
    glm::vec3(2.4f, -0.4f, -3.5f),
    glm::vec3(-1.7f, 3.0f, -7.5f),
    glm::vec3(1.3f, -2.0f, -2.5f),
    glm::vec3(1.5f, 2.0f, -2.5f),
    glm::vec3(1.5f, 0.2f, -1.5f),
    glm::vec3(-1.3f, 1.0f, -1.5f)};


enum BlockTypes
{
  NONE   = 0,
  GRASS  = 1,
  DIRT   = 2,
  PORTAL = 3,
  CASTLE = 4
};

glm::uint8 layer1[2][9][9] = {
  {
    { 1,1,2,1,1,1,1,1,1 },
    { 1,1,2,1,1,1,1,1,1 },
    { 1,1,2,1,1,1,1,1,1 },
    { 1,1,2,1,2,2,2,1,1 },
    { 1,1,2,1,2,1,2,1,1 },
    { 1,1,2,2,2,1,2,1,1 },
    { 1,1,1,1,1,1,2,1,1 },
    { 1,1,1,1,1,1,2,1,1 },
    { 1,1,1,1,1,1,2,1,1 },
  },
  {
    { 0,0,0,0,0,0,0,0,0 },
    { 0,0,3,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,0,0,0 },
    { 0,0,0,0,0,0,4,0,0 },
    { 0,0,0,0,0,0,0,0,0 },
  }
};

glm::vec3 slimePath[15] = {
  glm::vec3(2.0f,1.0f,1.0f),
  glm::vec3(2.0f,1.0f,2.0f),
  glm::vec3(2.0f,1.0f,3.0f),
  glm::vec3(2.0f,1.0f,4.0f),
  glm::vec3(2.0f,1.0f,5.0f),
  glm::vec3(3.0f,1.0f,5.0f),
  glm::vec3(4.0f,1.0f,5.0f),
  glm::vec3(4.0f,1.0f,4.0f),
  glm::vec3(4.0f,1.0f,3.0f),
  glm::vec3(5.0f,1.0f,3.0f),
  glm::vec3(6.0f,1.0f,3.0f),
  glm::vec3(6.0f,1.0f,4.0f),
  glm::vec3(6.0f,1.0f,5.0f),
  glm::vec3(6.0f,1.0f,6.0f),
  glm::vec3(6.0f,1.0f,7.0f)
};
struct TransformComponent : public ECSComponent<TransformComponent>
{
  glm::vec3 position;
  glm::vec3 scale;
};

struct ColorComponent : public ECSComponent<ColorComponent>
{
  glm::vec4 color;
};

struct SlimeMovementComponent : public ECSComponent<SlimeMovementComponent>
{
  glm::uint8 targetIndex;
  glm::uint8 startIndex;
  glm::uint8 endIndex;
  float speed;
  float maxHeight;
  float minHeight;
  
};

class RenderCubeSystem : public BaseECSSystem
{
public:
  RenderCubeSystem() : BaseECSSystem()
  {
    AddComponentType(TransformComponent::ID);
    AddComponentType(ColorComponent::ID);
  }

  virtual void UpdateComponents(float deltaTime, BaseECSComponent** components)
  {
    //TransformComponent* transform = (TransformComponent*)components[0];
    //transform->position.y += -0.1f * deltaTime;
  }
private:
};

class MoveSlimeSystem : public BaseECSSystem
{
public:
  MoveSlimeSystem() : BaseECSSystem()
  {
    AddComponentType(TransformComponent::ID);
    AddComponentType(SlimeMovementComponent::ID);
  }

  virtual void UpdateComponents(float delta, BaseECSComponent** components)
  {
    TransformComponent* transform = (TransformComponent*)components[0];
    SlimeMovementComponent* slimeMovement = (SlimeMovementComponent*)components[1];

    transform->position = lerp(
      transform->position,
      slimePath[slimeMovement->targetIndex],
      delta
    );

    float distance = glm::length(transform->position - slimePath[slimeMovement->targetIndex]);
    
    slimeMovement->targetIndex = (distance < 0.1f) ? slimeMovement->targetIndex+1 : slimeMovement->targetIndex;
    
    transform->position = (slimeMovement->endIndex < slimeMovement->targetIndex) ? slimePath[slimeMovement->startIndex] : transform->position;

    slimeMovement->targetIndex = (slimeMovement->endIndex < slimeMovement->targetIndex) ? slimeMovement->startIndex : slimeMovement->targetIndex;
  }
private:
  glm::vec3 lerp(glm::vec3& x, glm::vec3& y, float t)
  {
    return x * (1.f - t) + y * t;
  }
};



// Create Components

// Create Entities

// Create Systems
RenderCubeSystem renderCubeSystem;
MoveSlimeSystem moveSlimeSystem;
ECSSystemList mainSystem;

std::vector<EntityHandle> terrainEntities = {};



App::App()
{
  Canis::Log("Object Made");
}
App::~App()
{
  Canis::Log("Object Destroyed");
}

void App::Run()
{
  if (appState == AppState::ON)
    Canis::FatalError("App already running.");
  
  Canis::Init();

  unsigned int windowFlags = 0;

  // windowFlags |= Canis::WindowFlags::FULLSCREEN;

  // windowFlags |= Canis::WindowFlags::BORDERLESS;

  window.Create("Canis", 800, 600, windowFlags);

  time.init(120);

  Load();

  appState = AppState::ON;

  Loop();
}

void App::Load()
{
  // ECS
  TransformComponent transformComponent;
  ColorComponent colorComponent;
  SlimeMovementComponent slimeMovementComponent;
  
  for(int y; y < 2; y++)
  {
    for(int x = 0; x < 9; x++)
    {
      for(int z = 0; z < 9; z++)
      {
        switch (layer1[y][z][x]) // this looks bad
        {
        case GRASS:
          colorComponent.color = glm::vec4(0.15f, 0.52f, 0.30f, 1.0f); // #26854c
          transformComponent.position = glm::vec3(x, y, z);
          transformComponent.scale = glm::vec3(1,1,1);
          terrainEntities.push_back(ecs.MakeEntity(transformComponent,colorComponent));
          break;
        case DIRT:
          colorComponent.color = glm::vec4(0.91f, 0.82f, 0.51f, 1.0f); // #e8d282
          transformComponent.position = glm::vec3(x, y, z);
          transformComponent.scale = glm::vec3(1,1,1);
          terrainEntities.push_back(ecs.MakeEntity(transformComponent,colorComponent));
          break;
        case PORTAL:
          colorComponent.color = glm::vec4(0.21f, 0.77f, 0.96f, 1.0f); // #36c5f4
          transformComponent.position = glm::vec3(x, y, z);
          transformComponent.scale = glm::vec3(1,1,1);
          terrainEntities.push_back(ecs.MakeEntity(transformComponent,colorComponent));
          break;
        case CASTLE:
          colorComponent.color = glm::vec4(0.69f, 0.65f, 0.72f, 1.0f); // #b0a7b8
          transformComponent.position = glm::vec3(x, y, z);
          transformComponent.scale = glm::vec3(1,1,1);
          terrainEntities.push_back(ecs.MakeEntity(transformComponent,colorComponent));
          break;
        default:
          break;
        }
      }
    }
  }

  colorComponent.color = glm::vec4(0.35f, 0.71f, 0.32f, 0.8f); // #5ab552
  transformComponent.position = slimePath[0];
  transformComponent.scale = glm::vec3(0.8f,0.8f,0.8f);
  slimeMovementComponent.targetIndex = 1;
  slimeMovementComponent.startIndex = 0;
  slimeMovementComponent.endIndex = 14;
  slimeMovementComponent.speed = 2;
  terrainEntities.push_back(ecs.MakeEntity(transformComponent,colorComponent,slimeMovementComponent));

  // System
  mainSystem.AddSystem(renderCubeSystem);
  mainSystem.AddSystem(moveSlimeSystem);

  // configure global opengl state
  glEnable(GL_DEPTH_TEST);
  // build and compile our shader program
  shader.Compile("assets/shaders/sts.block.vs", "assets/shaders/sts.block.fs");
  shader.AddAttribute("aPos");
  shader.Link();

  // unsigned int VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
  // load texture 1
  texture1 = Canis::LoadPNGToGLTexture("assets/textures/container.png", GL_RGBA, GL_RGBA);
  // load texture 2
  texture2 = Canis::LoadPNGToGLTexture("assets/textures/awesomeface.png", GL_RGBA, GL_RGBA);

  // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
  // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
  glBindVertexArray(0);

  // wireframe
  // glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
  // fill
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  // start timer
  previousTime = high_resolution_clock::now();
}

void App::Loop()
{
  while (appState == AppState::ON)
  {
    deltaTime = time.startFrame();

    Update();
    Draw();
    // Get SDL to swap our buffer
    window.SwapBuffer();
    LateUpdate();
    FixedUpdate(deltaTime);
    InputUpdate();

    float fps = time.endFrame();

    Canis::Log("fps : " + std::to_string(fps) + " deltaTime : " + std::to_string(deltaTime));
    //Canis::Log("x : " + std::to_string(camera.Position.x) + " y : " + std::to_string(camera.Position.y) +" z : " + std::to_string(camera.Position.z));
  }
}
void App::Update()
{
  ecs.UpdateSystems(mainSystem, deltaTime);
}
void App::Draw()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // also clear the depth buffer now!

  // activate shader
  shader.Use();

  // create transformations
  glm::mat4 view = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
  glm::mat4 projection = glm::mat4(1.0f);
  projection = glm::perspective(glm::radians(camera.Zoom), (float)window.GetScreenWidth() / (float)window.GetScreenHeight(), 0.1f, 100.0f);
  view = camera.GetViewMatrix();
  // pass transformation matrices to the shader
  shader.SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
  shader.SetMat4("view", view);

  // render boxes
  glBindVertexArray(VAO);

  for(int i = 0; i < terrainEntities.size(); i++)
  {
    // calculate the model matrix for each object and pass it to shader before drawing
    glm::vec3& position = ecs.GetComponent<TransformComponent>(terrainEntities[i])->position;
    glm::vec3& scale = ecs.GetComponent<TransformComponent>(terrainEntities[i])->scale;
    glm::vec4& color = ecs.GetComponent<ColorComponent>(terrainEntities[i])->color;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, scale);
    shader.SetMat4("model", model);
    shader.SetVec4("fColor", color);

    glDrawArrays(GL_TRIANGLES, 0, 36);
  }

  shader.UnUse();
}

void App::LateUpdate() {}
void App::FixedUpdate(float dt)
{
    if (inputManager.isKeyPressed(SDLK_w))
  {
    camera.ProcessKeyboard(Canis::Camera_Movement::FORWARD, dt);
  }

  if (inputManager.isKeyPressed(SDLK_s))
  {
    camera.ProcessKeyboard(Canis::Camera_Movement::BACKWARD, dt);
  }

  if (inputManager.isKeyPressed(SDLK_a))
  {
    camera.ProcessKeyboard(Canis::Camera_Movement::LEFT, dt);
  }

  if (inputManager.isKeyPressed(SDLK_d))
  {
    camera.ProcessKeyboard(Canis::Camera_Movement::RIGHT, dt);
  }
}

void App::InputUpdate()
{
  SDL_Event event;
  while (SDL_PollEvent(&event))
  {
    switch (event.type)
    {
    case SDL_QUIT:
      appState = AppState::OFF;
      break;
    case SDL_MOUSEMOTION:
      camera.ProcessMouseMovement(
        event.motion.xrel,
        -event.motion.yrel
      );
      break;
    case SDL_KEYUP:
      inputManager.releasedKey(event.key.keysym.sym);
      break;
    case SDL_KEYDOWN:
      inputManager.pressKey(event.key.keysym.sym);
      break;
    }
  }
}