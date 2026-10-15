#pragma once
namespace Canis
{
// Shared by lit material shaders. Baked lights never enter the runtime arrays.
inline constexpr const char* DirectLightingGLSL = R"GLSL(
uniform bool canisDirectCapture;
uniform int directionalLightMode;
uniform int pointLightModes[8];
uniform vec3 pointLightDirections[8];
uniform vec2 pointLightCones[8];
bool CanisDirectLightVisible(int mode, bool bakedSurface)
{
    if (mode == 2 || (mode == 0 && canisDirectCapture)) return false;
    return mode == 0 || !bakedSurface;
}
float CanisLightAttenuation(int index, vec3 toLight, bool bakedSurface)
{
    if (!CanisDirectLightVisible(pointLightModes[index], bakedSurface)) return 0.0;
    // A negative outer cosine identifies an omnidirectional point light.
    vec2 cone = pointLightCones[index];
    if (cone.y < 0.0) return 1.0;
    float angle = dot(pointLightDirections[index], -normalize(toLight));
    return smoothstep(cone.y, max(cone.y + 0.0001, cone.x), angle);
}
)GLSL";
}
