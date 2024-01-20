#pragma once
#include <algorithm>
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
#include "../Components/DirectionalLightComponent.hpp"

#include <Canis/Asset.hpp>
#include <Canis/DataStructure/List.hpp>
#include <Canis/Time.hpp>

namespace Canis
{
	struct RenderEnttRapper {
		entt::entity e;
		float value;
	};

	class RenderMeshWithShadowSystem : public System
	{
	private:
		std::vector<RenderEnttRapper> sortingEntities = {};
		RenderEnttRapper* sortingEntitiesList = nullptr;
		high_resolution_clock::time_point startTime;
    	high_resolution_clock::time_point endTime;

	public:
		Canis::Shader *shadow_mapping_depth_shader;
		Canis::Shader *shadow_mapping_depth_instance_shader;
		Canis::Shader *depthMapShader;
		Canis::Shader *shadow_mapping_shader;
		Canis::Shader *blurShader;
		Canis::Shader *screenSpaceCopyShader;
		Canis::Shader *bloomFinalShader;

		Canis::GLTexture *diffuseColorPaletteTexture;
		Canis::GLTexture *specularColorPaletteTexture;
		Canis::GLTexture *emissionColorPaletteTexture;

		int entities_rendered = 0;
		glm::vec3 lightPos = glm::vec3(0.0f, 30.0f, 30.0f);
		glm::vec3 lightLookAt = glm::vec3(12.0f,0.0f,15.0f);
		float nearPlane = 1.0f;
		float farPlane = 40.0f;
		unsigned int quadVAO = 0;
		unsigned int quadVBO;
		const unsigned int SHADOW_WIDTH = 1024*4, SHADOW_HEIGHT = 1024*4;
		unsigned int shadowMapFBO = 0;
		unsigned int shadowMap;
		unsigned int depthMapFBO = 0;
		unsigned int depthMap;
		unsigned int screenSpaceFBO;
		unsigned int screenSpace;
		unsigned int hdrFBO;
		unsigned int colorBuffers[2];
		unsigned int rboDepth;
		int skyboxAssetId = 0;
		bool horizontal = true, first_iteration = true;
		unsigned int pingpongFBO[2];
		unsigned int pingpongColorbuffers[2];
		float lightProjectionSize = 20.0f;
		glm::mat4 lightProjection, lightView;
		glm::mat4 lightSpaceMatrix;
		glm::mat4 depthProjection, depthView;
		glm::mat4 depthSpaceMatrix;
		float m_time = 0.0f;

		bool bloom = true;
		float exposure = 1.0f;

		enum SortBy{
			DISTANCE,
			HEIGHT
		};

		SortBy sortBy = SortBy::DISTANCE;

		RenderMeshWithShadowSystem() : System() {
			
		}

