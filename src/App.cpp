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

RenderCubeSystem renderCubeSystem;
RenderTextSystem renderTextSystem;
PortalSystem portalSystem;
CastleSystem castleSystem;
MoveSlimeSystem moveSlimeSystem;
SpikeSystem spikeSystem;
SpikeTowerSystem spikeTowerSystem;
GemMineTowerSystem gemMineTowerSystem;
FireBallSystem fireBallSystem;
FireTowerSystem fireTowerSystem;
IceTowerSystem iceTowerSystem;
SlimeFreezeSystem slimeFreezeSystem;



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
	// Game Systems

	// ECS
	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 9; x++)
		{
			for (int z = 0; z < 9; z++)
			{
				const auto entity = entity_registry.create();
				switch (titleMap[y][z][x]) // this looks bad
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
						2.0f,
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
					break;
				case GEMMINETOWER:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(0.5f, 0.5f, 0.5f) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.972f, 0.827f, 0.207f, 1.0f) // #f8d335
					);
					entity_registry.emplace<GemMineTowerComponent>(entity,
						20, // gems
						5.0f, // timeToSpawn
						5.0f // currentTime
					);
					break;
				case FIRETOWER:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(0.5f, 0.5f, 0.5f) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.909f, 0.007f, 0.007f, 1.0f) // #e80202
					);
					entity_registry.emplace<FireTowerComponent>(entity,
						1, // damage
						20.0f,// speed
						5.0f, // range
						2.0f, // timeToSpawn
						2.0f // currentTime
					);
					break;
				case ICETOWER:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(0.5f, 0.5f, 0.5f) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.117f, 0.980f, 0.972f, 1.0f) // #e80202
					);
					entity_registry.emplace<IceTowerComponent>(entity,
						1, // maxSlimesToFreeze
						2, // damageOnBreak
						0.5f,// freezeTime
						2.0f, // range
						4.0f, // timeToSpawn
						4.0f // currentTime
					);
					break;
				default:
					break;
				}
			}
		}
	}

	const auto entity = entity_registry.create();
	entity_registry.emplace<RectTransformComponent>(entity,
		true, // active
		glm::vec2(25.0f, window.GetScreenHeight() - 65.0f), // position
		glm::vec2(0.0f, 0.0f), // rotation
		1.0f // scale
	);
	entity_registry.emplace<ColorComponent>(entity,
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
	);
	entity_registry.emplace<TextComponent>(entity,
		new std::string("Health : 0") // text
	);

	const auto entity1 = entity_registry.create();
	entity_registry.emplace<RectTransformComponent>(entity1,
		true, // active
		glm::vec2(25.0f, window.GetScreenHeight() - 130.0f), // position
		glm::vec2(0.0f, 0.0f), // rotation
		1.0f // scale
	);
	entity_registry.emplace<ColorComponent>(entity1,
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
	);
	entity_registry.emplace<TextComponent>(entity1,
		new std::string("Score : 0") // text
	);

	const auto entity2 = entity_registry.create();
	entity_registry.emplace<RectTransformComponent>(entity2,
		true, // active
		glm::vec2(25.0f, window.GetScreenHeight() - 195.0f), // position
		glm::vec2(0.0f, 0.0f), // rotation
		1.0f // scale
	);
	entity_registry.emplace<ColorComponent>(entity2,
		glm::vec4(0.0f, 0.0f, 0.0f, 1.0f) // #26854c
	);
	entity_registry.emplace<TextComponent>(entity2,
		new std::string("Gold : 0") // text
	);

	renderCubeSystem.VAO = VAO;
	renderCubeSystem.shader = &shader;
	renderCubeSystem.camera = &camera;
	renderCubeSystem.window = &window;

	renderTextSystem.Init();
	renderTextSystem.camera = &camera;
	renderTextSystem.window = &window;

	castleSystem.refRegistry = &entity_registry;
	castleSystem.healthText = entity;
	castleSystem.Init();

	wallet.refRegistry = &entity_registry;
	wallet.walletText = entity2;
	wallet.SetCash(200);

	scoreSystem.refRegistry = &entity_registry;
	scoreSystem.scoreText = entity1;

	portalSystem.refRegistry = &entity_registry;

	spikeSystem.refRegistry = &entity_registry;

	spikeTowerSystem.refRegistry = &entity_registry;

	gemMineTowerSystem.wallet = &wallet;

	moveSlimeSystem.wallet = &wallet;
	moveSlimeSystem.scoreSystem = &scoreSystem;

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
	gemMineTowerSystem.UpdateComponents(deltaTime, entity_registry);
	fireBallSystem.UpdateComponents(deltaTime, entity_registry);
	fireTowerSystem.UpdateComponents(deltaTime, entity_registry);
	iceTowerSystem.UpdateComponents(deltaTime, entity_registry);
	slimeFreezeSystem.UpdateComponents(deltaTime, entity_registry);
}
void App::Draw()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // also clear the depth buffer now!

	renderCubeSystem.UpdateComponents(deltaTime, entity_registry);
	renderTextSystem.UpdateComponents(deltaTime, entity_registry);
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