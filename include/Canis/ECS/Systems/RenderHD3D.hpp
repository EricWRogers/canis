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

#include <Canis/DataStructure/List.hpp>
#include <Canis/Time.hpp>

namespace Canis
{
	class RenderHD3D : public System
	{
		struct RenderEnttRapper {
			entt::entity e;
			float distance;
		};
	private:
		std::vector<RenderEnttRapper> sortingEntities = {};
		RenderEnttRapper* sortingEntitiesList = nullptr;
		high_resolution_clock::time_point startTime;
    	high_resolution_clock::time_point endTime;

	public:
		Canis::Shader *hdrShader;
		Canis::Shader *lightingShader;
		Canis::GLTexture *diffuseColorPaletteTexture;
		Canis::GLTexture *specularColorPaletteTexture;
		Canis::GLTexture *emissionColorPaletteTexture;

		int entities_rendered = 0;
		glm::vec3 lightPos = glm::vec3(0.0f, 20.0f, 0.0f);
		glm::vec3 lightLookAt = glm::vec3(12.0f,0.0f,15.0f);
		float nearPlane = 1.0f;
		float farPlane = 40.0f;
		glm::mat4 lightProjection, lightView;
		glm::mat4 lightSpaceMatrix;

		bool hdr = true;
		float exposure = 1.0f;

		unsigned int hdrFBO;
		unsigned int colorBuffer;
		unsigned int rboDepth;
		unsigned int quadVAO = 0;
		unsigned int quadVBO;

		std::vector<glm::vec3> lightPositions;
		std::vector<glm::vec3> lightColors;

		RenderHD3D() : System() {
			

		}

