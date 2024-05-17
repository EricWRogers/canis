#pragma once
#include <Canis/ECS/Systems/System.hpp>
#include <glm/glm.hpp>
#include <Canis/Shader.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace Canis
{
    class Shader;

    class RenderTextSystem : public System
    {
    private:
    public:
        Canis::Shader textShader;

        RenderTextSystem() : System() { m_name = type_name<RenderTextSystem>(); }

        void RenderText(void *_entity, Canis::Shader &shader, std::string &t, float x, float y, float scale, glm::vec4 color, int fontId, unsigned int align, glm::vec2 &_textOffset, unsigned int &_status, float _angle);

        void Create();

        void Ready() {}

        void Update(entt::registry &_registry, float _deltaTime);

    private:
    };
} // end of Canis namespace


/*
int i = 0;
                float lastX = 0.0f;
                std::string::const_iterator c;
                c = t.end();
                c--;
                float totalXMovement = 0.0f;
                for ( ; true; c--)
                {
                    Character ch = assetManager->Get<TextAsset>(fontId)->Characters[*c];

                    float xpos = x + ch.bearing.x * scale;
                    float ypos = y - (ch.size.y - ch.bearing.y) * scale;

                    float w = ch.size.x * scale;
                    float h = ch.size.y * scale;
                    // update VBO for each character
                    float vertices[6][4] = {
                        {totalXMovement + xpos - w, ypos + h, 0.0f, 0.0f},
                        {totalXMovement + xpos - w, ypos, 0.0f, 1.0f},
                        {totalXMovement + xpos, ypos, 1.0f, 1.0f},
                        {totalXMovement + xpos - w, ypos + h, 0.0f, 0.0f},
                        {totalXMovement + xpos, ypos, 1.0f, 1.0f},
                        {totalXMovement + xpos, ypos + h, 1.0f, 0.0f}};
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

                    totalXMovement = lastX - xpos;
                    lastX = xpos;
                    i++;
                    if (c == t.begin())
                        break;
                }
*/