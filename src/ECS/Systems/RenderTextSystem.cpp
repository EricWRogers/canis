#include <Canis/ECS/Systems/RenderTextSystem.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GL/glew.h>

#include <Canis/ECS/Components/RectTransformComponent.hpp>
#include <Canis/ECS/Components/ColorComponent.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>

#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Canis
{
    void RenderTextSystem::RenderText(void *_entity, Canis::Shader &shader, std::string &t, float x, float y, float scale, glm::vec3 color, int fontId, unsigned int align, glm::vec2 &_textOffset, unsigned int &_status)
    {
        // activate corresponding render state
        shader.Use();
        glUniform3f(glGetUniformLocation(shader.GetProgramID(), "textColor"), color.x, color.y, color.z);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(assetManager->Get<TextAsset>(fontId)->GetVAO());

        if ((_status & BIT::ONE) > 0) { // check if we need to recalculate the size &| alignment
            
            float left  =  FLT_MAX;
            float right = -FLT_MAX;
            float bigestH = -FLT_MAX;
            float xBackUp = x;
            std::string::const_iterator c;
            c = t.end();
            c--;
            for (; true; c--)
            {
                Character ch = assetManager->Get<TextAsset>(fontId)->Characters[*c];

                float xpos = x + ch.bearing.x * scale;
                float ypos = y - (ch.size.y - ch.bearing.y) * scale;

                float w = ch.size.x * scale;
                float h = ch.size.y * scale;

                if (h > bigestH)
                    bigestH = h;
                
                // update VBO for each character
                float vertices[6][4] = {
                    {xpos - w, ypos + h, 0.0f, 0.0f},
                    {xpos - w, ypos, 0.0f, 1.0f},
                    {xpos, ypos, 1.0f, 1.0f},
                    {xpos - w, ypos + h, 0.0f, 0.0f},
                    {xpos, ypos, 1.0f, 1.0f},
                    {xpos, ypos + h, 1.0f, 0.0f}};
                
                if ((xpos - w) < left)
                    left = xpos - w;
                if ((xpos) > right)
                    right = xpos;
                
                x -= (ch.advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))

                if (c == t.begin())
                    break;
            }

            // set _textOffset
            float deltaX = right - left;
            x = xBackUp;

            RectTransformComponent& rectTransform = ((Entity*)_entity)->GetComponent<RectTransformComponent>();
            rectTransform.size.x = deltaX;
            rectTransform.size.y = bigestH;

            if (align == Text::TextAlignment::RIGHT)
            {
                _textOffset.x = -deltaX;
            }

            if (align == Text::TextAlignment::CENTER) {
                _textOffset.x = -(deltaX/2);
            }
        }

        // draw text
        std::string::const_iterator c;
        for (c = t.begin(); c != t.end(); c++)
        {
            Character ch = assetManager->Get<TextAsset>(fontId)->Characters[*c];

            float xpos = _textOffset.x + x + ch.bearing.x * scale;
            float ypos = _textOffset.y + y - (ch.size.y - ch.bearing.y) * scale;

            float w = ch.size.x * scale;
            float h = ch.size.y * scale;
            // update VBO for each character
            float vertices[6][4] = {
                {xpos, ypos + h, 0.0f, 0.0f},
                {xpos, ypos, 0.0f, 1.0f},
                {xpos + w, ypos, 1.0f, 1.0f},

                {xpos, ypos + h, 0.0f, 0.0f},
                {xpos + w, ypos, 1.0f, 1.0f},
                {xpos + w, ypos + h, 1.0f, 0.0f}};
            // render glyph texture over quad
            glBindTexture(GL_TEXTURE_2D, ch.textureID);
            // update content of VBO memory
            glBindBuffer(GL_ARRAY_BUFFER, assetManager->Get<TextAsset>(fontId)->GetVBO());
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); // be sure to use glBufferSubData and not glBufferData

            glBindBuffer(GL_ARRAY_BUFFER, 0);
            // render quad
            glDrawArrays(GL_TRIANGLES, 0, 6);
            // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
            x += (ch.advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
        }
    
        // iterate through all characters
        /**/
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void RenderTextSystem::Create()
    {
        textShader.Compile("assets/shaders/text.vs", "assets/shaders/text.fs");
        textShader.AddAttribute("vertex");
        textShader.Link();
    }

    void RenderTextSystem::Update(entt::registry &_registry, float _deltaTime)
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_ALPHA);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_ALWAYS);

        // render text
        textShader.Use();
        glm::mat4 projection = glm::mat4(1.0f);
        projection = glm::ortho(0.0f, static_cast<float>(window->GetScreenWidth()), 0.0f, static_cast<float>(window->GetScreenHeight()));
        glUniformMatrix4fv(glGetUniformLocation(textShader.GetProgramID(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        auto view = _registry.view<RectTransformComponent, ColorComponent, TextComponent>();
        glm::vec2 positionAnchor = glm::vec2(0.0f);
        Canis::Entity e;
        e.scene = scene;

        for (auto [entity, transform, color, text] : view.each())
        {
            if (transform.active == true)
            {
                e.entityHandle = entity;
                positionAnchor = GetAnchor((Canis::RectAnchor)transform.anchor,
                                           (float)window->GetScreenWidth(),
                                           (float)window->GetScreenHeight());
                
                RenderText(&e,
                            textShader,
                            *text.text,
                            transform.position.x + positionAnchor.x,
                            transform.position.y + positionAnchor.y,
                            transform.scale,
                            color.color,
                            text.assetId,
                            text.alignment,
                            transform.originOffset,
                            text._status);
            }
        }

        textShader.UnUse();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_ALPHA);
        glDisable(GL_BLEND);
    }

    bool DecodeRenderTextSystem(const std::string &_name, Canis::Scene *_scene)
    {
        if (_name == "Canis::RenderTextSystem")
        {
            _scene->CreateRenderSystem<RenderTextSystem>();
            return true;
        }
        return false;
    }
} // end of Canis namespace