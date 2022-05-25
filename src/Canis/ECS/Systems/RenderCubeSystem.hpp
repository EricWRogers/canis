#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../../Shader.hpp"
#include "../../Camera.hpp"
#include "../../Window.hpp"
#include "../../External/entt.hpp"

#include "../Components/TransformComponent.hpp"
#include "../Components/ColorComponent.hpp"
#include "../Components/CubeMeshComponent.hpp"

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
		/*shader->SetVec3("viewPos", camera->Position);
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
    	shader->SetFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f))); */

		// create transformations
		glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
		glm::mat4 projection = glm::mat4(1.0f);
		projection = glm::perspective(glm::radians(30.0f), (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 100.0f);
		//projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
		cameraView = camera->GetViewMatrix();
		// pass transformation matrices to the shader
		shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
		shader->SetMat4("view", cameraView);

		// render boxes
		glBindVertexArray(VAO);

		auto view = registry.view<const TransformComponent, ColorComponent, CubeMeshComponent>();

		for(auto [entity, transform, color, cube]: view.each())
		{
			if(transform.active == true)
			{
				// material properties
				//shader->SetVec3("material.ambient", 1.0f, 0.5f, 0.31f);
				//shader->SetVec3("material.diffuse", color.color);
				//shader->SetFloat("material.shininess", 32.0f);

				glm::mat4 model = glm::mat4(1.0f);
				model = glm::translate(model, transform.position);
				model = glm::rotate(model, transform.rotation.x, glm::vec3(1,0,0));
				model = glm::rotate(model, transform.rotation.y, glm::vec3(0,1,0));
				model = glm::rotate(model, transform.rotation.z, glm::vec3(0,0,1));
				model = glm::scale(model, transform.scale);
				shader->SetMat4("model", model);
				shader->SetVec4("fColor", color.color);

				glDrawArrays(GL_TRIANGLES, 0, 36);
			}
		}

		shader->UnUse();
	}

private:
};