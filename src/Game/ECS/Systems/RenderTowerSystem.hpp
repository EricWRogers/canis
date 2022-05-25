#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../../../Canis/Shader.hpp"
#include "../../../Canis/Camera.hpp"
#include "../../../Canis/Window.hpp"
#include "../../../Canis/IOManager.hpp"
#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/TowerMeshComponent.hpp"

class RenderTowerSystem
{
public:
	Canis::Shader *shader;
	Canis::Camera *camera;
	Canis::Window *window;
	Canis::GLTexture *colorPaletteTexture;
	int size = 0;

	void UpdateComponents(float deltaTime, entt::registry &registry)
	{
		// activate shader
		shader->Use();
		
		// create transformations
		glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 projection = glm::mat4(1.0f);
		projection = glm::perspective(glm::radians(30.0f), (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 100.0f);
		//projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
		cameraView = camera->GetViewMatrix();
		// pass transformation matrices to the shader
		shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
		shader->SetMat4("view", cameraView);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorPaletteTexture->id);

		shader->SetInt("myTextureSampler", 0);

		auto view = registry.view<const TransformComponent, ColorComponent, TowerMeshComponent>();

		for(auto [entity, transform, color, mesh]: view.each())
		{
			if(transform.active == true)
			{
				glBindVertexArray(mesh.vao);

				//Canis::Log("vao " + std::to_string(mesh.vao));
				
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, transform.position);
				model = glm::rotate(model, transform.rotation.x, glm::vec3(1,0,0));
				model = glm::rotate(model, transform.rotation.y, glm::vec3(0,1,0));
				model = glm::rotate(model, transform.rotation.z, glm::vec3(0,0,1));
				model = glm::scale(model, transform.scale);
				shader->SetMat4("model", model);
				//shader->SetVec4("fColor", color.color);

				glDrawArrays(GL_TRIANGLES, 0, size);
				//Canis::Log("s " + std::to_string(size));
			}
		}

		glBindVertexArray(0);
		//glActiveTexture(GL_TEXTURE0);

		shader->UnUse();
	}

private:
};

/*#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../../Canis/Shader.hpp"
#include "../../../Canis/Camera.hpp"
#include "../../../Canis/Window.hpp"
#include "../../../Canis/IOManager.hpp"
#include "../../../Canis/External/entt.hpp"

#include "../../../Canis/ECS/Components/TransformComponent.hpp"
#include "../../../Canis/ECS/Components/ColorComponent.hpp"

#include "../Components/TowerMeshComponent.hpp"

struct Vertex
{
	// position
	glm::vec3 Position;
	// normal
	glm::vec3 Normal;
	// texCoords
	glm::vec2 TexCoords;
};

class RenderTowerSystem
{
public:
	Canis::Shader *shader;
	Canis::Camera *camera;
	Canis::Window *window;
	Canis::GLTexture *colorPaletteTexture;
	int size = 0;

	std::vector<glm::vec3> vertex;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;

	std::vector<Vertex> vertices;

	GLuint VAO;
	GLuint VBO;

	void Init()
	{
		// Read our .obj file
		bool res = Canis::LoadOBJ("assets/models/towers/fire_crystal_tower.obj", vertex, uvs, normals);

		Canis::Log("v : " + std::to_string(vertex.size()));
		Canis::Log("u : " + std::to_string(uvs.size()));
		Canis::Log("n : " + std::to_string(normals.size()));

		for(int i = 0; i < vertex.size(); i++)
		{
			Vertex v = {};
			v.Position = vertex[i];
			v.Normal = normals[i];
			v.TexCoords = uvs[i];
			vertices.push_back(v);
		}

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);

		// bind the Vertex Array Object first, then bind and set vertex buffer(s), and then configure vertex attributes(s).
		glBindVertexArray(VAO);

		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

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
	}

	void UpdateComponents(float deltaTime, entt::registry &registry)
	{
		// activate shader
		shader->Use();
		
		// create transformations
		glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 projection = glm::mat4(1.0f);
		projection = glm::perspective(glm::radians(30.0f), (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 100.0f);
		//projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
		cameraView = camera->GetViewMatrix();
		// pass transformation matrices to the shader
		shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
		shader->SetMat4("view", cameraView);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorPaletteTexture->id);

		shader->SetInt("myTextureSampler", 0);

		auto view = registry.view<const TransformComponent, ColorComponent, TowerMeshComponent>();

		for(auto [entity, transform, color, mesh]: view.each())
		{
			if(transform.active == true)
			{
				glBindVertexArray(VAO);

				//Canis::Log("vao " + std::to_string(mesh.vao));
				
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, transform.position);
				model = glm::rotate(model, transform.rotation.x, glm::vec3(1,0,0));
				model = glm::rotate(model, transform.rotation.y, glm::vec3(0,1,0));
				model = glm::rotate(model, transform.rotation.z, glm::vec3(0,0,1));
				model = glm::scale(model, transform.scale);
				shader->SetMat4("model", model);
				//shader->SetVec4("fColor", color.color);

				glDrawArrays(GL_TRIANGLES, 0, vertices.size());
			}
		}

		glBindVertexArray(0);
		//glActiveTexture(GL_TEXTURE0);

		shader->UnUse();
	}

private:
};*/