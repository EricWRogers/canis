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
		{ 0,0,0,0,0,0,0,0,0 },
		{ 0,0,0,0,0,0,0,0,0 },
		{ 0,0,0,0,0,0,0,0,0 },
		{ 0,0,0,0,0,0,4,0,0 },
		{ 0,0,0,0,0,0,0,0,0 },
	}
};

glm::vec3 slimePath[15] = {
	glm::vec3(2.0f, 1.0f, 1.0f),
	glm::vec3(2.0f, 1.0f, 2.0f),
	glm::vec3(2.0f, 1.0f, 3.0f),
	glm::vec3(2.0f, 1.0f, 4.0f),
	glm::vec3(2.0f, 1.0f, 5.0f),
	glm::vec3(3.0f, 1.0f, 5.0f),
	glm::vec3(4.0f, 1.0f, 5.0f),
	glm::vec3(4.0f, 1.0f, 4.0f),
	glm::vec3(4.0f, 1.0f, 3.0f),
	glm::vec3(5.0f, 1.0f, 3.0f),
	glm::vec3(6.0f, 1.0f, 3.0f),
	glm::vec3(6.0f, 1.0f, 4.0f),
	glm::vec3(6.0f, 1.0f, 5.0f),
	glm::vec3(6.0f, 1.0f, 6.0f),
	glm::vec3(6.0f, 1.0f, 7.0f)};

enum BlockTypes
{
	NONE   = 0,
	GRASS  = 1,
	DIRT   = 2,
	PORTAL = 3,
	CASTLE = 4
};

struct TransformComponent
{
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
};

struct ColorComponent
{
	glm::vec4 color;
};

struct SlimeMovementComponent
{
	int targetIndex;
	int startIndex;
	int endIndex;
	float speed;
	float maxHeight;
	float minHeight;
};

class RenderCubeSystem
{
public:
	unsigned int VAO;
	Canis::Shader *shader;
	Canis::Camera *camera;
	Canis::Window *window;

	RenderCubeSystem() {}

	void UpdateComponents(float deltaTime, entt::registry &registry)
	{
		// activate shader
		shader->Use();
		shader->SetVec3("viewPos", camera->Position);
		shader->SetInt("numDirLights", 1);
		shader->SetInt("numPointLights", 0);
		shader->SetInt("numSpotLights", 0);

		// directional light
        shader->SetVec3("dirLight.direction", -0.2f, -1.0f, -0.3f);
        shader->SetVec3("dirLight.ambient", 0.05f, 0.05f, 0.05f);
        shader->SetVec3("dirLight.diffuse", 0.8f, 0.8f, 0.8f);
        shader->SetVec3("dirLight.specular", 0.5f, 0.5f, 0.5f);
        // point light 1
        shader->SetVec3("pointLights[0].position", glm::vec3(0.0f,0.0f,0.0f));
        shader->SetVec3("pointLights[0].ambient", 0.05f, 0.05f, 0.05f);
        shader->SetVec3("pointLights[0].diffuse", 0.8f, 0.8f, 0.8f);
    	shader->SetVec3("pointLights[0].specular", 1.0f, 1.0f, 1.0f);
    	shader->SetFloat("pointLights[0].constant", 1.0f);
    	shader->SetFloat("pointLights[0].linear", 0.09f);
    	shader->SetFloat("pointLights[0].quadratic", 0.032f);
        // spotLight
    	shader->SetVec3("spotLight.position", camera->Position);
    	shader->SetVec3("spotLight.direction", camera->Front);
    	shader->SetVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
    	shader->SetVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
    	shader->SetVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
    	shader->SetFloat("spotLight.constant", 1.0f);
    	shader->SetFloat("spotLight.linear", 0.09f);
    	shader->SetFloat("spotLight.quadratic", 0.032f);
    	shader->SetFloat("spotLight.cutOff", glm::cos(glm::radians(12.5f)));
    	shader->SetFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f))); 

		// create transformations
		glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 projection = glm::mat4(1.0f);
		projection = glm::perspective(glm::radians(75.0f), (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 100.0f);
		cameraView = camera->GetViewMatrix();
		// pass transformation matrices to the shader
		shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
		shader->SetMat4("view", cameraView);

		// render boxes
		glBindVertexArray(VAO);

		auto view = registry.view<const TransformComponent, ColorComponent>();

		for(auto [entity, transform, color]: view.each())
		{
			// material properties
			shader->SetVec3("material.ambient", 1.0f, 0.5f, 0.31f);
			shader->SetVec3("material.diffuse", color.color);
			shader->SetFloat("material.shininess", 32.0f);

			glm::mat4 model = glm::mat4(1.0f);
			model = glm::translate(model, transform.position);
			model = glm::scale(model, transform.scale);
			shader->SetMat4("model", model);
			//shader->SetVec4("fColor", color->color);

			glDrawArrays(GL_TRIANGLES, 0, 36);
		}

		shader->UnUse();
	}

