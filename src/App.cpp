#include "App.hpp"

// set up vertex data (and buffer(s)) and configure vertex attributes
// ------------------------------------------------------------------
float vertices[] = {
    // positions           // texture coords
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,
    0.5f, -0.5f, -0.5f, 1.0f, 0.0f,
    0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
    0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 0.0f,

    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
    0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
    0.5f, 0.5f, 0.5f, 1.0f, 1.0f,
    -0.5f, 0.5f, 0.5f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,

    -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
    -0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
    -0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

    0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
    0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
    0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,
    0.5f, -0.5f, -0.5f, 1.0f, 1.0f,
    0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
    0.5f, -0.5f, 0.5f, 1.0f, 0.0f,
    -0.5f, -0.5f, 0.5f, 0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, 0.0f, 1.0f,

    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f,
    0.5f, 0.5f, -0.5f, 1.0f, 1.0f,
    0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
    0.5f, 0.5f, 0.5f, 1.0f, 0.0f,
    -0.5f, 0.5f, 0.5f, 0.0f, 0.0f,
    -0.5f, 0.5f, -0.5f, 0.0f, 1.0f};

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

  timer.init(60);

  Load();

  appState = AppState::ON;

  Loop();
}

void App::Load()
{
  // configure global opengl state
  glEnable(GL_DEPTH_TEST);
  // build and compile our shader program
  shader.Compile("assets/shaders/7.4.camera.vs", "assets/shaders/7.4.camera.fs");
  shader.AddAttribute("aPos");
  shader.AddAttribute("aTexCoord");
  shader.Link();

  // unsigned int VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // load texture 1
  texture1 = Canis::LoadPNGToGLTexture("assets/textures/container.png", GL_RGBA, GL_RGBA);
  // load texture 2
  texture2 = Canis::LoadPNGToGLTexture("assets/textures/awesomeface.png", GL_RGBA, GL_RGBA);

  // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
  // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
  glBindVertexArray(0);

  // tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
  // -------------------------------------------------------------------------------------------
  shader.Use(); // don't forget to activate/use the shader before setting uniforms!
  // either set it manually like so:
  glUniform1i(glGetUniformLocation(shader.GetProgramID(), "texture1"), 0);
  glUniform1i(glGetUniformLocation(shader.GetProgramID(), "texture2"), 1);
  // or set it via the texture class
  // shader.SetInt("texture1", 0);
  // shader.SetInt("texture2", 1);

  // wireframe
  // glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
  // fill
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

  // start timer
  previousTime = std::chrono::high_resolution_clock::now();
}

void App::Loop()
{
  while (appState == AppState::ON)
  {
    timer.begin();

    currentTime = std::chrono::high_resolution_clock::now();
    deltaTime = std::chrono::duration_cast<std::chrono::nanoseconds>(currentTime - previousTime).count() / 1000000000.0;
    previousTime = currentTime;
    Canis::Log("" + std::to_string(deltaTime));
    Update();
    Draw();
    // Get SDL to swap our buffer
    window.SwapBuffer();
    LateUpdate();
    FixedUpdate(deltaTime);
    InputUpdate();

    timer.end();
  }
}
void App::Update() {}
void App::Draw()
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // also clear the depth buffer now!

  // bind textures on corresponding texture units
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture1.id);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, texture2.id);

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
  for (unsigned int i = 0; i < 10; i++)
  {
    // calculate the model matrix for each object and pass it to shader before drawing
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, cubePositions[i]);
    float angle = 20.0f * i;
    model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
    shader.SetMat4("model", model);

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