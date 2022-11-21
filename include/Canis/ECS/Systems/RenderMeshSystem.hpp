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

#include <Canis/ECS/Systems/System.hpp>

#include "../Components/TransformComponent.hpp"
#include "../Components/ColorComponent.hpp"
#include "../Components/MeshComponent.hpp"
#include "../Components/SphereColliderComponent.hpp"
#include "../Components/SpotLightComponent.hpp"
#include "../Components/PointLightComponent.hpp"
#include "../Components/DirectionalLightComponent.hpp"

namespace Canis
{
	class RenderMeshSystem : public System
	{
	public:
		Canis::Shader *shader;
		Canis::GLTexture *diffuseColorPaletteTexture;
		Canis::GLTexture *specularColorPaletteTexture;

		int entities_rendered = 0;

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

		void UpdateComponents(float deltaTime, entt::registry &registry)
		{
			entities_rendered = 0;
			// activate shader
			shader->Use();

			shader->SetVec3("viewPos", camera->Position);

			// directional light
			int numDirLights = 0;

			auto viewDirLight = registry.view<const Canis::TransformComponent, Canis::DirectionalLightComponent>();

			for (auto [entity, transform, directionalLight] : viewDirLight.each())
			{
				if (transform.active)
				{
					numDirLights++;
					shader->SetVec3("dirLight.direction", transform.rotation);
					shader->SetVec3("dirLight.ambient", directionalLight.ambient);
					shader->SetVec3("dirLight.diffuse", directionalLight.diffuse);
					shader->SetVec3("dirLight.specular", directionalLight.specular);
				}
				break;
			}

			shader->SetInt("numDirLights", numDirLights);

			// point light 1
			int numPointLights = 0;
			int maxPointLights = 4;
			
			auto viewPointLight = registry.view<const Canis::TransformComponent, Canis::PointLightComponent>();

			for (auto [entity, transform, pointLight] : viewPointLight.each())
			{
				if (numPointLights > maxPointLights)
					break;
				
				if (transform.active)
				{
					shader->SetVec3("pointLights["+std::to_string(numPointLights)+"].position", transform.position);
					shader->SetVec3("pointLights["+std::to_string(numPointLights)+"].ambient", pointLight.ambient);
					shader->SetVec3("pointLights["+std::to_string(numPointLights)+"].diffuse", pointLight.diffuse);
					shader->SetVec3("pointLights["+std::to_string(numPointLights)+"].specular", pointLight.specular);
					shader->SetFloat("pointLights["+std::to_string(numPointLights)+"].constant", pointLight.constant);
					shader->SetFloat("pointLights["+std::to_string(numPointLights)+"].linear", pointLight.linear);
					shader->SetFloat("pointLights["+std::to_string(numPointLights)+"].quadratic", pointLight.quadratic);
					numPointLights++;
				}
			}

			shader->SetInt("numPointLights", numPointLights);
			// spotLight
			int numSpotLights = 0;
			int maxSpotLights = 4;

			auto viewSpotLight = registry.view<const Canis::TransformComponent, Canis::SpotLightComponent>();
			
			for (auto [entity, transform, spotLight] : viewSpotLight.each())
			{
				if (numSpotLights > maxSpotLights)
					break;
				
				if (transform.active)
				{
					shader->SetVec3("spotLight["+std::to_string(numSpotLights)+"].position", transform.position);
					shader->SetVec3("spotLight["+std::to_string(numSpotLights)+"].direction", transform.rotation);
					shader->SetFloat("spotLight["+std::to_string(numSpotLights)+"].cutOff", spotLight.cutOff);
					shader->SetFloat("spotLight["+std::to_string(numSpotLights)+"].outerCutOff", spotLight.outerCutOff);
					shader->SetFloat("spotLight["+std::to_string(numSpotLights)+"].constant", spotLight.constant);
					shader->SetFloat("spotLight["+std::to_string(numSpotLights)+"].linear", spotLight.linear);
					shader->SetFloat("spotLight["+std::to_string(numSpotLights)+"].quadratic", spotLight.quadratic);
					shader->SetVec3("spotLight["+std::to_string(numSpotLights)+"].ambient", spotLight.ambient);
					shader->SetVec3("spotLight["+std::to_string(numSpotLights)+"].diffuse", spotLight.diffuse);
					shader->SetVec3("spotLight["+std::to_string(numSpotLights)+"].specular", spotLight.specular);
					
					numSpotLights++;
				}
			}

			shader->SetInt("numSpotLights", numSpotLights);

			// create transformations
			glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
			glm::mat4 projection = glm::mat4(1.0f);
			projection = glm::perspective(camera->FOV, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.05f, 1000.0f);
			// projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
			cameraView = camera->GetViewMatrix();
			// pass transformation matrices to the shader
			shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
			shader->SetMat4("view", cameraView);

			Frustum camFrustum = CreateFrustumFromCamera(camera, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), camera->FOV, 0.1f, 1000.0f);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, diffuseColorPaletteTexture->id);

			shader->SetInt("material.diffuse", 0);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, specularColorPaletteTexture->id);

			shader->SetInt("material.specular", 1);

			shader->SetFloat("material.shininess", 32.0f);

			auto view = registry.view<Canis::TransformComponent, ColorComponent, MeshComponent, SphereColliderComponent>();

			for (auto [entity, transform, color, mesh, sphere] : view.each())
			{
				if (!transform.active)
					continue;

				glm::mat4 modelMatrix = Canis::GetModelMatrix(transform);

				if (!isOnFrustum(camFrustum, transform, modelMatrix, sphere))
					continue;

				glBindVertexArray(mesh.vao);

				shader->SetMat4("model", modelMatrix);
				shader->SetVec4("color", color.color);

				glDrawArrays(GL_TRIANGLES, 0, mesh.size);

				entities_rendered++;
			}

			glBindVertexArray(0);

			shader->UnUse();
		}

	private:
	};
} // end of Canis namespace