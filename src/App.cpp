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

RenderMeshSystem renderMeshSystem;
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
PlacementToolSystem placementToolSystem;



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

	window.MouseLock(mouseLock);

	appState = AppState::ON;

	Loop();
}
void App::Load()
{
	// configure global opengl state
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_ALPHA);
	//glEnable(GL_CULL_FACE);
	// build and compile our shader program
	shader.Compile("assets/shaders/sts.block.vs", "assets/shaders/sts.block.fs");
	shader.AddAttribute("aPos");
	shader.AddAttribute("aNormal");
	shader.AddAttribute("aTexcoords");
	shader.Link();

	towerShader.Compile("assets/shaders/lighting.vs", "assets/shaders/lighting.fs");
	towerShader.AddAttribute("a_pos");
	towerShader.AddAttribute("a_normal");
	towerShader.AddAttribute("a_texcoords");
	towerShader.Link();

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

	// Load color palette
	diffuseColorPaletteTexture = Canis::LoadPNGToGLTexture("assets/textures/palette/diffuse.png", GL_RGBA, GL_RGBA);
	specularColorPaletteTexture = Canis::LoadPNGToGLTexture("assets/textures/palette/specular.png", GL_RGBA, GL_RGBA);

	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;

	std::vector<Canis::Vertex> vecVertex;

	bool res = Canis::LoadOBJ("assets/models/towers/fire_crystal_tower.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	fireTowerSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &fireTowerVAO);
	glGenBuffers(1, &fireTowerVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(fireTowerVAO);

	glBindBuffer(GL_ARRAY_BUFFER, fireTowerVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/towers/gem_mine_tower.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	goldTowerSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &goldTowerVAO);
	glGenBuffers(1, &goldTowerVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(goldTowerVAO);

	glBindBuffer(GL_ARRAY_BUFFER, goldTowerVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/towers/ice_crystal_tower.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	iceTowerSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &iceTowerVAO);
	glGenBuffers(1, &iceTowerVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(iceTowerVAO);

	glBindBuffer(GL_ARRAY_BUFFER, iceTowerVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/towers/tree_tower.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	spikeTowerSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &spikeTowerVAO);
	glGenBuffers(1, &spikeTowerVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(spikeTowerVAO);

	glBindBuffer(GL_ARRAY_BUFFER, spikeTowerVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/blocks/white_block.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	whiteCubeSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &whiteCubeVAO);
	glGenBuffers(1, &whiteCubeVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(whiteCubeVAO);

	glBindBuffer(GL_ARRAY_BUFFER, whiteCubeVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/towers/root.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	rootSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &rootVAO);
	glGenBuffers(1, &rootVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(rootVAO);

	glBindBuffer(GL_ARRAY_BUFFER, rootVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/towers/fire_crystal.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	fireCrystalSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &fireCrystalVAO);
	glGenBuffers(1, &fireCrystalVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(fireCrystalVAO);

	glBindBuffer(GL_ARRAY_BUFFER, fireCrystalVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/tree_group.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	treeGroupSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &treeGroupVAO);
	glGenBuffers(1, &treeGroupVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(treeGroupVAO);

	glBindBuffer(GL_ARRAY_BUFFER, treeGroupVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/castle.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	castleSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &castleVAO);
	glGenBuffers(1, &castleVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(castleVAO);

	glBindBuffer(GL_ARRAY_BUFFER, castleVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	vertices.clear();
	uvs.clear();
	normals.clear();
	vecVertex.clear();

	res = Canis::LoadOBJ("assets/models/portal.obj", vertices, uvs, normals);

	for(int i = 0; i < vertices.size(); i++)
	{
		Canis::Vertex v = {};
		v.Position = vertices[i];
		v.Normal = normals[i];
		v.TexCoords = uvs[i];
		vecVertex.push_back(v);

		//Canis::Log("v : " + glm::to_string(v.Position) + " n : " + glm::to_string(v.Normal) + " t : " + glm::to_string(v.TexCoords));
	}

	portalSize = vecVertex.size();
	Canis::Log("s " + std::to_string(vecVertex.size()));

	glGenVertexArrays(1, &portalVAO);
	glGenBuffers(1, &portalVBO);

	// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
	glBindVertexArray(portalVAO);

	glBindBuffer(GL_ARRAY_BUFFER, portalVBO);
	glBufferData(GL_ARRAY_BUFFER, vecVertex.size() * sizeof(Canis::Vertex), &vecVertex[0], GL_STATIC_DRAW);

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

	glBindVertexArray(0);

	

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Game Systems
	LoadECS();

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
		
		/*Canis::Log(
			"pos: " + glm::to_string(camera.Position) +
			" Yaw: "  + std::to_string(camera.Yaw) +
			" Pitch: "  + std::to_string(camera.Pitch));*/
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

	if (!mouseLock)
	{
		hudManager.Update(deltaTime, entity_registry);
		placementToolSystem.UpdateComponents(deltaTime, entity_registry);
	}
}
void App::Draw()
{
	glDepthFunc(GL_LESS); 
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // also clear the depth buffer now!

	renderMeshSystem.UpdateComponents(deltaTime, entity_registry);
	renderTextSystem.UpdateComponents(deltaTime, entity_registry);
}
void App::LateUpdate() {}
void App::FixedUpdate(float dt)
{
	if (inputManager.isKeyPressed(SDLK_w) && mouseLock)
	{
		camera.ProcessKeyboard(Canis::Camera_Movement::FORWARD, dt);
	}

	if (inputManager.isKeyPressed(SDLK_s) && mouseLock)
	{
		camera.ProcessKeyboard(Canis::Camera_Movement::BACKWARD, dt);
	}

	if (inputManager.isKeyPressed(SDLK_a) && mouseLock)
	{
		camera.ProcessKeyboard(Canis::Camera_Movement::LEFT, dt);
	}

	if (inputManager.isKeyPressed(SDLK_d) && mouseLock)
	{
		camera.ProcessKeyboard(Canis::Camera_Movement::RIGHT, dt);
	}

	if (inputManager.justPressedKey(SDLK_ESCAPE))
    {
		mouseLock = !mouseLock;

        window.MouseLock(mouseLock);
    }
}
void App::InputUpdate()
{
	inputManager.swapMaps();

	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_QUIT:
			appState = AppState::OFF;
			break;
		case SDL_MOUSEMOTION:
			if (mouseLock)
			{
				camera.ProcessMouseMovement(
					event.motion.xrel,
					-event.motion.yrel);
			}
			else
			{
				inputManager.mouse.x = event.motion.x;
				inputManager.mouse.y = event.motion.y;
			}
			break;
		case SDL_KEYUP:
			inputManager.releasedKey(event.key.keysym.sym);
			//Canis::Log("UP" + std::to_string(event.key.keysym.sym));
			break;
		case SDL_KEYDOWN:
			inputManager.pressKey(event.key.keysym.sym);
			//Canis::Log("DOWN");
			break;
		case SDL_MOUSEBUTTONDOWN:
			if(event.button.button == SDL_BUTTON_LEFT)
				inputManager.leftClick = true;
			if(event.button.button == SDL_BUTTON_RIGHT)
				inputManager.rightClick = true;
			break;
		}
	}
}

void App::LoadECS()
{
	// ECS
	for (int y = 0; y < 2; y++)
	{
		for (int x = 0; x < 23; x++)
		{
			for (int z = 0; z < 30; z++)
			{
				if (0 == titleMap[y][z][x])
					continue;
				
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
					entity_registry.emplace<MeshComponent>(entity,
						whiteCubeVAO,
						whiteCubeSize
					);
					entity_registry.emplace<BlockComponent>(entity,
						BlockTypes::GRASS
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
					entity_registry.emplace<MeshComponent>(entity,
						whiteCubeVAO,
						whiteCubeSize
					);
					entity_registry.emplace<BlockComponent>(entity,
						BlockTypes::DIRT
					);
					break;
				case TREEGROUP0:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y-0.5f, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) // #e8d282
					);
					entity_registry.emplace<MeshComponent>(entity,
						treeGroupVAO,
						treeGroupSize
					);
					entity_registry.emplace<BlockComponent>(entity,
						BlockTypes::DIRT
					);
					break;
				case PORTAL:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y-0.5f, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1, 1, 1) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) // #36c5f4
					);
					entity_registry.emplace<MeshComponent>(entity,
						portalVAO,
						portalSize
					);
					entity_registry.emplace<PortalComponent>(entity,
						2.0f,
						0.1f
					);
					break;
				case CASTLE:
					entity_registry.emplace<TransformComponent>(entity,
						true, // active
						glm::vec3(x, y-0.5f, z), // position
						glm::vec3(0.0f, 0.0f, 0.0f), // rotation
						glm::vec3(1.2f, 1.2f, 1.2f) // scale
					);
					entity_registry.emplace<ColorComponent>(entity,
						glm::vec4(0.69f, 0.65f, 0.72f, 1.0f) // #b0a7b8
					);
					entity_registry.emplace<MeshComponent>(entity,
						castleVAO,
						castleSize
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
					entity_registry.emplace<MeshComponent>(entity,
						spikeTowerVAO,
						spikeTowerSize
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
					entity_registry.emplace<MeshComponent>(entity,
						goldTowerVAO,
						goldTowerSize
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
					entity_registry.emplace<MeshComponent>(entity,
						fireTowerVAO,
						fireTowerSize
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
					entity_registry.emplace<MeshComponent>(entity,
						iceTowerVAO,
						iceTowerSize
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

	hudManager.inputManager = &inputManager;
	hudManager.window = &window;
	hudManager.wallet = &wallet;
	hudManager.whiteCubeVAO = whiteCubeVAO;
	hudManager.whiteCubeSize = whiteCubeSize;
	hudManager.Load(entity_registry);

	renderTextSystem.Init();
	renderTextSystem.camera = &camera;
	renderTextSystem.window = &window;

	castleSystem.refRegistry = &entity_registry;
	castleSystem.healthText = hudManager.healthText;
	castleSystem.Init();

	wallet.refRegistry = &entity_registry;
	wallet.walletText = hudManager.walletText;
	wallet.SetCash(200);

	scoreSystem.refRegistry = &entity_registry;
	scoreSystem.scoreText = hudManager.scoreText;

	portalSystem.refRegistry = &entity_registry;
	portalSystem.whiteCubeVAO = whiteCubeVAO;
	portalSystem.whiteCubeSize = whiteCubeSize;

	spikeSystem.refRegistry = &entity_registry;

	spikeTowerSystem.refRegistry = &entity_registry;
	spikeTowerSystem.rootVAO = rootVAO;
	spikeTowerSystem.rootSize = rootSize;

	fireTowerSystem.fireCrystalVAO = fireCrystalVAO;
	fireTowerSystem.fireCrystalSize = fireCrystalSize;

	gemMineTowerSystem.wallet = &wallet;

	moveSlimeSystem.wallet = &wallet;
	moveSlimeSystem.scoreSystem = &scoreSystem;
	moveSlimeSystem.portalSystem = &portalSystem;
	moveSlimeSystem.aStar = &aStar;
	moveSlimeSystem.Init();

	placementToolSystem.inputManager = &inputManager;
	placementToolSystem.aStar = &aStar;
	placementToolSystem.wallet = &wallet;
	placementToolSystem.camera = &camera;
	placementToolSystem.window = &window;
	placementToolSystem.whiteCubeVAO = whiteCubeVAO;
	placementToolSystem.fireTowerVAO = fireTowerVAO;
	placementToolSystem.spikeTowerVAO = spikeTowerVAO;
	placementToolSystem.goldTowerVAO = goldTowerVAO;
	placementToolSystem.iceTowerVAO = iceTowerVAO;
	placementToolSystem.whiteCubeSize = whiteCubeSize;
    placementToolSystem.fireTowerSize = fireTowerSize;
    placementToolSystem.spikeTowerSize = spikeTowerSize;
    placementToolSystem.goldTowerSize = goldTowerSize; 
    placementToolSystem.iceTowerSize = iceTowerSize;

	renderMeshSystem.shader = &towerShader;
	renderMeshSystem.camera = &camera;
	renderMeshSystem.window = &window;
	renderMeshSystem.diffuseColorPaletteTexture = &diffuseColorPaletteTexture;
	renderMeshSystem.specularColorPaletteTexture = &specularColorPaletteTexture;
}