		~RenderMeshWithShadowSystem() {
			Canis::List::Free(&sortingEntitiesList);
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

		void renderQuad()
		{
			if (quadVAO == 0)
			{
				float quadVertices[] = {
					// positions        // texture Coords
					-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
					-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
					1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
					1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
				};
				// setup plane VAO
				glGenVertexArrays(1, &quadVAO);
				glGenBuffers(1, &quadVBO);
				glBindVertexArray(quadVAO);
				glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
				glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
				glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
			}
			glBindVertexArray(quadVAO);
			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
			glBindVertexArray(0);
		}

		void ShadowDepthPass(float deltaTime, entt::registry &registry)
		{
			glDisable(GL_CULL_FACE); 
			
			// render
        	// ------
        	//glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shadow_mapping_depth_shader->Use();

			shadow_mapping_depth_shader->SetInt("shadowMap", 0);

			// 1. render depth of scene to texture (from light's perspective)
			// --------------------------------------------------------------
			
			//lightProjection = glm::perspective(glm::radians(45.0f), (GLfloat)SHADOW_WIDTH / (GLfloat)SHADOW_HEIGHT, near_plane, far_plane); // note that if you use a perspective projection matrix you'll have to change the light position as the current light position isn't enough to reflect the whole scene
			lightProjection = glm::ortho(-lightProjectionSize, lightProjectionSize, -lightProjectionSize, lightProjectionSize, nearPlane, farPlane);
			lightView = glm::lookAt(lightPos, lightLookAt, glm::vec3(0.0, 1.0, 0.0));
			lightSpaceMatrix = lightProjection * lightView;
			// render scene from light's point of view
			shadow_mapping_depth_shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);

			glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
			glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
			glClear(GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			
			// render scene
			std::string modelKey = "model";

			unsigned int modelId = 0;
			unsigned int vao = 0;
			unsigned int size = 0;
			unsigned int ebo = 0;
			unsigned int instanceId = 0;

			InstanceMeshAsset* instanceMeshAsset = nullptr;

			for (RenderEnttRapper rer : sortingEntities)
			{
				const MeshComponent& mesh = registry.get<const MeshComponent>(rer.e);
				const TransformComponent& transform = registry.get<const TransformComponent>(rer.e);

				if (!mesh.useInstance)
				{
					if (mesh.id != modelId) {
						modelId = mesh.id;
						vao = AssetManager::Get<ModelAsset>(modelId)->vao;
						size = AssetManager::Get<ModelAsset>(modelId)->size;
						ebo = AssetManager::Get<ModelAsset>(modelId)->ebo;

						shadow_mapping_depth_shader->Use();

						shadow_mapping_depth_shader->SetInt("shadowMap", 0);

						shadow_mapping_depth_shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
					}

					shadow_mapping_depth_shader->SetMat4(modelKey, transform.modelMatrix);
				}
				else
				{
					instanceMeshAsset = AssetManager::Get<InstanceMeshAsset>(mesh.id);
					modelId = instanceMeshAsset->modelID;
					vao = AssetManager::Get<ModelAsset>(modelId)->vao;
					size = AssetManager::Get<ModelAsset>(modelId)->size;
					ebo = AssetManager::Get<ModelAsset>(modelId)->ebo;

					shadow_mapping_depth_instance_shader->Use();

					shadow_mapping_depth_instance_shader->SetInt("shadowMap", 0);

					shadow_mapping_depth_instance_shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
				}

				if (!mesh.castShadow)
					continue;

				glBindVertexArray(vao);

				if (!mesh.useInstance)
					glDrawElements(GL_TRIANGLES, AssetManager::Get<ModelAsset>(modelId)->indices.size(), GL_UNSIGNED_INT, 0);
				else
					glDrawElementsInstanced(GL_TRIANGLES,
						static_cast<unsigned int>(AssetManager::Get<ModelAsset>(modelId)->indices.size()),
						GL_UNSIGNED_INT,
						0,
						static_cast<unsigned int>(instanceMeshAsset->modelMatrices.size()));
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// reset viewport
			glViewport(0, 0, window->GetScreenWidth(), window->GetScreenHeight());
			//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shadow_mapping_depth_shader->UnUse();
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}

		void DepthPass(float deltaTime, entt::registry &registry)
		{
			glDisable(GL_CULL_FACE); 
			
			// render
        	// ------
        	//glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        	//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shadow_mapping_depth_shader->Use();

			shadow_mapping_depth_shader->SetInt("shadowMap", 0);

			// 1. render depth of scene to texture (from light's perspective)
			// --------------------------------------------------------------
			
			// create transformations
			glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
			glm::mat4 projection = glm::mat4(1.0f);
			glm::mat4 spaceMatrix = glm::mat4(1.0f);
			projection = glm::perspective(camera->FOV, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.05f, 100.0f);
			// projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
			cameraView = camera->GetViewMatrix();
			spaceMatrix = projection*cameraView;
			
			shadow_mapping_depth_shader->SetMat4("lightSpaceMatrix", spaceMatrix);

			glViewport(0, 0, window->GetScreenWidth(), window->GetScreenHeight());
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glClear(GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			
			// render scene
			std::string modelKey = "model";

			unsigned int modelId = 0;
			unsigned int vao = 0;
			unsigned int size = 0;
			unsigned int ebo = 0;
			unsigned int instanceId = 0;

			InstanceMeshAsset* instanceMeshAsset = nullptr;

			for (RenderEnttRapper rer : sortingEntities)
			{
				const MeshComponent& mesh = registry.get<const MeshComponent>(rer.e);
				const TransformComponent& transform = registry.get<const TransformComponent>(rer.e);

				if (!mesh.useInstance)
				{
					if (mesh.id != modelId) {
						modelId = mesh.id;
						vao = AssetManager::Get<ModelAsset>(modelId)->vao;
						size = AssetManager::Get<ModelAsset>(modelId)->size;
						ebo = AssetManager::Get<ModelAsset>(modelId)->ebo;

						shadow_mapping_depth_shader->Use();

						shadow_mapping_depth_shader->SetInt("shadowMap", 0);

						shadow_mapping_depth_shader->SetMat4("lightSpaceMatrix", spaceMatrix);
					}

					shadow_mapping_depth_shader->SetMat4(modelKey, transform.modelMatrix);
				}
				else
				{
					instanceMeshAsset = AssetManager::Get<InstanceMeshAsset>(mesh.id);
					modelId = instanceMeshAsset->modelID;
					vao = AssetManager::Get<ModelAsset>(modelId)->vao;
					size = AssetManager::Get<ModelAsset>(modelId)->size;
					ebo = AssetManager::Get<ModelAsset>(modelId)->ebo;

					shadow_mapping_depth_instance_shader->Use();

					shadow_mapping_depth_instance_shader->SetInt("shadowMap", 0);

					shadow_mapping_depth_instance_shader->SetMat4("lightSpaceMatrix", spaceMatrix);
				}

				if (!mesh.castDepth)
					continue;

				glBindVertexArray(vao);

				if (!mesh.useInstance)
					glDrawElements(GL_TRIANGLES, AssetManager::Get<ModelAsset>(modelId)->indices.size(), GL_UNSIGNED_INT, 0);
				else
					glDrawElementsInstanced(GL_TRIANGLES,
						static_cast<unsigned int>(AssetManager::Get<ModelAsset>(modelId)->indices.size()),
						GL_UNSIGNED_INT,
						0,
						static_cast<unsigned int>(instanceMeshAsset->modelMatrices.size()));
			}

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// reset viewport
			glViewport(0, 0, window->GetScreenWidth(), window->GetScreenHeight());
			//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			shadow_mapping_depth_shader->UnUse();
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);
		}

		void DrawMesh(float deltaTime, entt::registry &registry)
		{
			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			// reset viewport
			glDepthFunc(GL_LESS);

			entities_rendered = 0;    		

			// create transformations
			glm::mat4 cameraView = glm::mat4(1.0f); // make sure to initialize matrix to identity matrix first
			glm::mat4 projection = glm::mat4(1.0f);
			projection = glm::perspective(camera->FOV, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.05f, 100.0f);
			// projection = glm::ortho(0.1f, static_cast<float>(window->GetScreenWidth()), 100.0f, static_cast<float>(window->GetScreenHeight()));
			cameraView = camera->GetViewMatrix();
			
			std::string modelKey = "MODEL";
			std::string colorKey = "COLOR";
			std::string emissionKey = "EMISSION";
			std::string emissionUsingAlbedoIntesityKey = "EMISSIONUSINGALBEDOINTESITY";

			int modelId = -1;
			int materialId = -1;
			unsigned int vao = 0;
			unsigned int size = 0;
			unsigned int ebo = 0;
			unsigned int materialInfo = 0u;

			InstanceMeshAsset* instanceMeshAsset = nullptr;

			for (RenderEnttRapper rer : sortingEntities)
			{
				const TransformComponent& transform = registry.get<const TransformComponent>(rer.e);
				const ColorComponent& color = registry.get<const ColorComponent>(rer.e);
				const MeshComponent& mesh = registry.get<const MeshComponent>(rer.e);
				unsigned int textureCount = 0;

				if (!mesh.useInstance)
				{
					if (mesh.id != modelId) {
						modelId = mesh.id;
						vao = AssetManager::Get<ModelAsset>(modelId)->vao;
						size = AssetManager::Get<ModelAsset>(modelId)->size;
						ebo = AssetManager::Get<ModelAsset>(modelId)->ebo;
					}
				}
				else
				{
					instanceMeshAsset = AssetManager::Get<InstanceMeshAsset>(mesh.id);
					modelId = instanceMeshAsset->modelID;
					vao = AssetManager::Get<ModelAsset>(modelId)->vao;
					size = AssetManager::Get<ModelAsset>(modelId)->size;
					ebo = AssetManager::Get<ModelAsset>(modelId)->ebo;

					//glBindBuffer(GL_ARRAY_BUFFER, instanceMeshAsset->buffer);
				}

				materialId = mesh.material;
				MaterialAsset* material = AssetManager::Get<MaterialAsset>(materialId);
				materialInfo = material->info;

				shadow_mapping_shader = AssetManager::Get<ShaderAsset>(
					material->shaderId
				)->GetShader();

				if ( (materialInfo | MaterialInfo::HASSCREENTEXTURE) == materialInfo )
				{
					//glFinish();
					glDisable(GL_BLEND);

					glBindFramebuffer(GL_FRAMEBUFFER, 0);

					glFlush();

					glBindFramebuffer(GL_FRAMEBUFFER, screenSpaceFBO);
					glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
					screenSpaceCopyShader->Use();

					glActiveTexture(GL_TEXTURE0+textureCount);
					screenSpaceCopyShader->SetInt("image", textureCount);
					textureCount++;

					glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
					
					renderQuad();
					screenSpaceCopyShader->UnUse();


					glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
				}
				else
				{
					glEnable(GL_BLEND);
				}

				// activate shader
				shadow_mapping_shader->Use();

				// directional light
				int numDirLights = 0;

				auto viewDirLight = registry.view<const Canis::TransformComponent, const Canis::DirectionalLightComponent>();

				for (auto [entity, transform, directionalLight] : viewDirLight.each())
				{
					if (transform.active)
					{
						numDirLights++;
						shadow_mapping_shader->SetVec3("dirLight.direction", glm::normalize(glm::eulerAngles(transform.rotation)));
						shadow_mapping_shader->SetVec3("dirLight.ambient", directionalLight.ambient);
						shadow_mapping_shader->SetVec3("dirLight.diffuse", directionalLight.diffuse);
						shadow_mapping_shader->SetVec3("dirLight.specular", directionalLight.specular);
					}
					break;
				}

				shadow_mapping_shader->SetInt("numDirLights", numDirLights);

				shadow_mapping_shader->SetFloat("material.shininess", 32.0f);

				shadow_mapping_shader->SetInt("hdr", true);
				shadow_mapping_shader->SetFloat("exposure", 3.1f);

				shadow_mapping_shader->SetVec3("viewPos", camera->Position);
				shadow_mapping_shader->SetVec3("lightPos", lightPos);
				shadow_mapping_shader->SetMat4("projection", projection); // note: currently we set the projection matrix each frame, but since the projection matrix rarely changes it's often best practice to set it outside the main loop only once.
				shadow_mapping_shader->SetMat4("view", cameraView);
				shadow_mapping_shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
				shadow_mapping_shader->SetFloat("NEARPLANE", camera->nearPlane);
				shadow_mapping_shader->SetFloat("FARPLANE", camera->farPlane);

				diffuseColorPaletteTexture = AssetManager::Get<Canis::TextureAsset>(material->albedoId)->GetPointerToTexture();
				specularColorPaletteTexture = AssetManager::Get<Canis::TextureAsset>(material->specularId)->GetPointerToTexture();
				emissionColorPaletteTexture = AssetManager::Get<Canis::TextureAsset>(material->emissionId)->GetPointerToTexture();

				glActiveTexture(GL_TEXTURE0+textureCount);
				glBindTexture(GL_TEXTURE_2D, diffuseColorPaletteTexture->id);
				shadow_mapping_shader->SetInt("material.diffuse", textureCount);
				textureCount++;

				glActiveTexture(GL_TEXTURE0+textureCount);
				glBindTexture(GL_TEXTURE_2D, specularColorPaletteTexture->id);
				shadow_mapping_shader->SetInt("material.specular", textureCount);
				textureCount++;

				glActiveTexture(GL_TEXTURE0+textureCount);
				glBindTexture(GL_TEXTURE_2D, emissionColorPaletteTexture->id);
				shadow_mapping_shader->SetInt("material.emission", textureCount);
				textureCount++;

				glActiveTexture(GL_TEXTURE0+textureCount);
				glBindTexture(GL_TEXTURE_2D, shadowMap);
				shadow_mapping_shader->SetInt("shadowMap", textureCount);
				textureCount++;

				glActiveTexture(GL_TEXTURE0+textureCount);
				glBindTexture(GL_TEXTURE_2D, screenSpace);
				shadow_mapping_shader->SetInt("SCREENTEXTURE", textureCount);
				textureCount++;

				if ( (material->info | MaterialInfo::HASDEPTH) == material->info )
				{
					glActiveTexture(GL_TEXTURE0+textureCount);
					glBindTexture(GL_TEXTURE_2D, depthMap);
					shadow_mapping_shader->SetInt("DEPTHTEXTURE", textureCount);
					textureCount++;
				}

				for(int i = 0; i < material->texNames.size(); i++)
				{
					glActiveTexture(GL_TEXTURE0+textureCount);
					glBindTexture(GL_TEXTURE_2D, material->texId[i]);
					shadow_mapping_shader->SetInt(material->texNames[i].c_str(), textureCount);
					textureCount++;
				}

				shadow_mapping_shader->SetFloat("TIME", m_time);

				glBindVertexArray(vao);

				shadow_mapping_shader->SetMat4(modelKey, transform.modelMatrix);

				shadow_mapping_shader->SetVec4(colorKey, color.color);
				shadow_mapping_shader->SetBool("USEEMISSION", material->info & MaterialInfo::HASEMISSION);
				shadow_mapping_shader->SetVec3(emissionKey, color.emission);
				shadow_mapping_shader->SetFloat(emissionUsingAlbedoIntesityKey, color.emissionUsingAlbedoIntesity);

				//glDrawArrays(GL_TRIANGLES, 0, size);
				if (!mesh.useInstance)
					glDrawElements(GL_TRIANGLES, AssetManager::Get<ModelAsset>(modelId)->indices.size(), GL_UNSIGNED_INT, 0);
				else
					glDrawElementsInstanced(GL_TRIANGLES,
						static_cast<unsigned int>(AssetManager::Get<ModelAsset>(modelId)->indices.size()),
						GL_UNSIGNED_INT,
						0,
						static_cast<unsigned int>(instanceMeshAsset->modelMatrices.size()));

				entities_rendered++;
			}

			// draw skybox as last
            glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
            AssetManager::Get<SkyboxAsset>(skyboxAssetId)->GetShader()->Use();
            AssetManager::Get<SkyboxAsset>(skyboxAssetId)->GetShader()->SetMat4("view", glm::mat4(glm::mat3(cameraView)));
            AssetManager::Get<SkyboxAsset>(skyboxAssetId)->GetShader()->SetMat4("projection", projection);
            // skybox cube
            glBindVertexArray(AssetManager::Get<SkyboxAsset>(skyboxAssetId)->GetVAO());
            //Canis::Log(std::to_string(AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetVAO()));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, AssetManager::Get<SkyboxAsset>(skyboxAssetId)->GetTexture());
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glDepthFunc(GL_LESS); // set depth function back to default
            AssetManager::Get<SkyboxAsset>(skyboxAssetId)->GetShader()->UnUse();
			
			glBindVertexArray(0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			shadow_mapping_shader->UnUse();
		}
		
		void Blur(float deltaTime, entt::registry &registry)
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			// 2. blur bright fragments with two-pass Gaussian Blur 
			// --------------------------------------------------
			horizontal = true;
			first_iteration = true;
			unsigned int amount = 4;
			blurShader->Use();
			blurShader->SetInt("image", 0);
			for (unsigned int i = 0; i < amount; i++)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
				blurShader->SetInt("horizontal", horizontal);
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);  // bind texture of other framebuffer (or scene if first iteration)
				renderQuad();
				horizontal = !horizontal;
				if (first_iteration)
					first_iteration = false;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void BloomCombine(float deltaTime, entt::registry &registry)
		{
			// 3. now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
			// --------------------------------------------------------------------------------------------------------------------------
			glClear(GL_COLOR_BUFFER_BIT);
			bloomFinalShader->Use();
			bloomFinalShader->SetInt("scene", 0);
			bloomFinalShader->SetInt("bloomBlur", 1);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
			bloomFinalShader->SetInt("bloom", bloom);
			bloomFinalShader->SetFloat("exposure", exposure);
			renderQuad();
		}

		void ConfigureBuffers() {
			// configure depth map FBO
			// -----------------------
			glGenFramebuffers(1, &shadowMapFBO);
			// create depth texture
			glGenTextures(1, &shadowMap);
			glBindTexture(GL_TEXTURE_2D, shadowMap);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
			// attach depth texture as FBO's depth buffer
			glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowMap, 0);
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// configure depth map FBO
			// -----------------------
			glGenFramebuffers(1, &depthMapFBO);
			// create depth texture
			glGenTextures(1, &depthMap);
			glBindTexture(GL_TEXTURE_2D, depthMap);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, window->GetScreenWidth(), window->GetScreenHeight(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
			// attach depth texture as FBO's depth buffer
			glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// configure (floating point) framebuffers
			// ---------------------------------------
			glGenFramebuffers(1, &hdrFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
			// create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
			glGenTextures(2, colorBuffers);
			for (unsigned int i = 0; i < 2; i++)
			{
				glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window->GetScreenWidth(), window->GetScreenHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
				// attach texture to framebuffer
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
			}
			// create and attach depth buffer (renderbuffer)
			glGenRenderbuffers(1, &rboDepth);
			glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, window->GetScreenWidth(), window->GetScreenHeight());
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
			// tell OpenGL which color attachments we'll use (of this framebuffer) for rendering 
			unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
			glDrawBuffers(2, attachments);
			// finally check if framebuffer is complete
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cout << "Framebuffer not complete!" << std::endl;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			// ping-pong-framebuffer for blurring
			glGenFramebuffers(2, pingpongFBO);
			glGenTextures(2, pingpongColorbuffers);
			for (unsigned int i = 0; i < 2; i++)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
				glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window->GetScreenWidth(), window->GetScreenHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);
				// also check if framebuffers are complete (no need for depth buffer)
				if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
					std::cout << "Framebuffer not complete!" << std::endl;
			}

			glGenFramebuffers(1, &screenSpaceFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, screenSpaceFBO);
			// create 2 floating point color buffers (1 for normal rendering, other for brightness threshold values)
			glGenTextures(1, &screenSpace);
			glBindTexture(GL_TEXTURE_2D, screenSpace);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window->GetScreenWidth(), window->GetScreenHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);  // we clamp to the edge as the blur filter would otherwise sample repeated texture values!
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
			
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenSpace, 0);
		}

		void Create() {
			skyboxAssetId = AssetManager::LoadSkybox("assets/textures/lowpoly-skybox/");
			//skyboxAssetId = AssetManager::LoadSkybox("assets/textures/space-nebulas-skybox/");

			if (shadowMapFBO == 0)
				ConfigureBuffers();

			Canis::List::Init(&sortingEntitiesList,100,sizeof(RenderEnttRapper));

			int id = AssetManager::LoadShader("assets/shaders/shadow_mapping_depth");
            shadow_mapping_depth_shader = AssetManager::Get<Canis::ShaderAsset>(id)->GetShader();
            
            if(!shadow_mapping_depth_shader->IsLinked())
            {
                shadow_mapping_depth_shader->Link();
            }

			id = AssetManager::LoadShader("assets/shaders/shadow_mapping_depth_instance");
            shadow_mapping_depth_instance_shader = AssetManager::Get<Canis::ShaderAsset>(id)->GetShader();
            
            if(!shadow_mapping_depth_instance_shader->IsLinked())
            {
                shadow_mapping_depth_instance_shader->Link();
            }

			id = AssetManager::LoadShader("assets/shaders/blur");
            blurShader = AssetManager::Get<Canis::ShaderAsset>(id)->GetShader();
            
            if(!blurShader->IsLinked())
            {
                blurShader->Link();
            }
			
			id = AssetManager::LoadShader("assets/shaders/bloom_final");
            bloomFinalShader = AssetManager::Get<Canis::ShaderAsset>(id)->GetShader();
            
            if(!bloomFinalShader->IsLinked())
            {
                bloomFinalShader->Link();
            }

			id = AssetManager::LoadShader("assets/shaders/screen_space_copy");
			screenSpaceCopyShader = AssetManager::Get<Canis::ShaderAsset>(id)->GetShader();
			if (!screenSpaceCopyShader->IsLinked())
			{
				screenSpaceCopyShader->Link();
			}
		}
    	void Ready() {}
    	void Update(entt::registry &_registry, float _deltaTime)
		{
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_ALPHA);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthFunc(GL_LESS);
			glEnable(GL_CULL_FACE);

			m_time += _deltaTime;

			sortingEntities.clear();
			Canis::List::Clear(&sortingEntitiesList);

			Frustum camFrustum = CreateFrustumFromCamera(camera, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), camera->FOV, camera->nearPlane, camera->farPlane);

			auto view = _registry.view<TransformComponent, const MeshComponent, const SphereColliderComponent>();

			for (auto [entity, transform, mesh, sphere] : view.each())
			{
				if (!transform.active)
					continue;

				if (transform.isDirty)
					UpdateModelMatrix(transform);

				if (!isOnFrustum(camFrustum, transform, transform.modelMatrix, sphere))
					continue;

				RenderEnttRapper rer = {};
				rer.e = entity;
				if (sortBy == SortBy::DISTANCE)
					rer.value = glm::distance(GetGlobalPosition(transform), camera->Position);
				if (sortBy == SortBy::HEIGHT)
					rer.value = transform.position.y;

				sortingEntities.push_back(rer);
				//Canis::List::Add(&sortingEntitiesList, &rer);
			}

			//startTime = high_resolution_clock::now();
			if (sortBy == SortBy::DISTANCE)
				std::stable_sort(sortingEntities.begin(), sortingEntities.end(), [](const RenderEnttRapper& a, const RenderEnttRapper& b){ return (a.value > b.value); });
			if (sortBy == SortBy::HEIGHT)
				std::stable_sort(sortingEntities.begin(), sortingEntities.end(), [](const RenderEnttRapper& a, const RenderEnttRapper& b){ return (a.value < b.value); });
			//endTime = high_resolution_clock::now();
			//std::cout << "Vector MergeSort : " << std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count() / 1000000000.0f << std::endl;

			//startTime = high_resolution_clock::now();
			//Canis::List::MergeSort(&sortingEntitiesList, Canis::DoubleAscending);
			//endTime = high_resolution_clock::now();
			//std::cout << "MergeSort : " << std::chrono::duration_cast<std::chrono::nanoseconds>(endTime - startTime).count() / 1000000000.0f << std::endl;

			ShadowDepthPass(_deltaTime, _registry);

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_ALPHA);
			
			glDepthFunc(GL_LESS);
			glEnable(GL_CULL_FACE);

			DepthPass(_deltaTime, _registry);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glEnable(GL_BLEND);
			DrawMesh(_deltaTime, _registry);
			glDisable(GL_BLEND);
			Blur(_deltaTime, _registry);
			BloomCombine(_deltaTime, _registry);

			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_ALPHA);
			glDisable(GL_BLEND);
		}
	};

	bool DecodeRenderMeshWithShadowSystem(const std::string &_name, Canis::Scene *_scene)
	{
        if (_name == "Canis::RenderMeshWithShadowSystem")
        {
            _scene->CreateRenderSystem<RenderMeshWithShadowSystem>();
            return true;
        }
        return false;
    }
} // end of Canis namespace