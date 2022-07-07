#pragma once
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <stb_image.h>

#include "../../Shader.hpp"
#include "../../Camera.hpp"
#include "../../Window.hpp"
#include "../../IOManager.hpp"
#include "../../External/entt.hpp"
#include <Canis/Asset.hpp>
#include <Canis/AssetManager.hpp>


namespace Canis
{
    class RenderSkyboxSystem
    {
    private:
        int skyboxAssetId = 0;
    public:
        Canis::Camera *camera;
        Canis::Window *window;

        void Init()
        {
            skyboxAssetId = AssetManager::GetInstance().LoadSkybox("assets/textures/space-nebulas-skybox/");
        }

        void UpdateComponents(float deltaTime, entt::registry &registry)
        {
            // draw skybox as last
            glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
            AssetManager::GetInstance().Get<SkyboxAsset>(skyboxAssetId)->GetShader()->Use();
            glm::mat4 projection = glm::perspective(camera->FOV, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 1000.0f);
            glm::mat4 view = glm::mat4(glm::mat3(camera->GetViewMatrix())); // remove translation from the view matrix
            AssetManager::GetInstance().Get<SkyboxAsset>(skyboxAssetId)->GetShader()->SetMat4("view", view);
            AssetManager::GetInstance().Get<SkyboxAsset>(skyboxAssetId)->GetShader()->SetMat4("projection", projection);
            // skybox cube
            glBindVertexArray(AssetManager::GetInstance().Get<SkyboxAsset>(skyboxAssetId)->GetVAO());
            //Canis::Log(std::to_string(AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetVAO()));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, AssetManager::GetInstance().Get<SkyboxAsset>(skyboxAssetId)->GetTexture());
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glDepthFunc(GL_LESS); // set depth function back to default
            AssetManager::GetInstance().Get<SkyboxAsset>(skyboxAssetId)->GetShader()->UnUse();
        }
    };
} // end of Canis namespace