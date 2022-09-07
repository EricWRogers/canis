#pragma once
#include <glm/glm.hpp>

namespace Canis
{
struct SpriteVertex
{
    glm::vec3 position;
    glm::vec4 color;
    glm::vec2 uv;
};
}