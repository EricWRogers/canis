#pragma once
#include <string>

#if __ANDROID__
#define GLES
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#elif __EMSCRIPTEN__
#define GLES
#include <emscripten.h>
#include <GLES3/gl3.h>
#else
#define GLAD
#include <glad/glad.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

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

        
    public:
        Canis::Camera *camera;
        Canis::Window *window;
        std::vector<std::string> faces;

        ~RenderSkyboxSystem()
        {
            glDeleteTextures(1, &cubemapTexture);
            glDeleteVertexArrays(1, &skyboxVAO);
            glDeleteBuffers(1, &skyboxVBO);
            Canis::Log("Delete Texture");
        }

        void Init()
        {
            skyboxShader.Compile("assets/shaders/skybox.vs", "assets/shaders/skybox.fs");
            skyboxShader.AddAttribute("aPos");
            skyboxShader.Link();

            cubemapTexture = Canis::LoadImageToCubemap(faces, GL_RGBA);

            Canis::Log("s " + std::to_string(skyboxVertices.size()));

            #ifdef __ANDROID__
            genVertexArraysOES(1,&skyboxVAO);
            #else
            glGenVertexArrays(1, &skyboxVAO);
            #endif
            glGenBuffers(1, &skyboxVBO);

            #ifdef __ANDROID__
            bindVertexArrayOES(skyboxVAO);
            #else
            glBindVertexArray(skyboxVAO);
            #endif
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
            #ifdef __ANDROID__
            bindVertexArrayOES(skyboxVAO);
            #else
            glBindVertexArray(skyboxVAO);
            #endif
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            #ifdef __ANDROID__
            bindVertexArrayOES(0);
            #else
            glBindVertexArray(0);
            #endif
            glDepthFunc(GL_LESS); // set depth function back to default
            skyboxShader.UnUse();
        }
    };
} // end of Canis namespace