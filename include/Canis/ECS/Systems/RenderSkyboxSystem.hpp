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
//#include "../../External/picoPNG.h"


namespace Canis
{
    class RenderSkyboxSystem
    {
    private:
        Canis::Shader skyboxShader;
        unsigned int skyboxVAO, skyboxVBO;
        unsigned int cubemapTexture;

        std::vector<float> skyboxVertices = {
            // positions
            -1.0f, 1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,

            -1.0f, -1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, -1.0f,
            -1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, -1.0f, 1.0f,
            -1.0f, -1.0f, 1.0f,

            -1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, -1.0f,
            1.0f, 1.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, 1.0f,
            -1.0f, 1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f, 1.0f,
            1.0f, -1.0f, 1.0f
        };

        // loads a cubemap texture from 6 individual texture faces
        // order:
        // +X (right)
        // -X (left)
        // +Y (top)
        // -Y (bottom)
        // +Z (front) 
        // -Z (back)
        // -------------------------------------------------------
        unsigned int loadCubemap(std::vector<std::string> faces)
        {
            stbi_set_flip_vertically_on_load(false);

            unsigned int textureID;
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

            int width, height, nrChannels;

            for (unsigned int i = 0; i < faces.size(); i++)
            {
                unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
                if (data)
                {
                    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
                    stbi_image_free(data);
                }
                else
                {
                    std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
                    stbi_image_free(data);
                }
            }

            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

            stbi_set_flip_vertically_on_load(true);

            return textureID;
        }

    public:
        Canis::Camera *camera;
        Canis::Window *window;
        std::vector<std::string> faces;

        void Init()
        {
            skyboxShader.Compile("assets/shaders/skybox.vs", "assets/shaders/skybox.fs");
            skyboxShader.AddAttribute("aPos");
            skyboxShader.Link();

            cubemapTexture = loadCubemap(faces);

            Canis::Log("s " + std::to_string(skyboxVertices.size()));

            glGenVertexArrays(1, &skyboxVAO);
            glGenBuffers(1, &skyboxVBO);
            glBindVertexArray(skyboxVAO);
            glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
            glBufferData(GL_ARRAY_BUFFER, skyboxVertices.size() * sizeof(float), &skyboxVertices[0], GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        }

        void UpdateComponents(float deltaTime, entt::registry &registry)
        {
            // draw skybox as last
            glDepthFunc(GL_LEQUAL);  // change depth function so depth test passes when values are equal to depth buffer's content
            skyboxShader.Use();
            glm::mat4 projection = glm::perspective(camera->FOV, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 1000.0f);
            glm::mat4 view = glm::mat4(glm::mat3(camera->GetViewMatrix())); // remove translation from the view matrix
            skyboxShader.SetMat4("view", view);
            skyboxShader.SetMat4("projection", projection);
            // skybox cube
            glBindVertexArray(skyboxVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glDepthFunc(GL_LESS); // set depth function back to default
            skyboxShader.UnUse();
        }
    };
} // end of Canis namespace



/*#pragma once
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
            AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetShader()->Use();
            glm::mat4 projection = glm::perspective(camera->FOV, (float)window->GetScreenWidth() / (float)window->GetScreenHeight(), 0.1f, 1000.0f);
            glm::mat4 view = glm::mat4(glm::mat3(camera->GetViewMatrix())); // remove translation from the view matrix
            AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetShader()->SetMat4("view", view);
            AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetShader()->SetMat4("projection", projection);
            // skybox cube
            glBindVertexArray(AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetVAO());
            //Canis::Log(std::to_string(AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetVAO()));
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetTexture());
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glDepthFunc(GL_LESS); // set depth function back to default
            AssetManager::GetInstance().Get<Skybox>(skyboxAssetId)->GetShader()->UnUse();
        }
    };
} // end of Canis namespace
*/