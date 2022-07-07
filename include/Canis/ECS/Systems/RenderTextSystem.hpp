#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <map>

#include "../../Shader.hpp"
#include "../../Debug.hpp"
#include "../../Camera.hpp"
#include "../../Window.hpp"
#include "../../External/entt.hpp"
#include <Canis/AssetManager.hpp>

#include "../Components/RectTransformComponent.hpp"
#include "../Components/ColorComponent.hpp"
#include "../Components/TextComponent.hpp"

namespace Canis
{
    class RenderTextSystem
    {
    public:
        Canis::Shader textShader;
        Canis::Camera *camera;
        Canis::Window *window;

        RenderTextSystem()
        {
        }

        void Init()
        {
            textShader.Compile("assets/shaders/text.vs", "assets/shaders/text.fs");
            textShader.AddAttribute("vertex");
            textShader.Link();
        }

        void RenderText(Canis::Shader &shader, std::string t, float x, float y, float scale, glm::vec3 color, int fontId)
        {
            // activate corresponding render state
            shader.Use();
            glUniform3f(glGetUniformLocation(shader.GetProgramID(), "textColor"), color.x, color.y, color.z);
            glActiveTexture(GL_TEXTURE0);
            glBindVertexArray(AssetManager::GetInstance().Get<TextAsset>(fontId)->GetVAO());

            // iterate through all characters
            std::string::const_iterator c;
            for (c = t.begin(); c != t.end(); c++)
            {
                Character ch = AssetManager::GetInstance().Get<TextAsset>(fontId)->Characters[*c];

                float xpos = x + ch.Bearing.x * scale;
                float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;

                float w = ch.Size.x * scale;
                float h = ch.Size.y * scale;
                // update VBO for each character
                float vertices[6][4] = {
                    {xpos, ypos + h, 0.0f, 0.0f},
                    {xpos, ypos, 0.0f, 1.0f},
                    {xpos + w, ypos, 1.0f, 1.0f},

                    {xpos, ypos + h, 0.0f, 0.0f},
                    {xpos + w, ypos, 1.0f, 1.0f},
                    {xpos + w, ypos + h, 1.0f, 0.0f}};
                // render glyph texture over quad
                glBindTexture(GL_TEXTURE_2D, ch.TextureID);
                // update content of VBO memory
                glBindBuffer(GL_ARRAY_BUFFER, AssetManager::GetInstance().Get<TextAsset>(fontId)->GetVBO());
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData

                glBindBuffer(GL_ARRAY_BUFFER, 0);
                // render quad
                glDrawArrays(GL_TRIANGLES, 0, 6);
                // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
                x += (ch.Advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
            }
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        void UpdateComponents(float deltaTime, entt::registry &registry)
        {
            // render text
            textShader.Use();
            glm::mat4 projection = glm::mat4(1.0f);
            projection = glm::ortho(0.0f, static_cast<float>(window->GetScreenWidth()), 0.0f, static_cast<float>(window->GetScreenHeight()));
            glUniformMatrix4fv(glGetUniformLocation(textShader.GetProgramID(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

            auto view = registry.view<const RectTransformComponent, ColorComponent, TextComponent>();

            for (auto [entity, transform, color, text] : view.each())
            {
                if (transform.active == true)
                {
                    RenderText(textShader, *text.text, transform.position.x, transform.position.y, transform.scale, color.color, text.assetId);
                }
            }

            textShader.UnUse();
        }

    private:
    };
} // end of Canis namespace