private:
};

class MoveSlimeSystem
{
public:
	void UpdateComponents(float delta, entt::registry &registry)
	{
		auto view = registry.view<TransformComponent, SlimeMovementComponent>();

		for(auto [entity, transform, slimeMovement] : view.each())
		{
			transform.position = lerp(
				transform.position,
				slimePath[slimeMovement.targetIndex],
				delta);

			//transform.position = transform.position * (1.0f - delta) + slimePath[slimeMovement.targetIndex] * delta;

			float distance = glm::length(transform.position - slimePath[slimeMovement.targetIndex]);

			slimeMovement.targetIndex = (distance < 0.1f) ? slimeMovement.targetIndex + 1 : slimeMovement.targetIndex;

			transform.position = (slimeMovement.endIndex < slimeMovement.targetIndex) ? slimePath[slimeMovement.startIndex] : transform.position;

			slimeMovement.targetIndex = (slimeMovement.endIndex < slimeMovement.targetIndex) ? slimeMovement.startIndex : slimeMovement.targetIndex;
		}
	}

private:
	glm::vec3 lerp(glm::vec3 x, glm::vec3 y, float t)
	{
		return x * (1.f - t) + y * t;
	}
};

RenderCubeSystem renderCubeSystem;
MoveSlimeSystem moveSlimeSystem;

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
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.21f, 0.77f, 0.96f, 1.0f) // #36c5f4
					);
					break;
				case CASTLE:
					entity_registry.emplace<TransformComponent>(entity,
						glm::vec3(x, y, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.69f, 0.65f, 0.72f, 1.0f) // #b0a7b8
					);
					break;
				default:
					break;
				}
			}
		}
	}

	const entt::entity entity = entity_registry.create();

	entity_registry.emplace<TransformComponent>(entity,
		slimePath[0], // position
		glm::vec3(0.0f, 0.0f, 0.0f), // rotation
		glm::vec3(0.8f, 0.8f, 0.8f) // scale
	);
	entity_registry.emplace<ColorComponent>(entity,
		glm::vec4(0.35f, 0.71f, 0.32f, 0.8f) // #5ab552
	);
	entity_registry.emplace<SlimeMovementComponent>(entity,
		   1, // targetIndex
		   0, // startIndex
		  14, // endIndex
		2.0f, // speed
		1.5f, // maxHeight
		0.5f  // minHeight
	);

	renderCubeSystem.VAO = VAO;
	renderCubeSystem.shader = &shader;
	renderCubeSystem.camera = &camera;
	renderCubeSystem.window = &window;

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
		// Canis::Log("x : " + std::to_string(camera.Position.x) + " y : " + std::to_string(camera.Position.y) +" z : " + std::to_string(camera.Position.z));
	}
}
void App::Update()
{
	moveSlimeSystem.UpdateComponents(deltaTime, entity_registry);
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