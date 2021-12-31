#include "Game.hpp"

Game::Game() {}
Game::~Game() {
    // optional: de-allocate all resources once they've outlived their purpose:
    // ------------------------------------------------------------------------
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

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
    shader.Compile("bin/shaders/5.1.transform.vs","bin/shaders/5.1.transform.fs");
    shader.AddAttribute("ourColor");
    shader.AddAttribute("TexCoord");
    shader.Link();
    // set up vertex data (and buffer(s)) and configure vertex attributes
    // ------------------------------------------------------------------
    float vertices[] = {
        // positions          // texture coords
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f, // top right
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f, // bottom right
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, // bottom left
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f  // top left 
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

    // color attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // load texture 1
    texture1 = Canis::LoadPNGToGLTexture("bin/Textures/container.png", GL_RGBA, GL_RGBA);
    // load texture 2
    texture2 = Canis::LoadPNGToGLTexture("bin/Textures/awesomeface.png", GL_RGBA, GL_RGBA);

    // note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0); 

    // You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
    // VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
    glBindVertexArray(0);

    // tell opengl for each sampler to which texture unit it belongs to (only has to be done once)
    // -------------------------------------------------------------------------------------------
    shader.Use(); // don't forget to activate/use the shader before setting uniforms!
    // either set it manually like so:
    //glUniform1i(glGetUniformLocation(shader.program_id, "texture1"), texture1.id);
    // or set it via the texture class
    shader.SetInt("texture1", 0);
    shader.SetInt("texture2", 1);
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
    glClearColor(0.2f,0.3,0.3f,1.0f);
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
    unsigned int transformLoc = glGetUniformLocation(shader.program_id, "transform");
    glUniformMatrix4fv(transformLoc, 1, GL_FALSE, glm::value_ptr(transform));

    // render container
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    glBindVertexArray(0);
    shader.UnUse();
}

void Game::LateUpdate() {

}

void Game::FixedUpdate(float _delta_time) {
    //Canis::Log("FixedUpdate: " + std::to_string(_delta_time));
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