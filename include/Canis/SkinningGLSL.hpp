#pragma once

namespace Canis {
inline constexpr unsigned int SkinningBufferBinding = 4;
inline constexpr unsigned int MaxSkinningJoints = 128;

// GL 3.3 / ES 3.0: bind geometry stays resident; each pose uploads bone
// transforms.
inline constexpr const char *SkinningGLSL = R"GLSL(
layout(location = 8) in vec4 canisJointIndices;
layout(location = 9) in vec4 canisJointWeights;
layout(std140) uniform CanisSkinPalette { mat4 canisJointMatrices[128]; };
uniform bool canisUseSkinning;
uniform int canisSkinJointCount;
void CanisSkin(inout vec3 position, inout vec3 normal) {
    if (!canisUseSkinning) return;
    float total = dot(canisJointWeights, vec4(1.0));
    if (total <= 0.000001) return;
    mat4 skin = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        int joint = int(canisJointIndices[i]);
        float weight = canisJointWeights[i] / total;
        if (joint >= 0 && joint < canisSkinJointCount && weight > 0.0)
            skin += canisJointMatrices[joint] * weight;
    }
    position = (skin * vec4(position, 1.0)).xyz;
    vec3 transformedNormal = mat3(skin) * normal;
    float lengthSquared = dot(transformedNormal, transformedNormal);
    normal = lengthSquared > 0.0 ? transformedNormal * inversesqrt(lengthSquared) : vec3(0, 1, 0);
}
)GLSL";
} // namespace Canis
