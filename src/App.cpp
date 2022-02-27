#include "App.hpp"

App::App()
{
  Engine::Log("Object Made");
}
App::~App()
{
  Engine::Log("Object Destroyed");
}

void App::Run()
{
  if (appState == AppState::ON)
    Engine::FatalError("App already running.");

  Engine::Init();

  unsigned int windowFlags = 0;

  // windowFlags |= Engine::WindowFlags::FULLSCREEN;

  // windowFlags |= Engine::WindowFlags::BORDERLESS;

  window.Create("Engine", 800, 600, windowFlags);

  Load();

  appState = AppState::ON;

  Loop();
}

void App::Load()
{
  // build and compile our shader program
  shader.Compile("assets/shaders/5.1.transform.vs","assets/shaders/5.1.transform.fs");
  shader.AddAttribute("aPos");
  shader.AddAttribute("aTexCoord");
  shader.Link();
  // set up vertex data (and buffer(s)) and configure vertex attributes
  // ------------------------------------------------------------------
  float vertices[] = {
      // positions          // texture coords
       0.5f,  0.5f, 0.0f,   1.0f, 1.0f,   // top right
       0.5f, -0.5f, 0.0f,   1.0f, 0.0f,   // bottom right
      -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,   // bottom left
      -0.5f,  0.5f, 0.0f,   0.0f, 1.0f    // top left 
  };
  unsigned int indices[] = {
      0, 1, 3, // first triangle
      1, 2, 3, // second triangle
  };

  //unsigned int VBO, VAO;
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);
  glGenBuffers(1, &EBO);

  // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

  // position attribute
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  // texture coord attribute
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // load texture 1
  texture1 = Engine::LoadPNGToGLTexture("assets/textures/container.png", GL_RGBA, GL_RGBA);
  // load texture 2
  texture2 = Engine::LoadPNGToGLTexture("assets/textures/awesomeface.png", GL_RGBA, GL_RGBA);

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
  //shader.SetInt("texture1", 0);
  //shader.SetInt("texture2", 1);

  // wireframe
  // glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
  // fill
  glPolygonMode( GL_FRONT_AND_BACK, GL_FILL);
}

void App::Loop()
{
  while (appState == AppState::ON)
  {
    Update();
    Draw();
    // Get SDL to swap our buffer
    window.SwapBuffer();
    LateUpdate();
    FixedUpdate(0.0f);
    InputUpdate();
  }
}
void App::Update() {}
void App::Draw()
{
  glClear(GL_COLOR_BUFFER_BIT);

    // bind textures on corresponding texture units
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture1.id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture2.id);

    // create transformations
    glm::mat4 transform = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
    transform = glm::translate(transform, glm::vec3(0.5f, -0.5f, 0.0f));
    transform = glm::rotate(transform, (float)SDL_GetTicks(), glm::vec3(0.0f, 0.0f, 1.0f));

    // get matrix's uniform location and set matrix
    shader.Use();
    unsigned int transformLoc = glGetUniformLocation(shader.GetProgramID(), "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));

    // render container
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    shader.UnUse();
}

void App::LateUpdate() {}
void App::FixedUpdate(float _delta_time) {}
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
      break;
    case SDL_KEYUP:
      break;
    case SDL_KEYDOWN:
      break;
    }
  }
}