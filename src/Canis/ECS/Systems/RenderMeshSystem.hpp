#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../../Shader.hpp"
#include "../../Camera.hpp"
#include "../../Window.hpp"
#include "../../IOManager.hpp"
#include "../../External/entt.hpp"

#include "../Components/TransformComponent.hpp"
#include "../Components/ColorComponent.hpp"
#include "../Components/MeshComponent.hpp"

class RenderMeshSystem
{
public:
	Canis::Shader *shader;
	Canis::Shader *shadowShader;
	Canis::Camera *camera;
	Canis::Window *window;
	Canis::GLTexture *diffuseColorPaletteTexture;
	Canis::GLTexture *specularColorPaletteTexture;

	const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
	unsigned int depthMapFBO, depthMap;

	glm::mat4 lightProjection, lightView, lightSpaceMatrix;
    float near_plane = 1.0f, far_plane = 100.0f;

	glm::vec3 lightPos = glm::vec3(-2.0f, 4.0f, -1.0f);

	void Init()
	{
		// configure depth map FBO
		// -----------------------
		glGenFramebuffers(1, &depthMapFBO);
		// create depth texture
		glGenTextures(1, &depthMap);
		glBindTexture(GL_TEXTURE_2D, depthMap);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// attach depth texture as FBO's depth buffer
		glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

        lightProjection = glm::ortho(-50.0f, 50.0f, -50.0f, 50.0f, near_plane, far_plane);
        lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        lightSpaceMatrix = lightProjection * lightView;
	}

	void DrawShadow(float deltaTime, entt::registry &registry)
	{
		/*
		uniform mat4 lightSpaceMatrix;
		uniform mat4 model;
		*/
		// render
        // ------
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 1. render depth of scene to texture (from light's perspective)
        // --------------------------------------------------------------
        // render scene from light's point of view
        shadowShader->Use();
        shadowShader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
		glClear(GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, diffuseColorPaletteTexture->id);

		auto view = registry.view<const TransformComponent, ColorComponent, MeshComponent>();

		for(auto [entity, transform, color, mesh]: view.each())
		{
			if(transform.active == true)
			{
				glBindVertexArray(mesh.vao);
				
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, transform.position);
				model = glm::rotate(model, transform.rotation.x, glm::vec3(1,0,0));
				model = glm::rotate(model, transform.rotation.y, glm::vec3(0,1,0));
				model = glm::rotate(model, transform.rotation.z, glm::vec3(0,0,1));
				model = glm::scale(model, transform.scale);
				shadowShader->SetMat4("model", model);
				shadowShader->SetVec4("color", color.color);

				glDrawArrays(GL_TRIANGLES, 0, mesh.size);
			}
		}

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
		shadowShader->UnUse();

        // reset viewport
        glViewport(0, 0, window->GetScreenWidth(), window->GetScreenHeight());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void DrawModels(float deltaTime, entt::registry &registry)
	{
		shader->Use();

		shader->SetVec3("viewPos", camera->Position);
        shader->SetVec3("lightPos", lightPos);

		// create transformations
		glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 projection = glm::mat4(1.0f);
		projection = glm::perspective(glm::radians(30.0f), (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 100.0f);
		//projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
		cameraView = camera->GetViewMatrix();
		// pass transformation matrices to the shader
		shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
		shader->SetMat4("view", cameraView);
		shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, diffuseColorPaletteTexture->id);

		shader->SetInt("diffuseTexture", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthMap);

		shader->SetInt("shadowMap", 1);

		//glActiveTexture(GL_TEXTURE1);
		//glBindTexture(GL_TEXTURE_2D, specularColorPaletteTexture->id);

		//shader->SetInt("material.specular", 1);

		auto view = registry.view<const TransformComponent, ColorComponent, MeshComponent>();

		for(auto [entity, transform, color, mesh]: view.each())
		{
			if(transform.active == true)
			{
				glBindVertexArray(mesh.vao);
				
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, transform.position);
				model = glm::rotate(model, transform.rotation.x, glm::vec3(1,0,0));
				model = glm::rotate(model, transform.rotation.y, glm::vec3(0,1,0));
				model = glm::rotate(model, transform.rotation.z, glm::vec3(0,0,1));
				model = glm::scale(model, transform.scale);
				shader->SetMat4("model", model);
				shader->SetVec4("color", color.color);

				glDrawArrays(GL_TRIANGLES, 0, mesh.size);
			}
		}

		glBindVertexArray(0);

		shader->UnUse();
	}

	void UpdateComponents(float deltaTime, entt::registry &registry)
	{
		DrawShadow(deltaTime, registry);
		DrawModels(deltaTime, registry);
	}

private:
};

	/*void UpdateComponents(float deltaTime, entt::registry &registry)
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
		projection = glm::perspective(glm::radians(30.0f), (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 100.0f);
		//projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
		cameraView = camera->GetViewMatrix();
		// pass transformation matrices to the shader
		shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
		shader->SetMat4("view", cameraView);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, diffuseColorPaletteTexture->id);

		shader->SetInt("material.diffuse", 0);

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, specularColorPaletteTexture->id);

		shader->SetInt("material.specular", 1);

		shader->SetFloat("material.shininess", 32.0f);

		auto view = registry.view<const TransformComponent, ColorComponent, MeshComponent>();

		for(auto [entity, transform, color, mesh]: view.each())
		{
			if(transform.active == true)
			{
				glBindVertexArray(mesh.vao);
				
				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, transform.position);
				model = glm::rotate(model, transform.rotation.x, glm::vec3(1,0,0));
				model = glm::rotate(model, transform.rotation.y, glm::vec3(0,1,0));
				model = glm::rotate(model, transform.rotation.z, glm::vec3(0,0,1));
				model = glm::scale(model, transform.scale);
				shader->SetMat4("model", model);
				shader->SetVec4("color", color.color);

				glDrawArrays(GL_TRIANGLES, 0, mesh.size);
			}
		}

		glBindVertexArray(0);

		shader->UnUse();
	}*/