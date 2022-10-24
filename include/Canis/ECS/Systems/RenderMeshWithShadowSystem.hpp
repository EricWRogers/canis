#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include "../../Math.hpp"
#include "../../Shader.hpp"
#include "../../Camera.hpp"
#include "../../Window.hpp"
#include "../../IOManager.hpp"
#include "../../External/entt.hpp"

#include "../Components/TransformComponent.hpp"
#include "../Components/ColorComponent.hpp"
#include "../Components/MeshComponent.hpp"
#include "../Components/SphereColliderComponent.hpp"

namespace Canis
{
	class RenderMeshWithShadowSystem
	{
	public:
		Canis::Shader *shadow_mapping_depth_shader;
		Canis::Shader *shadow_mapping_shader;
		Canis::Camera *camera;
		Canis::Window *window;
		Canis::GLTexture *diffuseColorPaletteTexture;
		Canis::GLTexture *specularColorPaletteTexture;

		int entities_rendered = 0;
		glm::vec3 lightPos = glm::vec3(0.0f, 20.0f, 0.0f);
		glm::vec3 lightLookAt = glm::vec3(12.0f,0.0f,15.0f);
		float nearPlane = 1.0f;
		float farPlane = 40.0f;
		const unsigned int SHADOW_WIDTH = 1024*4, SHADOW_HEIGHT = 1024*4;
		unsigned int depthMapFBO;
		unsigned int depthMap;
		float lightProjectionSize = 20.0f;
		glm::mat4 lightProjection, lightView;
		glm::mat4 lightSpaceMatrix;

		RenderMeshWithShadowSystem() {
			// configure depth map FBO
			// -----------------------
			glGenFramebuffers(1, &depthMapFBO);
			// create depth texture
			glGenTextures(1, &depthMap);
			glBindTexture(GL_TEXTURE_2D, depthMap);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
			// attach depth texture as FBO's depth buffer
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		struct Plan
		{
			glm::vec3 normal = {0.f, 1.f, 0.f}; // unit vector
			float distance = 0.f;				// Distance with origin

			Plan() = default;

			Plan(const glm::vec3 &p1, const glm::vec3 &norm)
				: normal(glm::normalize(norm)),
				  distance(glm::dot(normal, p1))
			{
			}

			float getSignedDistanceToPlan(const glm::vec3 &point) const
			{
				return glm::dot(normal, point) - distance;
			}
		};

		struct Frustum
		{
			Plan topFace;
			Plan bottomFace;

			Plan rightFace;
			Plan leftFace;

			Plan farFace;
			Plan nearFace;
		};

		Frustum CreateFrustumFromCamera(Canis::Camera *cam, float aspect, float fovY, float zNear, float zFar)
		{
			Frustum frustum;
			const float halfVSide = zFar * tanf(fovY * .5f);
			const float halfHSide = halfVSide * aspect;
			const glm::vec3 frontMultFar = zFar * cam->Front;

			frustum.nearFace = {cam->Position + zNear * cam->Front, cam->Front};
			frustum.farFace = {cam->Position + frontMultFar, -cam->Front};
			frustum.rightFace = {cam->Position, glm::cross(cam->Up, frontMultFar + cam->Right * halfHSide)};
			frustum.leftFace = {cam->Position, glm::cross(frontMultFar - cam->Right * halfHSide, cam->Up)};
			frustum.topFace = {cam->Position, glm::cross(cam->Right, frontMultFar - cam->Up * halfVSide)};
			frustum.bottomFace = {cam->Position, glm::cross(frontMultFar + cam->Up * halfVSide, cam->Right)};

			return frustum;
		}

		bool isOnOrForwardPlan(const Plan &plan, const SphereColliderComponent &sphere)
		{
			return plan.getSignedDistanceToPlan(sphere.center) > -sphere.radius;
		}

		bool isOnFrustum(const Frustum &camFrustum, const Canis::TransformComponent &transform, const glm::mat4 &modelMatrix, const SphereColliderComponent &sphere)
		{
			// Get global scale thanks to our transform
			const glm::vec3 globalScale = transform.scale;

			// Get our global center with process it with the global model matrix of our transform
			const glm::vec3 globalCenter{modelMatrix * glm::vec4(sphere.center, 1.0f)};

			// To wrap correctly our shape, we need the maximum scale scalar.
			const float maxScale = std::max(std::max(globalScale.x, globalScale.y), globalScale.z);
			// const float maxScale = std::max(std::max(transform.position.x, transform.position.y), transform.position.z);

			// Max scale is assuming for the diameter. So, we need the half to apply it to our radius
			// SphereColliderComponent globalSphere(transform.position + sphere.center, sphere.radius * (maxScale * 0.5f));
			SphereColliderComponent globalSphere = {};
			globalSphere.center = globalCenter;
			globalSphere.radius = sphere.radius * (maxScale * 0.5f);

			// Check Firstly the result that have the most chance to faillure to avoid to call all functions.
			return (isOnOrForwardPlan(camFrustum.leftFace, globalSphere) &&
				isOnOrForwardPlan(camFrustum.rightFace, globalSphere) &&
				isOnOrForwardPlan(camFrustum.farFace, globalSphere) &&
				isOnOrForwardPlan(camFrustum.nearFace, globalSphere) &&
				isOnOrForwardPlan(camFrustum.topFace, globalSphere) &&
				isOnOrForwardPlan(camFrustum.bottomFace, globalSphere));
		};

		void ShadowDepthPass(float deltaTime, entt::registry &registry)
		{
			glDisable(GL_CULL_FACE); 
			
			// render
        	// ------
        	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shadow_mapping_depth_shader->Use();

			shadow_mapping_depth_shader->SetInt("depthMap", 0);

			// 1. render depth of scene to texture (from light's perspective)
			// --------------------------------------------------------------
			
			//lightProjection = glm::perspective(glm::radians(45.0f), (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene
			lightProjection = glm::ortho(-lightProjectionSize, lightProjectionSize, -lightProjectionSize, lightProjectionSize, nearPlane, farPlane);
			lightView = glm::lookAt(lightPos, lightLookAt, glm::vec3(0.0, 1.0, 0.0));
			lightSpaceMatrix = lightProjection * lightView;
			// render scene from light's point of view
			shadow_mapping_depth_shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);

			glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glClear(GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			
			// render scene
			Frustum camFrustum = CreateFrustumFromCamera(camera, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), camera->FOV, 0.1f, 100.0f);

			auto view = registry.view<Canis::TransformComponent, ColorComponent, MeshComponent, SphereColliderComponent>();

			for (auto [entity, transform, color, mesh, sphere] : view.each())
			{
				if (!transform.active)
					continue;
				
				if (!mesh.castShadow)
					continue;

				glm::mat4 modelMatrix = Canis::GetModelMatrix(transform);

				if (!isOnFrustum(camFrustum, transform, modelMatrix, sphere))
					continue;

				glBindVertexArray(mesh.vao);

				shadow_mapping_depth_shader->SetMat4("model", modelMatrix);

				glDrawArrays(GL_TRIANGLES, 0, mesh.size);
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// reset viewport
			glViewport(0, 0, window->GetScreenWidth(), window->GetScreenHeight());
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shadow_mapping_depth_shader->UnUse();
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}

		void DrawMesh(float deltaTime, entt::registry &registry)
		{
			// reset viewport
			glViewport(0, 0, window->GetScreenWidth(), window->GetScreenHeight());
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			entities_rendered = 0;
			// activate shader
			shadow_mapping_shader->Use();

			// directional light
			int numDirLights = 0;

			auto viewDirLight = registry.view<Canis::TransformComponent, Canis::DirectionalLightComponent>();

			for (auto [entity, transform, directionalLight] : viewDirLight.each())
			{
				if (transform.active)
				{
					numDirLights++;
					shadow_mapping_shader->SetVec3("dirLight.direction", transform.rotation);
					shadow_mapping_shader->SetVec3("dirLight.ambient", directionalLight.ambient);
					shadow_mapping_shader->SetVec3("dirLight.diffuse", directionalLight.diffuse);
					shadow_mapping_shader->SetVec3("dirLight.specular", directionalLight.specular);
				}
				break;
			}

			shadow_mapping_shader->SetInt("numDirLights", numDirLights);

			// material
			shadow_mapping_shader->SetInt("material.diffuse", 0);
			shadow_mapping_shader->SetInt("material.specular", 1);
			shadow_mapping_shader->SetFloat("material.shininess", 32.0f);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, diffuseColorPaletteTexture->id);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, depthMap);

