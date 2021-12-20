#include "Game.hpp"

Game::Game() {}
Game::~Game() {}

void Game::Run() {
    Canis::Init();

    frame_counter = 0;

    window.Spawn("Canis Engine Demo", 800, 600, 0);

    Load();

    target_fixed_update_ticks = (1.0f / 16.0f) * 1000;
    last_fixed_update = SDL_GetTicks();  

    frame_limiter.Init(60.0f);

    Loop();
}

void Game::Load() {
    // build and compile our shader program
    // ------------------------------------
    // vertex shader
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);
    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // fragment shader
    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    // check for shader compile errors
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    // link shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    // check for linking errors
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
        -0.5f, -0.5f, 0.0f, // left  
         0.5f, -0.5f, 0.0f, // right 
         0.0f,  0.5f, 0.0f  // top   
    }; 

    //unsigned int VBO, VAO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    // bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0); 
}

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

        window.SwapBuffer();

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

void Game::Update() {

}

void Game::Draw() {
    Canis::Log("Draw");

    glClearColor(0.2f,0.3,0.3f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // draw first triangle
    glUseProgram(shaderProgram);
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

void Game::LateUpdate() {

}

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