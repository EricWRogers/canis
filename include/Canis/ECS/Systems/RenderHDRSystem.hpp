#pragma once

#include <glm/glm.hpp>

#include <Canis/ECS/Systems/RenderMeshSystem.hpp>
#include <Canis/ECS/Systems/RenderSkyboxSystem.hpp>

namespace Canis
{
    class RenderHDRSystem
    {
    private:
        unsigned int hdrFBO;
        unsigned int colorBuffer;
        unsigned int quadVAO = 0;
        unsigned int quadVBO;
        Shader hdrShader;
    public:
        RenderMeshSystem *renderMeshSystem;
        RenderSkyboxSystem *renderSkyboxSystem;
        Canis::Window *window;
        float exposure = 1.0f;

        RenderHDRSystem(Canis::Window *w) {
            window = w;
            glGenFramebuffers(1, &hdrFBO);
            // create floating point color buffer
            glGenTextures(1, &colorBuffer);
            glBindTexture(GL_TEXTURE_2D, colorBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, (float)window->GetScreenWidth(), (float)window->GetScreenHeight(), 0, GL_RGBA, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            // create depth buffer (renderbuffer)
            unsigned int rboDepth;
            glGenRenderbuffers(1, &rboDepth);
            glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, (float)window->GetScreenWidth(), (float)window->GetScreenHeight());
            
            // attach buffers
            glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                std::cout << "Framebuffer not complete!" << std::endl;
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            hdrShader.Compile(
                "assets/shaders/hdr.vs",
                "assets/shaders/hdr.fs"
            );
            hdrShader.AddAttribute("aPos");
            hdrShader.AddAttribute("aTexCoords");
            hdrShader.Link();
        }

        ~RenderHDRSystem() {

        }

        void UpdateComponents(float deltaTime, entt::registry &registry) {
            

            glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            renderSkyboxSystem->UpdateComponents(deltaTime, registry);
            renderMeshSystem->UpdateComponents(deltaTime, registry);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            hdrShader.Use();
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, colorBuffer);
            hdrShader.SetFloat("exposure", exposure);
            
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
            hdrShader.UnUse();
        }
    };
} // end of Canis namespace