		~RenderHD3D() {
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

		unsigned int cubeVAO = 0;
		unsigned int cubeVBO = 0;
		void renderCube()
		{
			// initialize (if necessary)
			if (cubeVAO == 0)
			{
				float vertices[] = {
					// back face
					-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
					1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
					1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
					1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
					-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
					-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
					// front face
					-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
					1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
					1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
					1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
					-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
					-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
					// left face
					-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
					-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
					-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
					-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
					-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
					-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
					// right face
					1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
					1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
					1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
					1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
					1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
					1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
					// bottom face
					-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
					1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
					1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
					1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
					-1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
					-1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
					// top face
					-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
					1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
					1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
					1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
					-1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
					-1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
				};
				glGenVertexArrays(1, &cubeVAO);
				glGenBuffers(1, &cubeVBO);
				// fill buffer
				glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
				glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
				// link vertex attributes
				glBindVertexArray(cubeVAO);
				glEnableVertexAttribArray(0);
				glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
				glEnableVertexAttribArray(1);
				glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
				glEnableVertexAttribArray(2);
				glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
				glBindBuffer(GL_ARRAY_BUFFER, 0);
				glBindVertexArray(0);
			}
			// render Cube
			glBindVertexArray(cubeVAO);
			glDrawArrays(GL_TRIANGLES, 0, 36);
			glBindVertexArray(0);
		}


		void LightingPass(float deltaTime, entt::registry &registry)
		{
			lightingShader->Use();
			lightingShader->SetInt("diffuseTexture", 0);

			// render
        	// ------
        	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        	// 1. render scene into floating point framebuffer
        	// -----------------------------------------------
        	glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom), window->GetScreenWidth()/(float)window->GetScreenHeight(), camera->nearPlane, camera->farPlane);
            glm::mat4 view = camera->GetViewMatrix();
            lightingShader->SetMat4("projection", projection);
            lightingShader->SetMat4("view", view);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, diffuseColorPaletteTexture->id);
            // set lighting uniforms
            for (unsigned int i = 0; i < lightPositions.size(); i++)
            {
                lightingShader->SetVec3("lights[" + std::to_string(i) + "].Position", lightPositions[i]);
                lightingShader->SetVec3("lights[" + std::to_string(i) + "].Color", lightColors[i]);
            }
            lightingShader->SetVec3("viewPos", camera->Position);
            // render tunnel
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, 25.0));
            model = glm::scale(model, glm::vec3(2.5f, 2.5f, 27.5f));
            lightingShader->SetMat4("model", model);
            lightingShader->SetInt("inverse_normals", true);
            renderCube();
        	glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		void HDRPass(float deltaTime, entt::registry &registry)
		{
			hdrShader->Use();
			hdrShader->SetInt("hdrBuffer", 0);

			// 2. now render floating point color buffer to 2D quad and tonemap HDR colors to default framebuffer's (clamped) color range
			// --------------------------------------------------------------------------------------------------------------------------
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, colorBuffer);
			hdrShader->SetInt("hdr", hdr);
			hdrShader->SetFloat("exposure", exposure);
			
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
		
		/*void DrawMesh(float deltaTime, entt::registry &registry)
		{
			// reset viewport
			glDepthFunc(GL_LESS);

			entities_rendered = 0;
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
			shadow_mapping_shader->SetInt("material.emission", 2);
			shadow_mapping_shader->SetFloat("material.shininess", 32.0f);

			shadow_mapping_shader->SetInt("hdr", true);
        	shadow_mapping_shader->SetFloat("exposure", 3.1f);

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, diffuseColorPaletteTexture->id);

			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, specularColorPaletteTexture->id);

			glActiveTexture(GL_TEXTURE2);
			glBindTexture(GL_TEXTURE_2D, emissionColorPaletteTexture->id);

			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_2D, depthMap);

    		shadow_mapping_shader->SetInt("shadowMap", 3);

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

			shadow_mapping_shader->SetMat4("lightSpaceMatrix", lightSpaceMatrix);
			
			std::string modelKey = "model";
			std::string colorKey = "color";

			for (RenderEnttRapper rer : sortingEntities)
			{
				const TransformComponent& transform = registry.get<const TransformComponent>(rer.e);
				const ColorComponent& color = registry.get<const ColorComponent>(rer.e);
				const MeshComponent& mesh = registry.get<const MeshComponent>(rer.e);

				glBindVertexArray(mesh.vao);

				shadow_mapping_shader->SetMat4(modelKey, transform.modelMatrix);
				shadow_mapping_shader->SetVec4(colorKey, color.color);

				glDrawArrays(GL_TRIANGLES, 0, mesh.size);

				entities_rendered++;
			}

			glBindVertexArray(0);

			shadow_mapping_shader->UnUse();
		}*/
		
		void Create() {
			Canis::List::Init(&sortingEntitiesList,100,sizeof(RenderEnttRapper));

			glEnable(GL_DEPTH_TEST);
			glGenFramebuffers(1, &hdrFBO);
			// create floating point color buffer
			glGenTextures(1, &colorBuffer);
			glBindTexture(GL_TEXTURE_2D, colorBuffer);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, window->GetScreenWidth(), window->GetScreenHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			// create depth buffer (renderbuffer)
			glGenRenderbuffers(1, &rboDepth);
			glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, window->GetScreenWidth(), window->GetScreenHeight());
			// attach buffers
			glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
				std::cout << "Framebuffer not complete!" << std::endl;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			lightPositions.push_back(glm::vec3( 0.0f,  0.0f, 49.5f)); // back light
			lightPositions.push_back(glm::vec3(-1.4f, -1.9f, 9.0f));
			lightPositions.push_back(glm::vec3( 0.0f, -1.8f, 4.0f));
			lightPositions.push_back(glm::vec3( 0.8f, -1.7f, 6.0f));
			// colors
			lightColors.push_back(glm::vec3(200.0f, 200.0f, 200.0f));
			lightColors.push_back(glm::vec3(0.1f, 0.0f, 0.0f));
			lightColors.push_back(glm::vec3(0.0f, 0.0f, 0.2f));
			lightColors.push_back(glm::vec3(0.0f, 0.1f, 0.0f));

			int id = assetManager->LoadShader("assets/shaders/hdr");
            hdrShader = assetManager->Get<Canis::ShaderAsset>(id)->GetShader();
            
            if(!hdrShader->IsLinked())
            {
                hdrShader->Link();
            }

			id = assetManager->LoadShader("assets/shaders/lighting");
            lightingShader = assetManager->Get<Canis::ShaderAsset>(id)->GetShader();
            
            if(!lightingShader->IsLinked())
            {
                lightingShader->AddAttribute("aPos");
                lightingShader->AddAttribute("aNormal");

                lightingShader->Link();
            }

			diffuseColorPaletteTexture = assetManager->Get<Canis::TextureAsset>(assetManager->LoadTexture("assets/textures/palette/diffuse.png"))->GetPointerToTexture();
			
			specularColorPaletteTexture = assetManager->Get<Canis::TextureAsset>(assetManager->LoadTexture("assets/textures/palette/specular.png"))->GetPointerToTexture();
			
			emissionColorPaletteTexture = assetManager->Get<Canis::TextureAsset>(assetManager->LoadTexture("assets/textures/palette/emission.png"))->GetPointerToTexture();
		}
    	void Ready() {}
    	void Update(entt::registry &_registry, float _deltaTime)
		{
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_ALPHA);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDepthFunc(GL_LESS);
			//glEnable(GL_CULL_FACE);

			sortingEntities.clear();
			Canis::List::Clear(&sortingEntitiesList);

			Frustum camFrustum = CreateFrustumFromCamera(camera, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), camera->FOV, camera->nearPlane, camera->farPlane);

			auto view = _registry.view<TransformComponent, const MeshComponent, const SphereColliderComponent>();

			for (auto [entity, transform, mesh, sphere] : view.each())
			{
				if (!transform.active)
					continue;

				glm::mat4 m = Canis::GetModelMatrix(transform);

				if (!isOnFrustum(camFrustum, transform, m, sphere))
					continue;

				RenderEnttRapper rer = {};
				rer.e = entity;
				rer.distance = glm::distance(transform.position, camera->Position);

				sortingEntities.push_back(rer);
			}

			std::stable_sort(sortingEntities.begin(), sortingEntities.end(), [](const RenderEnttRapper& a, const RenderEnttRapper& b){ return (a.distance >= b.distance); });

			LightingPass(_deltaTime, _registry);
			HDRPass(_deltaTime, _registry);

			glDisable(GL_CULL_FACE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_ALPHA);
			glDisable(GL_BLEND);
		}

	private:
	};
} // end of Canis namespace