			shadow_mapping_shader->SetInt("diffuseTexture", 0);
    		shadow_mapping_shader->SetInt("shadowMap", 1);

			shadow_mapping_shader->SetVec3("viewPos", camera->Position);
			shadow_mapping_shader->SetVec3("lightPos", lightPos);

			// create transformations
			glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
			glm::mat4 projection = glm::mat4(1.0f);
			projection = glm::perspective(camera->FOV, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.05f, 100.0f);
			// projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
			cameraView = camera->GetViewMatrix();
			// pass transformation matrices to the shader
			shadow_mapping_shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
			shadow_mapping_shader->SetMat4("view", cameraView);

			Frustum camFrustum = CreateFrustumFromCamera(camera, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), camera->FOV, 0.1f, 100.0f);

			shadow_mapping_shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);

			auto view = registry.view<Canis::TransformComponent, ColorComponent, MeshComponent, SphereColliderComponent>();

			for (auto [entity, transform, color, mesh, sphere] : view.each())
			{
				if (!transform.active)
					continue;

				glm::mat4 modelMatrix = Canis::GetModelMatrix(transform);

				if (!isOnFrustum(camFrustum, transform, modelMatrix, sphere))
					continue;

				glBindVertexArray(mesh.vao);

				shadow_mapping_shader->SetMat4("model", modelMatrix);
				shadow_mapping_shader->SetVec4("color", color.color);

				glDrawArrays(GL_TRIANGLES, 0, mesh.size);

				entities_rendered++;
			}

			glBindVertexArray(0);

			shadow_mapping_shader->UnUse();
		}		
		
		void UpdateComponents(float deltaTime, entt::registry &registry)
		{
			ShadowDepthPass(deltaTime, registry);
			DrawMesh(deltaTime, registry);
		}

	private:
	};
} // end of Canis namespace