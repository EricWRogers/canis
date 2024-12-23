#include <Canis/ECS/Systems/RenderTextSystem.hpp>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <Canis/External/OpenGl.hpp>

#include <Canis/ECS/Components/RectTransform.hpp>
#include <Canis/ECS/Components/Color.hpp>
#include <Canis/ECS/Components/TextComponent.hpp>

#include <Canis/Scene.hpp>
#include <Canis/Entity.hpp>

#include <Canis/Math.hpp>

namespace Canis
{
    void RenderTextSystem::RenderText(void *_entity, Canis::Shader &shader, std::string &t, float x, float y, float scale, glm::vec4 color, int fontId, unsigned int align, glm::vec2 &_textOffset, unsigned int &_status, float _angle)
    {
        // stop crashes that happen when string empty
        if (t.size() == 0)
            return;

        if (fontId < 0)
            return;
        
        TextAsset *asset = AssetManager::Get<TextAsset>(fontId);

        shader.Use();
        shader.SetVec4("textColor", color);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, asset->GetTexture());

        glBindVertexArray(asset->GetVAO());

        std::vector<float> vertices;

        glm::vec2 pos = glm::vec2(x,y);

        bool setPivot = false;

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
                Character ch = AssetManager::Get<TextAsset>(fontId)->characters[*c];

                float xpos = x + ch.bearing.x * scale;
                float ypos = y - (ch.size.y - ch.bearing.y) * scale;

                float w = ch.size.x * scale;
                float h = ch.size.y * scale;

                if (h > bigestH)
                    bigestH = h;

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

            RectTransform& rectTransform = ((Entity*)_entity)->GetComponent<RectTransform>();
            rectTransform.size.x = deltaX;
            rectTransform.size.y = bigestH;

            if (align == Text::RIGHT)
            {
                _textOffset.x = -deltaX;
            }

            if (align == Text::CENTER) {
                _textOffset.x = -(deltaX/2);
            }
        }

        for (const char &c : t)
        {
            Character ch = asset->characters[c];

            float xpos = _textOffset.x + x + ch.bearing.x * scale;
            float ypos = _textOffset.y + y - (ch.size.y - ch.bearing.y) * scale;

            if (setPivot == false)
            {
                setPivot = true;
                pos.x = xpos;
                pos.y = ypos;
            }

            float w = ch.size.x * scale;
            float h = ch.size.y * scale;
            // update VBO for each character
            glm::vec2 bottomLeft = glm::vec2(xpos, ypos);
            glm::vec2 bottomRight = glm::vec2(xpos + w, ypos);
            glm::vec2 topLeft = glm::vec2(xpos, ypos + h);
            glm::vec2 topRight = glm::vec2(xpos + w, ypos + h);

            if (_angle != 0.0f)
            {
                RotatePointAroundPivot(topLeft, pos, _angle);
                RotatePointAroundPivot(bottomLeft, pos, _angle);
                RotatePointAroundPivot(bottomRight, pos, _angle);
                RotatePointAroundPivot(topRight, pos, _angle);
            }
        
            vertices.push_back(topLeft.x);       vertices.push_back(topLeft.y);     vertices.push_back(ch.atlasPos.x);       vertices.push_back(ch.atlasPos.y);
            vertices.push_back(bottomRight.x);    vertices.push_back(bottomRight.y);       vertices.push_back(ch.atlasPos.x + ch.atlasSize.x); vertices.push_back(ch.atlasPos.y + ch.atlasSize.y);
            vertices.push_back(bottomLeft.x);   vertices.push_back(bottomLeft.y);       vertices.push_back(ch.atlasPos.x);       vertices.push_back(ch.atlasPos.y + ch.atlasSize.y);

            vertices.push_back(topLeft.x);       vertices.push_back(topLeft.y);  vertices.push_back(ch.atlasPos.x);       vertices.push_back(ch.atlasPos.y);
            vertices.push_back(topRight.x);   vertices.push_back(topRight.y);  vertices.push_back(ch.atlasPos.x + ch.atlasSize.x); vertices.push_back(ch.atlasPos.y);
            vertices.push_back(bottomRight.x);      vertices.push_back(bottomRight.y);       vertices.push_back(ch.atlasPos.x + ch.atlasSize.x); vertices.push_back(ch.atlasPos.y + ch.atlasSize.y);

            
            // now advance cursors for next glyph (note that advance is number of 1/64 pixels)
            x += (ch.advance >> 6) * scale; // bitshift by 6 to get value in pixels (2^6 = 64 (divide amount of 1/64th pixels by 64 to get amount of pixels))
        }

        // Upload all vertex data to the GPU at once
        glBindBuffer(GL_ARRAY_BUFFER, asset->GetVBO());
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Render the entire string in one draw call
        glDrawArrays(GL_TRIANGLES, 0, vertices.size() / 4); // 4 components per vertex (x, y, texX, texY)

        // Unbind resources
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
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthFunc(GL_ALWAYS);

        // render text
        textShader.Use();
        glm::mat4 projection = glm::mat4(1.0f);
        projection = glm::ortho(0.0f, static_cast<float>(window->GetScreenWidth()), 0.0f, static_cast<float>(window->GetScreenHeight()));
        glUniformMatrix4fv(glGetUniformLocation(textShader.GetProgramID(), "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        auto view = _registry.view<RectTransform, Color, TextComponent>();
        glm::vec2 positionAnchor = glm::vec2(0.0f);
        Canis::Entity e;
        e.scene = scene;

        for (auto [entity, transform, color, text] : view.each())
        {
            if (text.assetId < 0)
                continue;

            if (transform.active == true)
            {
                e.entityHandle = entity;
                positionAnchor = GetAnchor((Canis::RectAnchor)transform.anchor,
                                           (float)window->GetScreenWidth(),
                                           (float)window->GetScreenHeight());

                RenderText(&e,
                           textShader,
                           text.text,
                           transform.position.x + positionAnchor.x,
                           transform.position.y + positionAnchor.y,
                           transform.scale,
                           color.color,
                           text.assetId,
                           text.alignment,
                           transform.originOffset,
                           text._status,
                           transform.rotation);
            }
        }

        textShader.UnUse();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
    }
} // end of Canis namespace