#include "App.hpp"

// set up vertex data (and buffer(s)) and configure vertex attributes
// ------------------------------------------------------------------
float vertices[] = {
	// positions          // normals           // texture coords
	-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
	 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
	 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
	 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
	-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

	-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
	 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
	 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
	 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
	-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,

	-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
	-0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
	-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
	-0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
	-0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
	-0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

	 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
	 0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
	 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
	 0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
	 0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,

	-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
	 0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
	 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
	 0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,

	-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
	 0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
	 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
	 0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
	-0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
	-0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
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
		{ 0,0,0,5,0,5,0,0,0 },
		{ 0,0,0,0,0,0,0,0,0 },
		{ 0,0,0,0,0,0,0,0,0 },
		{ 0,0,0,0,0,0,4,0,0 },
		{ 0,0,0,0,0,0,0,0,0 },
	}
};

enum BlockTypes
{
	NONE   = 0,
	GRASS  = 1,
	DIRT   = 2,
	PORTAL = 3,
	CASTLE = 4,
	SPIKETOWER  = 5
};

RenderCubeSystem renderCubeSystem;
PortalSystem portalSystem;
CastleSystem castleSystem;
MoveSlimeSystem moveSlimeSystem;
SpikeSystem spikeSystem;
SpikeTowerSystem spikeTowerSystem;


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

	window.Create("Canis", 1280, 720, windowFlags);

	time.init(1000);

	Load();

	appState = AppState::ON;

	Loop();
}
void App::Load()
{
	// configure global opengl state
	glEnable(GL_DEPTH_TEST);
	// build and compile our shader program
	shader.Compile("assets/shaders/lighting.vs", "assets/shaders/lighting.fs");
	shader.AddAttribute("aPos");
	shader.AddAttribute("aNormal");
	shader.AddAttribute("aTexcoords");
	shader.Link();

	// unsigned int VBO, VAO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coords
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

	// note that this is allowed, the call to glVertexAttribPointer registered VBO as the vertex attribute's bound vertex buffer object so afterwards we can safely unbind
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// You can unbind the VAO afterwards so other VAO calls won't accidentally modify this VAO, but this rarely happens. Modifying other
	// VAOs requires a call to glBindVertexArray anyways so we generally don't unbind VAOs (nor VBOs) when it's not directly necessary.
	glBindVertexArray(0);

	// wireframe
	// glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	// fill
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// ECS
	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 9; x++)
		{
			for (int z = 0; z < 9; z++)
			{
				const auto entity = entity_registry.create();
				switch (layer1[y][z][x]) // this looks bad
				{
				case GRASS:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.15f, 0.52f, 0.30f, 1.0f) // #26854c
					);
					break;
				case DIRT:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.91f, 0.82f, 0.51f, 1.0f) // #e8d282
					);
					break;
				case PORTAL:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.21f, 0.77f, 0.96f, 1.0f) // #36c5f4
					);
					entity_registry.emplace<PortalComponent>(entity,
						3.0f,
						0.1f
					);
					break;
				case CASTLE:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.69f, 0.65f, 0.72f, 1.0f) // #b0a7b8
					);
					entity_registry.emplace<CastleComponent>(entity,
						20,
						20
					);
					break;
				case SPIKETOWER:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(0.5f, 0.5f, 0.5f) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.15f, 0.52f, 0.30f, 1.0f) // #26854c
					);
					entity_registry.emplace<SpikeTowerComponent>(entity,
						false, // setup
						0, // numOfSpikes
						3.0f, // timeToSpawn
						0.1f // currentTime
					);
				default:
					break;
				}
			}
		}
	}



	renderCubeSystem.VAO = VAO;
	renderCubeSystem.shader = &shader;
	renderCubeSystem.camera = &camera;
	renderCubeSystem.window = &window;

	castleSystem.refRegistry = &entity_registry;

	portalSystem.refRegistry = &entity_registry;

	spikeSystem.refRegistry = &entity_registry;

	spikeTowerSystem.refRegistry = &entity_registry;

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

		window.SetWindowName("Canis : StopTheSlimes fps : " + std::to_string(fps) + " deltaTime : " + std::to_string(deltaTime) + " Enitity : " + std::to_string(entity_registry.size()));
	}
}
void App::Update()
{
	castleSystem.UpdateComponents(deltaTime, entity_registry);
	portalSystem.UpdateComponents(deltaTime, entity_registry);
	moveSlimeSystem.UpdateComponents(deltaTime, entity_registry);
	spikeSystem.UpdateComponents(deltaTime, entity_registry);
	spikeTowerSystem.UpdateComponents(deltaTime, entity_registry);
}
void App::Draw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // also clear the depth buffer now!

	renderCubeSystem.UpdateComponents(deltaTime, entity_registry);
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
				-event.motion.yrel);
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